#include "expander.hpp"
#include <cctype>
#include "shell.hpp"

namespace minishell {

void Expander::expandAt(std::string &out,
                        std::string_view line,
                        std::size_t &i,
                        const Environment &env) {
    std::size_t j = i + 1;
    if (j >= line.size()) {
        out.push_back('$');
        return;
    }

    auto isAlphaUnd = [](unsigned char ch) { return std::isalpha(ch) || ch == '_'; };
    auto isAlnumUnd = [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; };

    if (!isAlphaUnd(static_cast<unsigned char>(line[j]))) {
        out.push_back('$');
        return;
    }

    std::string name;
    name.push_back(static_cast<char>(line[j]));
    ++j;
    while (j < line.size() && isAlnumUnd(static_cast<unsigned char>(line[j]))) {
        name.push_back(static_cast<char>(line[j]));
        ++j;
    }

    if (auto v = env.get(name); v.has_value()) {
        out += *v;
    }

    i = j - 1;
}

}  // namespace minishell
