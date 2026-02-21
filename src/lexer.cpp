#include "lexer.hpp"
#include <cctype>
#include <stdexcept>
#include "expander.hpp"
#include "shell.hpp"

namespace minishell {

std::vector<Token> Lexer::tokenize(std::string_view line, const Environment &env) const {
    enum class State { Normal, InSingle, InDouble };
    State st = State::Normal;

    std::vector<Token> tokens;
    std::string cur;

    auto flush = [&]() {
        if (!cur.empty()) {
            tokens.push_back(Token{Token::Type::Word, cur});
            cur.clear();
        }
    };

    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        switch (st) {
            case State::Normal:
                if (c == '|') {
                    flush();
                    tokens.push_back(Token{Token::Type::Pipe, ""});
                } else if (c == ' ' || c == '\t') {
                    flush();
                } else if (c == '\'') {
                    st = State::InSingle;
                } else if (c == '"') {
                    st = State::InDouble;
                } else if (c == '$') {
                    Expander::expandAt(cur, line, i, env);
                } else {
                    cur.push_back(c);
                }
                break;

            case State::InSingle:
                if (c == '\'') {
                    st = State::Normal;
                } else {
                    cur.push_back(c);
                }
                break;

            case State::InDouble:
                if (c == '"') {
                    st = State::Normal;
                } else if (c == '$') {
                    Expander::expandAt(cur, line, i, env);
                } else {
                    cur.push_back(c);
                }
                break;
        }
    }

    if (st != State::Normal) {
        throw std::runtime_error("unterminated quote");
    }
    flush();
    return tokens;
}

}  // namespace minishell
