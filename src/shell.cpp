#include "shell.hpp"
#include <iostream>
#include <string>

int Shell::run() {
    std::string line;

    while (true) {
        std::cout << "> " << std::flush;

        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            return 0;
        }

        if (line == "exit") {
            return 0;
        }

        if (!line.empty()) {
            std::cout << "you typed: " << line << "\n";
        }
    }
}