#include "parser.hpp"
#include <stdexcept>

namespace minishell {

std::vector<std::vector<std::string>> Parser::parse(const std::vector<Token> &tokens) const {
    std::vector<std::vector<std::string>> stages;
    std::vector<std::string> cur;

    auto pushStage = [&]() {
        if (cur.empty()) {
            throw std::runtime_error("empty command in pipeline");
        }
        stages.push_back(cur);
        cur.clear();
    };

    bool sawAnything = false;
    for (const auto &t : tokens) {
        if (t.type == Token::Type::Pipe) {
            if (!sawAnything) {
                // leading pipe
                throw std::runtime_error("empty command in pipeline");
            }
            pushStage();
        } else {
            sawAnything = true;
            cur.push_back(t.text);
        }
    }

    if (!cur.empty()) {
        stages.push_back(cur);
    } else if (sawAnything) {
        throw std::runtime_error("empty command in pipeline");
    }

    return stages;
}

}  // namespace minishell
