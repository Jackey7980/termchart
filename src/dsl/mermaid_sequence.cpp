#include "termchart/dsl/mermaid_sequence.hpp"

#include <string>
#include <vector>

namespace termchart::dsl {
namespace {

constexpr std::string_view kSequenceKeywords[] = {
    "sequencediagram", "participant", "actor", "database", "queue",
    "activate",        "deactivate",  "note",    "left",     "right",
    "over",            "loop",        "alt",     "else",     "opt",
    "par",             "critical",    "break",   "end",      "title",
    "autonumber",      "box",         "link",    "links",    "of",
    "and",             "as",          "rect",    "create",   "destroy",
};

KeywordSet sequence_keywords() noexcept {
    return KeywordSet(std::span<const std::string_view>(kSequenceKeywords));
}

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

MessageKind message_kind_of(std::string_view arrow, bool& open_or_cross) noexcept {
    open_or_cross = false;
    if (arrow == "->") {
        return MessageKind::Solid;
    }
    if (arrow == "-->") {
        return MessageKind::Dotted;
    }
    if (arrow == "->>") {
        return MessageKind::Async;
    }
    if (arrow == "-->>") {
        return MessageKind::AsyncDotted;
    }
    if (arrow == "-)") {
        open_or_cross = true;
        return MessageKind::Open;
    }
    if (arrow == "--)") {
        open_or_cross = true;
        return MessageKind::OpenDotted;
    }
    if (arrow == "-x") {
        open_or_cross = true;
        return MessageKind::Cross;
    }
    if (arrow == "--x") {
        open_or_cross = true;
        return MessageKind::CrossDotted;
    }
    return MessageKind::Solid;
}

bool is_block_keyword(std::string_view text) noexcept {
    return text == "loop" || text == "alt" || text == "else" || text == "opt" ||
           text == "par" || text == "critical" || text == "break" || text == "and" ||
           text == "rect" || text == "end";
}

class SequenceParser : public ParserBase {
public:
    SequenceParser(std::vector<Token> tokens, std::string_view source,
                   const SequenceParseOptions& options)
        : ParserBase(std::move(tokens), "mermaid-sequence", source), options_(options) {}

    Result<AstSequence> run() {
        parse_header();
        parse_statements();
        if (failed()) {
            return Unexpected<ErrorCode>(diagnostic().code);
        }
        return ast_;
    }

private:
    void parse_header() {
        skip_newlines();
        if (!check_keyword("sequencediagram")) {
            report(ErrorCode::ParseError, "缺少 sequenceDiagram 头部");
            return;
        }
        ast_.directive = advance().text;
    }

    void parse_statements() {
        while (!at_end()) {
            skip_newlines();
            if (at_end()) {
                break;
            }
            if (match_punct(";")) {
                continue;
            }
            if (check_keyword("participant") || check_keyword("actor") ||
                check_keyword("database") || check_keyword("queue")) {
                parse_participant();
                if (failed()) {
                    return;
                }
                continue;
            }
            if (check_keyword("activate") || check_keyword("deactivate")) {
                parse_activation();
                continue;
            }
            if (check_keyword("note")) {
                parse_note();
                continue;
            }
            if (check_keyword("title")) {
                (void)advance();
                ast_.title = take_rest_of_line();
                continue;
            }
            if (check_keyword("autonumber")) {
                ast_.attributes.push_back(AstAttribute{"autonumber", "true"});
                skip_to_end_of_line();
                continue;
            }
            if (check_keyword("create") || check_keyword("destroy") || check_keyword("box") ||
                check_keyword("link") || check_keyword("links")) {
                // 认出但不做：记一笔，便于将来判断"输入里到底有什么"。
                ast_.attributes.push_back(AstAttribute{"ignored_directive", current().text});
                skip_to_end_of_line();
                continue;
            }
            if (current().kind == TokenKind::Keyword && is_block_keyword(current().text)) {
                ast_.attributes.push_back(AstAttribute{"ignored_block", current().text});
                skip_to_end_of_line();
                continue;
            }
            if (!parse_message()) {
                if (failed()) {
                    return;
                }
                skip_to_end_of_line();
            }
        }
    }

    void parse_participant() {
        const std::string kind = advance().text;
        if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text))) {
            report(ErrorCode::ParseError, "participant 缺少女名");
            skip_to_end_of_line();
            return;
        }
        const std::string key = advance().text;
        std::string label = key;
        if (check_keyword("as")) {
            (void)advance();
            if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text)) {
                label = advance().text;
            }
        }
        SequenceParticipant& participant = ensure_participant(key);
        participant.label = label;
        participant.kind = kind;
        skip_to_end_of_line();
    }

    void parse_activation() {
        const bool activating = check_keyword("activate");
        (void)advance();
        if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text))) {
            report(ErrorCode::ParseError, "activate/deactivate 缺少女名");
            skip_to_end_of_line();
            return;
        }
        const std::string key = advance().text;
        (void)ensure_participant(key);
        // 激活作用在**上一条消息**上；没有上一条消息时记成图的元信息而不是报错
        // （真实语料里 `activate` 也可能出现在第一条消息之前）。
        if (ast_.messages.empty()) {
            ast_.attributes.push_back(
                AstAttribute{activating ? "pending_activate" : "pending_deactivate", key});
        } else if (activating) {
            ast_.messages.back().activate_target = true;
        } else {
            ast_.messages.back().deactivate_source = true;
        }
        skip_to_end_of_line();
    }

    void parse_note() {
        (void)advance();  // note
        SequenceNote note;
        if (check_keyword("left") || check_keyword("right") || check_keyword("over")) {
            const std::string placement = advance().text;
            if (placement == "left") {
                note.placement = SequenceNote::Placement::Left;
            } else if (placement == "right") {
                note.placement = SequenceNote::Placement::Right;
            } else {
                note.placement = SequenceNote::Placement::Over;
            }
        } else {
            report(ErrorCode::ParseError, "note 缺少 left/right/over");
            skip_to_end_of_line();
            return;
        }
        if (check_keyword("of")) {
            (void)advance();
        }
        while (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text)) {
            const std::string key = advance().text;
            note.participants.push_back(key);
            (void)ensure_participant(key);
            if (!match_punct(",")) {
                break;
            }
        }
        if (note.participants.empty()) {
            report(ErrorCode::ParseError, "note 缺少女名");
            skip_to_end_of_line();
            return;
        }
        if (match_punct(":")) {
            note.text = take_rest_of_line();
        } else {
            skip_to_end_of_line();
        }
        ast_.notes.push_back(std::move(note));
    }

    // `A->>B: 文本`，含 `+`/`-` 激活简写
    bool parse_message() {
        if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
              check_kind(TokenKind::Number))) {
            report(ErrorCode::ParseError, "期望消息发送者");
            return false;
        }
        const std::string from = advance().text;
        if (!check_kind(TokenKind::Arrow)) {
            report(ErrorCode::ParseError, "消息缺少箭头");
            return false;
        }
        bool open_or_cross = false;
        const std::string arrow = advance().text;
        const MessageKind kind = message_kind_of(arrow, open_or_cross);

        bool activate_target = false;
        bool deactivate_source = false;
        if (match_punct("+")) {
            activate_target = true;
        } else if (match_punct("-")) {
            deactivate_source = true;
        }
        if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
              check_kind(TokenKind::Number))) {
            report(ErrorCode::ParseError, "消息缺少接收者");
            return false;
        }
        const std::string to = advance().text;

        std::string text;
        if (match_punct(":")) {
            text = take_rest_of_line();
        } else {
            skip_to_end_of_line();
        }

        if (ast_.messages.size() >= options_.max_messages) {
            report(ErrorCode::OutOfRange, "消息数超过 max_messages");
            return false;
        }
        (void)ensure_participant(from);
        (void)ensure_participant(to);
        SequenceMessage message;
        message.from = from;
        message.to = to;
        message.text = std::move(text);
        message.kind = kind;
        message.order = ast_.messages.size();
        message.activate_target = activate_target;
        message.deactivate_source = deactivate_source;
        ast_.messages.push_back(std::move(message));
        return true;
    }

    // `: 文本` 按原文切片取（可含标点/中文），再跳到行尾。
    std::string take_rest_of_line() {
        const std::size_t start = current().offset;
        std::size_t end = start;
        while (end < source().size() && source()[end] != '\n' && source()[end] != '\r') {
            ++end;
        }
        const std::string text = trim(source().substr(start, end - start));
        skip_to_end_of_line();
        return text;
    }

    SequenceParticipant& ensure_participant(const std::string& key) {
        if (SequenceParticipant* existing =
                const_cast<SequenceParticipant*>(ast_.find_participant(key))) {
            return *existing;
        }
        if (ast_.participants.size() >= options_.max_participants) {
            report(ErrorCode::OutOfRange, "参与者数超过 max_participants");
            return ast_.ensure_participant(key);
        }
        SequenceParticipant& participant = ast_.ensure_participant(key);
        participant.kind = "participant";
        return participant;
    }

    SequenceParseOptions options_{};
    AstSequence ast_;
};

}  // namespace

const SequenceParticipant* AstSequence::find_participant(std::string_view key) const noexcept {
    for (const SequenceParticipant& participant : participants) {
        if (participant.key == key) {
            return &participant;
        }
    }
    return nullptr;
}

bool AstSequence::has_participant(std::string_view key) const noexcept {
    return find_participant(key) != nullptr;
}

SequenceParticipant& AstSequence::ensure_participant(std::string_view key) {
    if (SequenceParticipant* existing = const_cast<SequenceParticipant*>(find_participant(key))) {
        return *existing;
    }
    SequenceParticipant participant;
    participant.key.assign(key);
    participant.label.assign(key);
    participant.kind = "participant";
    participants.push_back(std::move(participant));
    return participants.back();
}

const char* message_kind_name(MessageKind kind) noexcept {
    switch (kind) {
        case MessageKind::Solid:
            return "solid";
        case MessageKind::Dotted:
            return "dotted";
        case MessageKind::Async:
            return "async";
        case MessageKind::AsyncDotted:
            return "async-dotted";
        case MessageKind::Open:
            return "open";
        case MessageKind::OpenDotted:
            return "open-dotted";
        case MessageKind::Cross:
            return "cross";
        case MessageKind::CrossDotted:
            return "cross-dotted";
    }
    return "unknown";
}

Result<AstSequence> parse_sequence(std::string_view source, const SequenceParseOptions& options,
                                   ParseDiagnostic* out_diagnostic) {
    const Result<std::vector<Token>> tokens = tokenize(source, sequence_keywords());
    if (!tokens.has_value()) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = tokens.error();
            out_diagnostic->message = "词法分析失败";
        }
        return Unexpected<ErrorCode>(tokens.error());
    }
    SequenceParser parser(*tokens, source, options);
    Result<AstSequence> result = parser.run();
    if (!result.has_value() && out_diagnostic != nullptr) {
        *out_diagnostic = parser.diagnostic();
    }
    return result;
}

}  // namespace termchart::dsl
