#include <iostream>
#include "shell.hpp"

/**
 * @brief Точка входа приложения.
 *
 * Создаёт интерпретатор и запускает REPL.
 */
int main() {
    minishell::Shell shell;
    return shell.run(std::cin, std::cout, std::cerr);
}