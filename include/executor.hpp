#pragma once

#include <iosfwd>
#include <string>
#include <vector>
#include "shell.hpp"

namespace minishell {

/**
 * @brief Исполнитель пайплайнов.
 */
class Executor {
public:
    ExecResult execute(const std::vector<std::vector<std::string>> &stages,
                       Shell &shell,
                       std::ostream &out,
                       std::ostream &err) const;
};

}  // namespace minishell
