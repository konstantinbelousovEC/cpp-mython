#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

namespace parse {

    namespace token_type {
        struct Number {
            int value;
        };

        struct Id {
            std::string value;
        };

        struct Char {
            char value;
        };

        struct String {
            std::string value;
        };

        struct Class {};
        struct Return {};
        struct If {};
        struct Else {};
        struct Def {};
        struct Newline {};
        struct Print {};
        struct Indent {};
        struct Dedent {};
        struct Eof {};
        struct And {};
        struct Or {};
        struct Not {};
        struct Eq {};
        struct NotEq {};
        struct LessOrEq {};
        struct GreaterOrEq {};
        struct None {};
        struct True {};
        struct False {};
    }

    using TokenBase
            = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
            token_type::Class, token_type::Return, token_type::If, token_type::Else,
            token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
            token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
            token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
            token_type::None, token_type::True, token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        [[nodiscard]] const Token& CurrentToken() const;
        Token NextToken();

        template <typename T>
        const T& Expect() const;

        template <typename T, typename U>
        void Expect(const U& value) const;

        template <typename T>
        const T& ExpectNext();

        template <typename T, typename U>
        void ExpectNext(const U& value);

    private:
        static std::unordered_map<std::string, Token> token_types_;

        std::istream& input_;
        Token token_;
        bool initialized_ = false;
        int current_indents_count_ = 0;
        int dedents_to_make_ = 0;

        std::optional<parse::Token> ReadKeyWord(const std::string& str);
        token_type::String ReadString(char quotation_mark);
        token_type::Number ReadNumber();
        std::string ReadIdOrKeyWord();
        void SkipComment();
        void SkipSpaces();
        void SkipEmptyLines();
        int CountIndents();
        Token ProcessEOF();
        void ProcessAlphabetSymbol(char c);
        void ProcessComparisonSymbol(char c);
        void ProcessLineFeed();
        void ProcessSpaceSymbol();
    };

    template <typename T>
    const T& Lexer::Expect() const {
        using namespace std::literals;
        if (!CurrentToken().Is<T>()) throw LexerError("Current token is not of type T"s);
        return CurrentToken().As<T>();
    }

    template <typename T, typename U>
    void Lexer::Expect(const U& value) const {
        using namespace std::literals;
        Expect<T>();
        if (!(CurrentToken().As<T>().value == value)) throw LexerError("Current token has not required value"s);
    }

    template <typename T>
    const T& Lexer::ExpectNext() {
        NextToken();
        return Expect<T>();
    }

    template <typename T, typename U>
    void Lexer::ExpectNext(const U& value) {
        NextToken();
        Expect<T>(value);
    }

}