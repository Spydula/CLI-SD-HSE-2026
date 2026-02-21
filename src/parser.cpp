#include "parser.hpp"
#include <stdexcept>

namespace minishell {

auto Parser::parse(const std::vector<Token> &tokens) -> std::vector<std::vector<std::string>> {
    std::vector<std::vector<std::string>> stages;
    std::vector<std::string> current;

    const auto pushStage = [&]() {
        if (current.empty()) {
            throw std::runtime_error("empty command in pipeline");
        }
        stages.push_back(current);
        current.clear();
    };

    bool sawAnything = false;
    for (const auto &token : tokens) {
        if (token.type == Token::Type::Pipe) {
            if (!sawAnything) {
                throw std::runtime_error("empty command in pipeline");
            }
            pushStage();
        } else {
            sawAnything = true;
            current.push_back(token.text);
        }
    }

    if (!current.empty()) {
        stages.push_back(current);
    } else if (sawAnything) {
        throw std::runtime_error("empty command in pipeline");
    }

    return stages;
}

}  // namespace minishell
