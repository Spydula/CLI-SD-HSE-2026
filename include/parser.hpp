#pragma once

#include <string>
#include <vector>
#include "lexer.hpp"

namespace minishell {

/**
 * @brief Парсер пайплайна.
 */
class Parser {
public:
    static auto parse(const std::vector<Token> &tokens) -> std::vector<std::vector<std::string>>;
};

}  // namespace minishell
