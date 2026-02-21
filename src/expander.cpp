#include "expander.hpp"

#include "shell.hpp"

#include <cctype>

namespace minishell {

void Expander::expandAt(std::string &out,
                        std::string_view line,
                        std::size_t &index,
                        const Environment &env) {
    std::size_t scanIndex = index + 1U;
    if (scanIndex >= line.size()) {
        out.push_back('$');
        return;
    }

    const auto isAlphaUnd = [](unsigned char ch) {
        return (std::isalpha(ch) != 0) || ch == '_';
    };
    const auto isAlnumUnd = [](unsigned char ch) {
        return (std::isalnum(ch) != 0) || ch == '_';
    };

    if (!isAlphaUnd(static_cast<unsigned char>(line[scanIndex]))) {
        out.push_back('$');
        return;
    }

    std::string name;
    name.push_back(static_cast<char>(line[scanIndex]));
    ++scanIndex;
    while (scanIndex < line.size() &&
           isAlnumUnd(static_cast<unsigned char>(line[scanIndex]))) {
        name.push_back(static_cast<char>(line[scanIndex]));
        ++scanIndex;
    }

    if (auto value = env.get(name); value.has_value()) {
        out += *value;
    }

    index = scanIndex - 1U;
}

}  // namespace minishell
