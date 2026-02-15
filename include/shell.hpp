#pragma once

#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minishell {

/**
 * @brief Хранилище переменных окружения интерпретатора.
 *
 * Environment хранит пары NAME->VALUE и умеет экспортировать их
 * в формат, подходящий для передачи в execve/execvpe.
 */
class Environment {
public:
    /**
     * @brief Установить/обновить переменную окружения.
     * @param name Имя переменной (например, "PATH").
     * @param value Значение переменной.
     */
    void set(std::string name, std::string value);

    /**
     * @brief Получить значение переменной окружения.
     * @param name Имя переменной.
     * @return Значение, если переменная существует.
     */
    std::optional<std::string> get(std::string_view name) const;

    /**
     * @brief Получить снимок всех переменных окружения.
     * @return map со всеми переменными.
     */
    const std::map<std::string, std::string> &snapshot() const;

    /**
     * @brief Инициализировать окружение значениями из окружения текущего процесса.
     */
    static Environment fromProcessEnvironment();

private:
    std::map<std::string, std::string> vars_;
};

/**
 * @brief Результат выполнения команды/строки.
 */
struct ExecResult {
    /** @brief Код возврата (0 — успех). */
    int exitCode = 0;
    /** @brief Нужно ли завершить REPL (команда exit). */
    bool shouldExit = false;
};

/**
 * @brief Токенизатор командной строки.
 *
 * Поддерживает:
 * - пробелы как разделители аргументов
 * - одинарные и двойные кавычки: содержимое в кавычках — один аргумент
 *
 * Подстановки и пайпы НЕ поддерживаются в этой части.
 */
class Tokenizer {
public:
    /**
     * @brief Разбить строку на аргументы (argv).
     * @param line Строка ввода пользователя.
     * @return Вектор аргументов. Может быть пустым для пустой строки.
     * @throws std::runtime_error при незакрытых кавычках.
     */
    static std::vector<std::string> tokenize(std::string_view line);

    /**
     * @brief Разобрать строку с поддержкой пайпов '|' и подстановок $VAR.
     *        Кавычки: одинарные блокируют подстановки, двойные разрешают.
     * @throws std::runtime_error при незакрытых кавычках.
     */
    static std::vector<std::string> tokenizeWithPipesAndExpansion(std::string_view line,
                                                                  const Environment &env);
};

/**
 * @brief Основной класс интерпретатора: REPL + выполнение команд.
 */
class Shell {
public:
    /**
     * @brief Создать shell с окружением, инициализированным из окружения процесса.
     */
    Shell();

    /**
     * @brief Запустить Read-Execute-Print Loop.
     * @param in Поток ввода (обычно std::cin).
     * @param out Поток вывода (обычно std::cout).
     * @param err Поток ошибок (обычно std::cerr).
     * @return Код завершения интерпретатора.
     */
    int run(std::istream &in, std::ostream &out, std::ostream &err);

    /**
     * @brief Выполнить одну строку (удобно для unit-тестов).
     * @param line Строка ввода.
     * @param out Поток вывода.
     * @param err Поток ошибок.
     * @return Результат выполнения.
     */
    ExecResult executeLine(std::string_view line, std::ostream &out, std::ostream &err);

    /**
     * @brief Доступ к окружению интерпретатора.
     */
    Environment &env();

    /**
     * @brief Доступ к окружению интерпретатора (const).
     */
    const Environment &env() const;

private:
    /**
     * @brief Выполнить уже разобранный argv.
     */
    ExecResult executeArgv(const std::vector<std::string> &argv,
                           std::ostream &out,
                           std::ostream &err);

    /**
     * @brief Встроенная команда cat.
     */
    static ExecResult builtinCat(const std::vector<std::string> &argv,
                                 std::ostream &out,
                                 std::ostream &err);

    /**
     * @brief Встроенная команда echo.
     */
    static ExecResult builtinEcho(const std::vector<std::string> &argv,
                                  std::ostream &out,
                                  std::ostream &err);

    /**
     * @brief Встроенная команда wc.
     */
    static ExecResult builtinWc(const std::vector<std::string> &argv,
                                std::ostream &out,
                                std::ostream &err);

    /**
     * @brief Встроенная команда pwd.
     */
    static ExecResult builtinPwd(std::ostream &out, std::ostream &err);

    /**
     * @brief Встроенная команда exit.
     */
    static ExecResult builtinExit();

    /**
     * @brief Запустить внешнюю программу.
     */
    ExecResult runExternal(const std::vector<std::string> &argv, std::ostream &err);

    /**
     * @brief Обработать строку, состоящую только из присваиваний NAME=value.
     *
     * Если каждый аргумент имеет вид NAME=value, то переменные сохраняются в env_,
     * команда не исполняется, возвращается true.
     *
     * @param argv Аргументы строки.
     * @param err Поток ошибок (для сообщений о некорректных именах).
     * @return true если строка обработана как присваивания.
     */
    bool tryHandleAssignmentsOnly(const std::vector<std::string> &argv, std::ostream &err);

    Environment env_;
};

}  // namespace minishell