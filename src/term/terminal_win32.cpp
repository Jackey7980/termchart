// terminal_win32.cpp —— Windows 控制台平台实现
//
// 隔离层：本文件允许出现 #ifdef 与 <windows.h>；其它任何文件不得包含 <windows.h>。
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdio>

#include "src/term/platform_internal.hpp"

#if defined(_WIN32)

namespace termchart::term::platform {
namespace {

HANDLE stdout_handle() noexcept { return GetStdHandle(STD_OUTPUT_HANDLE); }
HANDLE stdin_handle() noexcept { return GetStdHandle(STD_INPUT_HANDLE); }

}  // namespace

SizeResult query_terminal_size() noexcept {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(stdout_handle(), &info) == 0) {
        return SizeResult{};
    }
    const int cols = info.srWindow.Right - info.srWindow.Left + 1;
    const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    if (cols <= 0 || rows <= 0) {
        return SizeResult{};
    }
    SizeResult out;
    out.ok = true;
    out.cols = static_cast<std::uint16_t>(cols);
    out.rows = static_cast<std::uint16_t>(rows);
    return out;
}

bool enable_raw_mode() noexcept {
    const HANDLE handle = stdin_handle();
    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode) == 0) {
        return false;
    }
    const DWORD wanted =
        mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    return SetConsoleMode(handle, wanted) != 0;
}

bool disable_raw_mode() noexcept {
    const HANDLE handle = stdin_handle();
    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode) == 0) {
        return false;
    }
    const DWORD wanted = mode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    return SetConsoleMode(handle, wanted) != 0;
}

bool stdout_is_tty() noexcept {
    DWORD mode = 0;
    return GetConsoleMode(stdout_handle(), &mode) != 0;
}

bool stdin_is_tty() noexcept {
    DWORD mode = 0;
    return GetConsoleMode(stdin_handle(), &mode) != 0;
}

// ── VT 处理 ─────────────────────────────────────────────────────────────────
// 进入前保存原 mode 并开启 ENABLE_VIRTUAL_TERMINAL_PROCESSING；恢复时还原原 mode
// （原本就开着 VT 的终端——如 Windows Terminal / VS Code 宿主——保持开启）。
namespace {

DWORD g_vt_original_mode = 0;
bool g_vt_saved = false;

}  // namespace

bool enable_vt() noexcept {
    const HANDLE handle = stdout_handle();
    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode) == 0) {
        return false;
    }
    if (!g_vt_saved) {
        g_vt_original_mode = mode;
        g_vt_saved = true;
    }
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        return true;
    }
    return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

bool disable_vt() noexcept {
    if (!g_vt_saved) {
        return true;
    }
    g_vt_saved = false;
    return SetConsoleMode(stdout_handle(), g_vt_original_mode) != 0;
}

}  // namespace termchart::term::platform

#endif  // defined(_WIN32)
