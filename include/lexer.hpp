#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace minishell {

class Environment;

struct Token {
    enum class Type { Word, Pipe };
    Type type;
    std::string text;
};

/**
 * @brief Лексер для строки командной строки.
 */
class Lexer {
public:
    static auto tokenize(std::string_view line, const Environment &env) -> std::vector<Token>;
};

}  // namespace minishell
