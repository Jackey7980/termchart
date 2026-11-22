// test_util_file.cpp —— util::fopen_utf8 回归
//
// 动机：Python 侧 save_png/save_svg 传的是 UTF-8 路径，Windows 上 std::fopen 按
// ANSI 代码页解释，用户名含中文（C:\Users\琳琳\...）时直接打不开文件。这里锁住
// 「UTF-8 路径能正常读写」这条契约。
#include "termchart/util/file.hpp"

#include <doctest.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

// path::u8string() 给的是 char8_t 序列，C++20 下不能隐式转 std::string。
std::string to_utf8(const std::filesystem::path& p) {
    const std::u8string s = p.u8string();
    std::string out;
    out.reserve(s.size());
    for (char8_t c : s) {
        out.push_back(static_cast<char>(c));
    }
    return out;
}

}  // namespace

TEST_CASE("util::fopen_utf8：含非 ASCII 的路径可写可读") {
    const std::filesystem::path dir = std::filesystem::temp_directory_path();
    const std::filesystem::path file = dir / "termchart_probe_中文 路径.txt";
    const std::string path = to_utf8(file);

    const std::string payload = "hello utf8 path\n";
    std::FILE* w = termchart::util::fopen_utf8(path, "wb");
    REQUIRE(w != nullptr);
    CHECK(std::fwrite(payload.data(), 1, payload.size(), w) == payload.size());
    std::fclose(w);

    std::FILE* r = termchart::util::fopen_utf8(path, "rb");
    REQUIRE(r != nullptr);
    char buf[64] = {};
    const std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, r);
    std::fclose(r);
    CHECK(std::string(buf, n) == payload);

    std::error_code ec;
    std::filesystem::remove(file, ec);
}

TEST_CASE("util::fopen_utf8：非法参数与不存在的目录返回 nullptr") {
    CHECK(termchart::util::fopen_utf8("", "rb") == nullptr);
    CHECK(termchart::util::fopen_utf8("termchart_no_such_dir_xyz/a.txt", "rb") == nullptr);
}
