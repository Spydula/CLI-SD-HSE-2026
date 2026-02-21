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
    std::vector<std::vector<std::string>> parse(const std::vector<Token> &tokens) const;
};

}  // namespace minishell
