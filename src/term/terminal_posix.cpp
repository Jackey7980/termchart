// terminal_posix.cpp —— POSIX 终端平台实现
//
// 隔离层：本文件允许出现 #ifdef 与 <termios.h>；其它任何文件不得包含它。
#if !defined(_WIN32)

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <new>

#include "src/term/platform_internal.hpp"

namespace termchart::term::platform {
namespace {

termios g_original_mode{};
bool g_original_saved = false;

}  // namespace

SizeResult query_terminal_size() noexcept {
    winsize size{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0) {
        return SizeResult{};
    }
    if (size.ws_col == 0 || size.ws_row == 0) {
        return SizeResult{};
    }
    SizeResult out;
    out.ok = true;
    out.cols = static_cast<std::uint16_t>(size.ws_col);
    out.rows = static_cast<std::uint16_t>(size.ws_row);
    return out;
}

bool enable_raw_mode() noexcept {
    if (!isatty(STDIN_FILENO)) {
        return false;
    }
    if (!g_original_saved) {
        if (tcgetattr(STDIN_FILENO, &g_original_mode) != 0) {
            return false;
        }
        g_original_saved = true;
    }
    termios raw = g_original_mode;
    raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | ISIG | IEXTEN));
    raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL | BRKINT | INPCK | ISTRIP));
    raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
}

bool disable_raw_mode() noexcept {
    if (!g_original_saved) {
        return false;
    }
    const bool ok = tcsetattr(STDIN_FILENO, TCSANOW, &g_original_mode) == 0;
    g_original_saved = false;
    return ok;
}

bool stdout_is_tty() noexcept { return isatty(fileno(stdout)) != 0; }

bool stdin_is_tty() noexcept { return isatty(fileno(stdin)) != 0; }

// POSIX 终端原生解释 ANSI 转义序列，无需开关。
bool enable_vt() noexcept { return true; }
bool disable_vt() noexcept { return true; }

// Waker 的 POSIX 实现（self-pipe）。Windows 版本见 waker_win32.cpp。
namespace {

struct PipeWaker {
    int read_fd = -1;
    int write_fd = -1;
};

}  // namespace

void* create_waker() noexcept {
    int fds[2] = {-1, -1};
    if (pipe(fds) != 0) {
        return nullptr;
    }
    // 读写端都设非阻塞：信号处理里 notify 不能卡住。
    for (int fd : fds) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }
    auto* waker = new (std::nothrow) PipeWaker{fds[0], fds[1]};
    if (waker == nullptr) {
        close(fds[0]);
        close(fds[1]);
        return nullptr;
    }
    return waker;
}

void destroy_waker(void* handle) noexcept {
    auto* waker = static_cast<PipeWaker*>(handle);
    if (waker == nullptr) {
        return;
    }
    if (waker->read_fd >= 0) {
        close(waker->read_fd);
    }
    if (waker->write_fd >= 0) {
        close(waker->write_fd);
    }
    delete waker;
}

void notify_waker(void* handle) noexcept {
    auto* waker = static_cast<PipeWaker*>(handle);
    if (waker == nullptr || waker->write_fd < 0) {
        return;
    }
    const char byte = 1;
    ssize_t ignored = write(waker->write_fd, &byte, 1);
    (void)ignored;
}

bool wait_waker(void* handle, int timeout_ms) noexcept {
    auto* waker = static_cast<PipeWaker*>(handle);
    if (waker == nullptr || waker->read_fd < 0) {
        return false;
    }
    pollfd entry{};
    entry.fd = waker->read_fd;
    entry.events = POLLIN;
    const int wait = timeout_ms < 0 ? -1 : timeout_ms;
    const int ready = poll(&entry, 1, wait);
    return ready > 0 && (entry.revents & POLLIN) != 0;
}

void drain_waker(void* handle) noexcept {
    auto* waker = static_cast<PipeWaker*>(handle);
    if (waker == nullptr || waker->read_fd < 0) {
        return;
    }
    char buffer[64];
    while (true) {
        const ssize_t n = read(waker->read_fd, buffer, sizeof(buffer));
        if (n <= 0) {
            break;
        }
        if (static_cast<size_t>(n) < sizeof(buffer)) {
            break;
        }
    }
}

std::uintptr_t waker_native_handle(void* handle) noexcept {
    auto* waker = static_cast<PipeWaker*>(handle);
    return waker == nullptr ? 0 : static_cast<std::uintptr_t>(waker->read_fd);
}

}  // namespace termchart::term::platform

#endif  // !defined(_WIN32)
