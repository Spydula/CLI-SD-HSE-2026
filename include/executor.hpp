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
    static auto execute(const std::vector<std::vector<std::string>> &stages,
                        Shell &shell,
                        std::ostream &out,
                        std::ostream &err) -> ExecResult;
};

}  // namespace minishell
