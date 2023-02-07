#include "lexer.h"
#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <string>
#include <string_view>

using namespace std::literals;

namespace parse {

    namespace {
        const char UNDERLINE_SYMBOL = '_';
        const char ZERO_SYMBOL = '0';
        const char COMMENT_SYMBOL = '#';
        const char LINE_FEED_SYMBOL = '\n';
        const char SPACE_SYMBOL = ' ';
    }

    namespace detail {
        bool IsAlphabetSymbol(char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        }
        bool IsPositiveDigitSymbol(char c) {
            return (c >= '1' && c <= '9');
        }
        bool IsSpecialSymbol(char c) {
            return (c >= '(' && c <= '/') || c == ':';
        }
        bool IsComparisonSymbol(char c) {
            return c == '!' || (c >= '<' && c <= '>');
        }
        bool IsQuotationMark(char c) {
            return c == '\'' || c == '\"';
        }
    }

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input) : input_(input) {
        token_ = NextToken();
    }

    const Token& Lexer::CurrentToken() const {
        return token_;
    }

    Token Lexer::NextToken() {
        if (dedents_to_make_ > 0) {
            token_ = token_type::Dedent{};
            current_indents_count_--;
            dedents_to_make_--;
            return token_;
        }
        if (token_.Is<token_type::Eof>()) return token_;

        char c = input_.get();
        if (c == EOF) return ProcessEOF();

        if (detail::IsQuotationMark(c)) {
            char quotation_mark = c;
            token_ = ReadString(quotation_mark);
        } else if (detail::IsAlphabetSymbol(c)) {
            ProcessAlphabetSymbol(c);
        } else if (c == UNDERLINE_SYMBOL) {
            input_.putback(c);
            std::string str = ReadIdOrKeyWord();
            token_ = token_type::Id{str};
        } else if (c == ZERO_SYMBOL) {
            token_ = token_type::Number{0};
        } else if (detail::IsPositiveDigitSymbol(c)) {
            input_.putback(c);
            token_ = ReadNumber();
        } else if (detail::IsSpecialSymbol(c)) {
            token_ = token_type::Char{c};
        } else if (detail::IsComparisonSymbol(c)) {
            ProcessComparisonSymbol(c);
        } else if (c == COMMENT_SYMBOL) {
            SkipComment();
            token_ = NextToken();
        } else if (c == LINE_FEED_SYMBOL) {
            ProcessLineFeed();
        } else if (c == SPACE_SYMBOL) {
            ProcessSpaceSymbol();
        }
        initialized_ = true;
        return token_;
    }

    std::optional<parse::Token> Lexer::ReadKeyWord(const std::string& str) {
        if (str == "class") {
            return parse::token_type::Class{};
        } else if (str == "return") {
            return parse::token_type::Return{};
        } else if (str == "if") {
            return parse::token_type::If{};
        } else if (str == "else") {
            return parse::token_type::Else{};
        } else if (str == "def") {
            return parse::token_type::Def{};
        } else if (str == "print") {
            return parse::token_type::Print{};
        } else if (str == "and") {
            return parse::token_type::And{};
        } else if (str == "or") {
            return parse::token_type::Or{};
        } else if (str == "not") {
            return parse::token_type::Not{};
        } else if (str == "None") {
            return parse::token_type::None{};
        } else if (str == "True") {
            return parse::token_type::True{};
        } else if (str == "False") {
            return parse::token_type::False{};
        } else {
            return std::nullopt;
        }
    }

    token_type::String Lexer::ReadString(char quotation_mark) {
        auto it = std::istreambuf_iterator<char>(input_);
        auto end = std::istreambuf_iterator<char>();
        std::string s;
        while (true) {
            if (it == end) {
                throw LexerError("String parsing error");
            }
            const char ch = *it;
            if (ch == quotation_mark) {
                ++it;
                break;
            } else if (ch == '\\') {
                ++it;
                if (it == end) {
                    throw LexerError("String parsing error");
                }
                const char escaped_char = *(it);
                switch (escaped_char) {
                    case 'n':
                        s.push_back('\n');
                        break;
                    case 't':
                        s.push_back('\t');
                        break;
                    case '"':
                        s.push_back('"');
                        break;
                    case '\'':
                        s.push_back('\'');
                        break;
                    default:
                        throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
                }
            } else if (ch == '\n' || ch == '\r') {
                throw LexerError("Unexpected end of line"s);
            } else {
                s.push_back(ch);
            }
            ++it;
        }

        return token_type::String{s};
    }

    token_type::Number Lexer::ReadNumber() {
        std::string str_num;
        char c;
        do {
            c = input_.get();
            str_num += c;
        } while (input_.peek() >= '0' && input_.peek() <= '9');
        int result = std::stoi(str_num);
        return token_type::Number{result};
    }

    void Lexer::SkipComment() {
        input_.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        input_.putback('\n');
    }

    int Lexer::CountIndents() {
        int spaces = 1;
        while (input_.peek() == SPACE_SYMBOL) {
            input_.get();
            spaces++;
        }
        return spaces / 2;
    }

    void Lexer::SkipSpaces() {
        while (input_.peek() == SPACE_SYMBOL) {
            input_.get();
        }
    }

    std::string Lexer::ReadIdOrKeyWord() {
        std::string str;
        char c = input_.peek();
        while (detail::IsAlphabetSymbol(c) ||
               detail::IsPositiveDigitSymbol(c) ||
               c == UNDERLINE_SYMBOL ||
               c == ZERO_SYMBOL) {
            str += input_.get();
            c = input_.peek();
        }
        return str;
    }

    Token Lexer::ProcessEOF() {
        if (current_indents_count_ > 0) {
            token_ = token_type::Dedent{};
            current_indents_count_--;
            return token_;
        }
        if (token_.Is<token_type::Newline>() || token_.Is<token_type::Dedent>() || !initialized_) {
            token_ = token_type::Eof{};
            return token_;
        } else {
            token_ = token_type::Newline{};
            return token_;
        }
    }

    void Lexer::ProcessAlphabetSymbol(char c) {
        input_.putback(c);
        std::string str = ReadIdOrKeyWord();
        std::optional<parse::Token> key_word = ReadKeyWord(str);
        if (!key_word.has_value()) {
            token_ = token_type::Id{str};
        } else {
            token_ = key_word.value();
        }
    }

    void Lexer::ProcessComparisonSymbol(char c) {
        char second = input_.peek();
        if (second == '=') {
            if (c == '!') {
                token_ = token_type::NotEq{};
            } else if (c == '=') {
                token_ = token_type::Eq{};
            } else if (c == '>') {
                token_ = token_type::GreaterOrEq{};
            } else if (c == '<') {
                token_ = token_type::LessOrEq{};
            }
            input_.ignore(1);
        } else {
            token_ = token_type::Char{c};
        }
    }

    void Lexer::ProcessLineFeed() {
        if (input_.peek() != SPACE_SYMBOL &&
            input_.peek() != LINE_FEED_SYMBOL &&
            current_indents_count_ > 0) dedents_to_make_ = current_indents_count_;

        if (token_.Is<token_type::Newline>() || !initialized_) {
            token_ = NextToken();
        } else {
            if (input_.peek() == '\n') SkipEmptyLines();
            char c = input_.peek();
            if ( (detail::IsAlphabetSymbol(c) || c == UNDERLINE_SYMBOL) && current_indents_count_ > 0) {
                dedents_to_make_ = current_indents_count_;
            }
            token_ = token_type::Newline{};
        }
    }

    void Lexer::ProcessSpaceSymbol() {
        if (token_.Is<token_type::Newline>()) {
            int indents_in_line = CountIndents();
            if (current_indents_count_ < indents_in_line) {
                token_ = token_type::Indent{};
            } else if (current_indents_count_ > indents_in_line) {
                token_ = token_type::Dedent{};
                dedents_to_make_ = current_indents_count_ - indents_in_line - 1;
            } else {
                token_ = NextToken();
            }
            current_indents_count_ = indents_in_line;
        } else {
            SkipSpaces();
            token_ = NextToken();
        }
    }

    void Lexer::SkipEmptyLines() {
        while (input_.peek() == LINE_FEED_SYMBOL) {
            input_.ignore(1);
        }
    }
}