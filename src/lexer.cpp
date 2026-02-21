#include "lexer.hpp"

#include "expander.hpp"
#include "shell.hpp"

#include <cstdint>
#include <stdexcept>

namespace minishell {

auto Lexer::tokenize(std::string_view line, const Environment &env) -> std::vector<Token> {
    enum class State : std::uint8_t { Normal, InSingle, InDouble };
    State state = State::Normal;

    std::vector<Token> tokens;
    std::string current;

    const auto flushCurrent = [&]() {
        if (!current.empty()) {
            tokens.push_back(Token{Token::Type::Word, current});
            current.clear();
        }
    };

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];

        switch (state) {
            case State::Normal:
                if (ch == '|') {
                    flushCurrent();
                    tokens.push_back(Token{Token::Type::Pipe, ""});
                } else if (ch == ' ' || ch == '\t') {
                    flushCurrent();
                } else if (ch == '\'') {
                    state = State::InSingle;
                } else if (ch == '"') {
                    state = State::InDouble;
                } else if (ch == '$') {
                    Expander::expandAt(current, line, index, env);
                } else {
                    current.push_back(ch);
                }
                break;

            case State::InSingle:
                if (ch == '\'') {
                    state = State::Normal;
                } else {
                    current.push_back(ch);
                }
                break;

            case State::InDouble:
                if (ch == '"') {
                    state = State::Normal;
                } else if (ch == '$') {
                    Expander::expandAt(current, line, index, env);
                } else {
                    current.push_back(ch);
                }
                break;
        }
    }

    if (state != State::Normal) {
        throw std::runtime_error("unterminated quote");
    }

    flushCurrent();
    return tokens;
}

}  // namespace minishell
