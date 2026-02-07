#include "shell.hpp"

#include <iostream>

/**
 * @brief Точка входа приложения.
 *
 * Создаёт интерпретатор и запускает REPL.
 */
int main() {
  minishell::Shell shell;
  return shell.run(std::cin, std::cout, std::cerr);
}