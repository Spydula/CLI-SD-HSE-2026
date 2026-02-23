#pragma once

#include <string>
#include <vector>
#include <regex>
#include <optional>
#include "shell.hpp"

namespace minishell {

/**
 * Хранит параметры конфигурации для grep:
 * - caseInsensitive: регистронезависимый поиск
 * - wholeWord: поиск только слова целиком
 * - afterLines: количество строк после совпадения для вывода
 * - pattern: шаблон поиска
 * - files: список файлов для поиска
 */
struct GrepOptions {
    bool caseInsensitive = false;
    bool wholeWord = false;
    int afterLines = 0;
    std::string pattern;
    std::vector<std::string> files;
};

/**
 * @brief Реализация grep
 * 
 * Поддерживает:
 * - регулярные выражения в запросе
 * - ключ -w (поиск только слова целиком)
 * - ключ -i (регистронезависимый поиск)
 * - ключ -A (следующее за -A число говорит, сколько строк после совпадения надо распечатать)
 * 
 */
class Grep {
public:
    /**
     * @brief Выполнить команду grep
     * 
     * Разбирает аргументы команды, выполняет поиск по указанным файлам или stdin
     * и выводит совпадения в stdout.
     * 
     * @param argv Аргументы команды
     * @param io Потоки ввода/вывода
     * @return Результат выполнения команды
     */
    static auto execute(const std::vector<std::string> &argv, IoStreams io) -> ExecResult;

    /**
     * @brief Разобрать аргументы команды grep
     * 
     * Парсит аргументы команды и формирует структуру GrepOptions с параметрами поиска.
     * 
     * @param argv Аргументы команды
     * @return Структура с параметрами поиска или nullopt при ошибке
     */
    static auto parseArgs(const std::vector<std::string> &argv) -> std::optional<GrepOptions>;

    /**
     * @brief Проверить, является ли символ границей слова
     * 
     * Граница слова определяется как символ, не являющийся буквой, цифрой или символом подчеркивания.
     * 
     * @param ch Символ для проверки
     * @return true, если символ является границей слова
     */
    static auto isWordBoundary(char ch) -> bool;

    /**
     * @brief Поиск совпадений в строке с учетом опций
     * 
     * 
     * @param line Строка для поиска
     * @param pattern Регулярное выражение
     * @param options Опции поиска
     * @return true, если найдено совпадение
     */
    static auto searchLine(const std::string &line, const std::regex &pattern, const GrepOptions &options) -> bool;

    /**
     * @brief Поиск слова целиком в строке
     * 
     * Реализует поиск слова целиком с учетом регистра и границ слов.
     * 
     * @param line Строка для поиска
     * @param word Слово для поиска
     * @param options Опции поиска
     * @return true, если найдено совпадение
     */
    static auto searchWord(const std::string &line, const std::string &word, const GrepOptions &options) -> bool;

    /**
     * @brief Обработать файл или stdin
     * 
     * Читает файл или stdin построчно и выполняет поиск совпадений.
     * При необходимости выводит дополнительные строки после совпадения.
     * 
     * @param filePath Путь к файлу (пустая строка для stdin)
     * @param pattern Регулярное выражение для поиска
     * @param options Опции поиска
     * @param io Потоки ввода/вывода
     * @return Результат выполнения
     */
    static auto processFile(const std::string &filePath, const std::regex &pattern, 
                           const GrepOptions &options, IoStreams io) -> ExecResult;
};

}  // namespace minishell