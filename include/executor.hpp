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

private:
    [[noreturn]] static auto execStage(const std::vector<std::string> &argv,
                                       Shell &shell,
                                       int inputFd,
                                       int outputFd,
                                       int errorFd) -> void;
};

}  // namespace minishell
