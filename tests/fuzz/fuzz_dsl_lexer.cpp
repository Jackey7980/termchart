// fuzz_dsl_lexer.cpp —— DSL 词法层的健壮性（规格 fuzz 目标之一）
//
// 关注点：词法器是**所有方言的地基**，它的两条不变量必须被反复验证：
//   * 每次 next() 至少消耗 1 字节（否则畸形 UTF-8 / 孤立字符会让调用方死循环）；
//   * 非法字节产出 Invalid，而不是被静默吞掉或当成标识符。
// 因此这里对同一份输入跑多套关键字/注释前缀组合，并在每一步断言游标严格前进。
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "fuzz_common.hpp"
#include "termchart/dsl/lexer.hpp"

namespace {

using namespace termchart::dsl;

constexpr std::string_view kMermaidKeywords[] = {"flowchart", "graph", "subgraph", "end"};
constexpr std::string_view kStateKeywords[] = {"stateDiagram", "state", "note", "end", "of"};

void lex_once(std::string_view input, KeywordSet keywords,
              const LexerOptions& options) {
    Lexer lexer(input, keywords, options);
    std::size_t previous_offset = 0;
    std::size_t guard = 0;
    while (true) {
        const Token token = lexer.next();
        if (token.kind == TokenKind::End) {
            break;
        }
        // 不变量：游标严格前进（除空白被跳过的情况，offset 至少不减）。
        if (token.offset < previous_offset) {
            std::fprintf(stderr, "fuzz: 词法器游标回退（offset %zu < %zu）\n", token.offset,
                         previous_offset);
            std::abort();
        }
        previous_offset = token.offset;
        if (++guard > options.max_tokens + 8) {
            std::fprintf(stderr, "fuzz: 词法器不收敛\n");
            std::abort();
        }
    }
}

void target(const std::uint8_t* data, std::size_t size) {
    const auto input = std::string_view(reinterpret_cast<const char*>(data), size);

    lex_once(input, KeywordSet(std::span<const std::string_view>(kMermaidKeywords)), {});
    lex_once(input, KeywordSet(std::span<const std::string_view>(kStateKeywords)), {});

    LexerOptions comment_heavy;
    comment_heavy.keep_comments = true;
    comment_heavy.comment_prefixes = {"%%", "//", "'"};
    lex_once(input, KeywordSet{}, comment_heavy);

    LexerOptions no_newlines;
    no_newlines.keep_newlines = false;
    lex_once(input, KeywordSet{}, no_newlines);

    // 一次性取完（含 max_tokens 上限路径）
    (void)tokenize(input, KeywordSet{});
    (void)tokenize(input, KeywordSet(std::span<const std::string_view>(kMermaidKeywords)),
                   LexerOptions{});
}

}  // namespace

TERMCHART_FUZZ_MAIN(target)
