#include "../include/grep.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>
#include <vector>

namespace minishell {

/**
 * @brief Выполнить grep
 * 
 * Разбирает аргументы команды, выполняет поиск по указанным файлам или stdin
 * и выводит совпадения в stdout.
 * 
 * @param argv Аргументы команды
 * @param io Потоки ввода/вывода
 * @return Результат выполнения команды
 */
auto Grep::execute(const std::vector<std::string> &argv, IoStreams io) -> ExecResult {
    auto options = parseArgs(argv);
    if (!options.has_value()) {
        io.err << "grep: invalid arguments\n";
        return ExecResult{1, false};
    }

    // Если не указаны файлы, читаем из stdin
    if (options->files.empty()) {
        std::regex pattern(options->pattern, 
                          options->caseInsensitive ? std::regex_constants::icase : std::regex_constants::ECMAScript);
        return processFile("", pattern, *options, io);
    }

    // Обрабатываем каждый файл по очереди
    int exitCode = 0;
    for (const auto &file : options->files) {
        std::regex pattern(options->pattern, 
                          options->caseInsensitive ? std::regex_constants::icase : std::regex_constants::ECMAScript);
        auto result = processFile(file, pattern, *options, io);
        if (result.exitCode != 0) {
            exitCode = result.exitCode;
        }
    }
    
    return ExecResult{exitCode, false};
}

/**
 * @brief Разобрать аргументы команды grep
 * 
 * Парсит аргументы команды и формирует структуру GrepOptions с параметрами поиска.
 * Поддерживает ключи -w, -i, -A.
 * 
 * @param argv Аргументы команды
 * @return Структура с параметрами поиска или nullopt при ошибке
 */
auto Grep::parseArgs(const std::vector<std::string> &argv) -> std::optional<GrepOptions> {
    GrepOptions options;
    std::vector<std::string> args = argv;
    
    // Пропускаем имя команды (первый аргумент)
    if (args.empty()) {
        return std::nullopt;
    }
    args.erase(args.begin());

    // Парсим аргументы команды
    for (size_t i = 0; i < args.size(); ) {
        const std::string &arg = args[i];
        
        if (arg == "-w") {
            // Включить режим поиска слова целиком
            options.wholeWord = true;
        } else if (arg == "-i") {
            // Включить регистронезависимый поиск
            options.caseInsensitive = true;
        } else if (arg == "-A") {
            // Обработка ключа -A с числом строк после совпадения
            if (i + 1 >= args.size()) {
                return std::nullopt; // Нет аргумента для -A
            }
            try {
                options.afterLines = std::stoi(args[i + 1]);
                i += 2; // Пропускаем -A и число
                continue;
            } catch (...) {
                return std::nullopt; // Невалидное число после -A
            }
        } else if (arg[0] == '-') {
            // Неизвестный флаг
            return std::nullopt;
        } else {
            // Это либо шаблон поиска, либо имя файла
            if (options.pattern.empty()) {
                options.pattern = arg;
            } else {
                options.files.push_back(arg);
            }
        }
        i++;
    }

    if (options.pattern.empty()) {
        return std::nullopt;
    }

    return options;
}

/**
 * @brief Проверить, является ли символ границей слова
 * 
 * Граница слова определяется как символ, не являющийся буквой, цифрой или символом подчеркивания.
 * 
 * @param ch Символ для проверки
 * @return true, если символ является границей слова
 */
auto Grep::isWordBoundary(char ch) -> bool {
    // Проверяем, является ли символ границей слова
    // Согласно спецификации: не буквы, цифры или символ подчеркивания
    return !std::isalnum(static_cast<unsigned char>(ch)) && ch != '_';
}

/**
 * @brief Поиск совпадений в строке с учетом опций
 * 
 * Если включена опция wholeWord, используется специальная логика поиска слова.
 * В противном случае применяется стандартный регулярный поиск.
 * 
 * @param line Строка для поиска
 * @param pattern Регулярное выражение
 * @param options Опции поиска
 * @return true, если найдено совпадение
 */
auto Grep::searchLine(const std::string &line, const std::regex &pattern, const GrepOptions &options) -> bool {
    if (options.wholeWord) {
        return searchWord(line, options.pattern, options);
    }
    // Для обычного поиска используем стандартный регулярный поиск
    return std::regex_search(line, pattern);
}

/**
 * @brief Поиск слова целиком в строке
 * 
 * Реализует поиск слова целиком с учетом регистра и границ слов.
 * Проверяет, что найденное слово окружено границами (не буквой, цифрой или подчеркиванием).
 * 
 * @param line Строка для поиска
 * @param word Слово для поиска
 * @param options Опции поиска
 * @return true, если найдено совпадение
 */
auto Grep::searchWord(const std::string &line, const std::string &word, const GrepOptions &options) -> bool {
    // Для регистронезависимого поиска преобразуем строку и слово в нижний регистр
    std::string searchLine = line;
    if (options.caseInsensitive) {
        std::transform(searchLine.begin(), searchLine.end(), searchLine.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
    }
    
    std::string searchWord = word;
    if (options.caseInsensitive) {
        std::transform(searchWord.begin(), searchWord.end(), searchWord.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
    }
    
    // Поиск слова в строке
    size_t pos = 0;
    while ((pos = searchLine.find(searchWord, pos)) != std::string::npos) {
        // Проверяем границы слова
        bool leftBoundary = (pos == 0) || isWordBoundary(searchLine[pos - 1]);
        bool rightBoundary = (pos + searchWord.length() >= searchLine.length()) || 
                            isWordBoundary(searchLine[pos + searchWord.length()]);
        
        if (leftBoundary && rightBoundary) {
            return true;
        }
        pos++;
    }
    return false;
}

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
auto Grep::processFile(const std::string &filePath, const std::regex &pattern, 
                      const GrepOptions &options, IoStreams io) -> ExecResult {
    std::istream *input = &std::cin;
    std::ifstream fileStream;
    
    // Открываем файл, если указан путь
    if (!filePath.empty()) {
        fileStream.open(filePath, std::ios::binary);
        if (!fileStream) {
            io.err << "grep: cannot open file: " << filePath << "\n";
            return ExecResult{1, false};
        }
        input = &fileStream;
    }
    
    std::string line;
    int lineNumber = 0;
    std::vector<std::string> linesToPrint;
    int linesPrinted = 0;
    bool printingAfterMatch = false;
    
    // Читаем файл построчно
    while (std::getline(*input, line)) {
        lineNumber++;
        bool match = searchLine(line, pattern, options);
        
        if (match) {
            // Если найдено совпадение
            if (options.afterLines > 0) {
                // Если нужно выводить строки после совпадения
                // Сохраняем текущую строку и следующие N строк
                linesToPrint.clear();
                linesToPrint.push_back(line);
                
                // Читаем следующие строки
                for (int i = 0; i < options.afterLines && std::getline(*input, line); i++) {
                    linesToPrint.push_back(line);
                }
                
                // Выводим все сохраненные строки
                for (const auto &l : linesToPrint) {
                    io.out << l << "\n";
                }
                linesPrinted = 0; // Сброс счетчика
                printingAfterMatch = false;
            } else {
                // Простой вывод строки
                io.out << line << "\n";
            }
        } else if (options.afterLines > 0) {
            // Если нужно выводить строки после совпадения, и текущая строка не совпадает
            if (printingAfterMatch && linesPrinted < options.afterLines) {
                // Выводим строку, если мы уже начали вывод после совпадения
                io.out << line << "\n";
                linesPrinted++;
            } else if (linesToPrint.size() > 0 && linesPrinted < options.afterLines) {
                // Выводим строки из буфера, если они есть
                io.out << line << "\n";
                linesPrinted++;
            }
        }
    }
    
    return ExecResult{0, false};
}

}  // namespace minishell