#include "termchart/plugin/dynamic.hpp"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace termchart::plugin {
namespace {

// 统一的"打开"原语：平台差异被压缩到这一层，其余代码只认 void* 句柄。
void* platform_open(const std::string& path) noexcept {
#if defined(_WIN32)
    return reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* platform_symbol(void* handle, const char* name) noexcept {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

void platform_close(void* handle) noexcept {
#if defined(_WIN32)
    static_cast<void>(FreeLibrary(static_cast<HMODULE>(handle)));
#else
    static_cast<void>(dlclose(handle));
#endif
}

using EntryFn = const Manifest* (*)();

EntryFn entry_of(const DynamicLibrary& library) noexcept {
    const void* sym = library.symbol(DynamicLibrary::kEntrySymbol);
    if (sym == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<EntryFn>(sym);
}

}  // namespace

DynamicLibrary::~DynamicLibrary() {
    close();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

Result<DynamicLibrary> DynamicLibrary::open(std::string_view path) noexcept {
    if (path.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    // 平台 API 都要零结尾字符串。
    std::string owned(path);
    void* handle = platform_open(owned);
    if (handle == nullptr) {
        return Unexpected<ErrorCode>(ErrorCode::PlatformError);
    }
    return DynamicLibrary(handle);
}

void* DynamicLibrary::symbol(const char* name) const noexcept {
    if (handle_ == nullptr || name == nullptr) {
        return nullptr;
    }
    return platform_symbol(handle_, name);
}

void DynamicLibrary::close() noexcept {
    if (handle_ != nullptr) {
        platform_close(handle_);
        handle_ = nullptr;
    }
}

Result<Manifest> read_manifest(const DynamicLibrary& library) noexcept {
    const EntryFn entry = entry_of(library);
    if (entry == nullptr) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
    const Manifest* manifest = entry();
    if (manifest == nullptr || manifest->name.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    return *manifest;
}

}  // namespace termchart::plugin
