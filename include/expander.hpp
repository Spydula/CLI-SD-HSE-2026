#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace minishell {

class Environment;

/**
 * @brief Переменная подстановка вида $NAME.
 */
class Expander {
public:
    /**
     * @brief Попробовать расширить $VAR, начиная с позиции i (line[i] == '$').
     */
    static void expandAt(std::string &out,
                         std::string_view line,
                         std::size_t &i,
                         const Environment &env);
};

}  // namespace minishell
