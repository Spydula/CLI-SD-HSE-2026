#include "shell.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include "executor.hpp"
#include "lexer.hpp"
#include "parser.hpp"

#if defined(__APPLE__)
#include <crt_externs.h>
#endif

namespace minishell {
namespace {

constexpr int kExitCommandNotFound = 127;
constexpr int kExitSignalBase = 128;
constexpr int kExitParseError = 2;
constexpr int kExitUsageError = 2;
constexpr int kExitRuntimeError = 1;

auto errnoMessage() -> std::string {
    return std::system_error(errno, std::generic_category()).what();
}

auto isValidVarName(std::string_view name) -> bool {
    if (name.empty()) {
        return false;
    }

    const auto isAlphaUnd = [](unsigned char ch) { return (std::isalpha(ch) != 0) || ch == '_'; };
    const auto isAlnumUnd = [](unsigned char ch) { return (std::isalnum(ch) != 0) || ch == '_'; };

    if (!isAlphaUnd(static_cast<unsigned char>(name[0]))) {
        return false;
    }
    for (std::size_t index = 1; index < name.size(); ++index) {
        if (!isAlnumUnd(static_cast<unsigned char>(name[index]))) {
            return false;
        }
    }
    return true;
}

auto isExecutableFile(const std::filesystem::path &path) -> bool {
    std::error_code error;
    if (!std::filesystem::exists(path, error) || error) {
        return false;
    }
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return false;
    }
    return ::access(path.c_str(), X_OK) == 0;
}

auto findInPath(const std::string &file, const Environment &env) -> std::optional<std::string> {
    // Если в имени есть '/', считаем что это путь и не ищем по PATH.
    if (file.find('/') != std::string::npos) {
        if (isExecutableFile(file)) {
            return file;
        }
        return std::nullopt;
    }

    std::string pathValue;
    if (auto value = env.get("PATH"); value.has_value()) {
        pathValue = *value;
    } else {
        const char *processPath = std::getenv("PATH");
        pathValue = (processPath != nullptr) ? processPath : "";
    }

    std::stringstream stream(pathValue);
    std::string directory;
    while (std::getline(stream, directory, ':')) {
        if (directory.empty()) {
            directory = ".";
        }
        const std::filesystem::path candidate = std::filesystem::path(directory) / file;
        if (isExecutableFile(candidate)) {
            return candidate.string();
        }
    }

    return std::nullopt;
}

}  // namespace

// ---------------- Environment ----------------

auto Environment::fromProcessEnvironment() -> Environment {
    Environment env;

    char **processEnv = nullptr;

#if defined(__APPLE__)
    processEnv = *_NSGetEnviron();
#else
    processEnv = ::environ;
#endif

    if (processEnv == nullptr) {
        return env;
    }

    for (char **entryPtr = processEnv; *entryPtr != nullptr; ++entryPtr) {
        const std::string entry(*entryPtr);
        const std::size_t pos = entry.find('=');
        if (pos == std::string::npos || pos == 0U) {
            continue;
        }
        env.set(entry.substr(0, pos), entry.substr(pos + 1U));
    }

    return env;
}

auto Environment::snapshot() const -> const std::map<std::string, std::string> & {
    return vars_;
}

auto Environment::get(std::string_view name) const -> std::optional<std::string> {
    const auto iterator = vars_.find(std::string(name));
    if (iterator == vars_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

auto Environment::set(std::string name, std::string value) -> void {
    vars_[std::move(name)] = std::move(value);
}

// ---------------- Tokenizer ----------------

auto Tokenizer::tokenize(std::string_view line) -> std::vector<std::string> {
    enum class State : std::uint8_t { Normal, InSingle, InDouble };

    State state = State::Normal;
    std::vector<std::string> argv;
    std::string current;

    const auto flush = [&]() {
        if (!current.empty()) {
            argv.push_back(current);
            current.clear();
        }
    };

    for (const char ch : line) {
        switch (state) {
            case State::Normal:
                if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                    flush();
                } else if (ch == '\'') {
                    state = State::InSingle;
                } else if (ch == '"') {
                    state = State::InDouble;
                } else {
                    current.push_back(ch);
                }
                break;

            case State::InSingle:
                if (ch == '\'') {
                    state = State::Normal;
                } else {
                    current.push_back(ch);
                }
                break;

            case State::InDouble:
                if (ch == '"') {
                    state = State::Normal;
                } else {
                    current.push_back(ch);
                }
                break;
        }
    }

    if (state != State::Normal) {
        throw std::runtime_error("unterminated quote");
    }

    flush();
    return argv;
}

auto Tokenizer::tokenizeWithPipesAndExpansion(std::string_view line, const Environment &env)
    -> std::vector<std::string> {
    const std::vector<Token> tokens = Lexer::tokenize(line, env);

    std::vector<std::string> output;
    output.reserve(tokens.size());
    for (const auto &token : tokens) {
        if (token.type == Token::Type::Pipe) {
            output.emplace_back("|");
        } else {
            output.push_back(token.text);
        }
    }
    return output;
}

// ---------------- Shell ----------------

Shell::Shell() : env_(Environment::fromProcessEnvironment()) {
}

auto Shell::run(std::istream &in, std::ostream &out, std::ostream &err) -> int {
    std::string line;
    while (true) {
        if (!std::getline(in, line)) {
            break;
        }

        const ExecResult result = executeLine(line, out, err);
        if (result.shouldExit) {
            return result.exitCode;
        }
    }
    return 0;
}

auto Shell::executeLine(std::string_view line, std::ostream &out, std::ostream &err) -> ExecResult {
    std::vector<std::vector<std::string>> stages;
    try {
        const std::vector<Token> tokens = Lexer::tokenize(line, env_);
        stages = Parser::parse(tokens);
    } catch (const std::exception &ex) {
        err << "parse error: " << ex.what() << "\n";
        return ExecResult{kExitParseError, false};
    }

    if (stages.empty()) {
        return ExecResult{0, false};
    }

    if (stages.size() == 1U) {
        const auto &argv = stages.front();
        if (argv.empty()) {
            return ExecResult{0, false};
        }
        if (tryHandleAssignmentsOnly(argv, err)) {
            return ExecResult{0, false};
        }
    }

    return Executor::execute(stages, *this, out, err);
}

auto Shell::env() -> Environment & {
    return env_;
}

auto Shell::env() const -> const Environment & {
    return env_;
}

auto Shell::tryHandleAssignmentsOnly(const std::vector<std::string> &argv, std::ostream &err)
    -> bool {
    for (const auto &arg : argv) {
        const std::size_t pos = arg.find('=');
        if (pos == std::string::npos || pos == 0U) {
            return false;
        }

        const std::string_view name(arg.data(), pos);
        if (!isValidVarName(name)) {
            err << "assignment error: invalid variable name: " << std::string(name) << "\n";
            return false;
        }
    }

    for (const auto &arg : argv) {
        const std::size_t pos = arg.find('=');
        env_.set(arg.substr(0, pos), arg.substr(pos + 1U));
    }

    return true;
}

auto Shell::executeArgv(const std::vector<std::string> &argv, IoStreams io) -> ExecResult {
    const std::string &cmd = argv[0];

    if (cmd == "cat") {
        return builtinCat(argv, io);
    }
    if (cmd == "echo") {
        return builtinEcho(argv, io);
    }
    if (cmd == "wc") {
        return builtinWc(argv, io);
    }
    if (cmd == "pwd") {
        return builtinPwd(io);
    }
    if (cmd == "exit") {
        return builtinExit();
    }

    return runExternal(argv, io.err);
}

// ---------------- Builtins ----------------

auto Shell::builtinExit() -> ExecResult {
    return ExecResult{0, true};
}

auto Shell::builtinPwd(IoStreams io) -> ExecResult {
    try {
        io.out << std::filesystem::current_path().string() << "\n";
        return ExecResult{0, false};
    } catch (const std::exception &ex) {
        io.err << "pwd: " << ex.what() << "\n";
        return ExecResult{kExitRuntimeError, false};
    }
}

auto Shell::builtinEcho(const std::vector<std::string> &argv, IoStreams io) -> ExecResult {
    for (std::size_t index = 1; index < argv.size(); ++index) {
        if (index > 1U) {
            io.out << ' ';
        }
        io.out << argv[index];
    }
    io.out << "\n";
    return ExecResult{0, false};
}

auto Shell::builtinCat(const std::vector<std::string> &argv, IoStreams io) -> ExecResult {
    if (argv.size() == 1U) {
        io.out << std::cin.rdbuf();
        return ExecResult{0, false};
    }

    if (argv.size() != 2U) {
        io.err << "cat: usage: cat <FILE>\n";
        return ExecResult{kExitUsageError, false};
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        io.err << "cat: cannot open file: " << argv[1] << "\n";
        return ExecResult{kExitRuntimeError, false};
    }

    io.out << file.rdbuf();
    return ExecResult{0, false};
}

auto Shell::builtinWc(const std::vector<std::string> &argv, IoStreams io) -> ExecResult {
    std::istream *input = &std::cin;
    std::ifstream file;

    if (argv.size() == 2U) {
        file.open(argv[1], std::ios::binary);
        if (!file) {
            io.err << "wc: cannot open file: " << argv[1] << "\n";
            return ExecResult{kExitRuntimeError, false};
        }
        input = &file;
    } else if (argv.size() != 1U) {
        io.err << "wc: usage: wc <FILE>\n";
        return ExecResult{kExitUsageError, false};
    }

    std::uint64_t lines = 0;
    std::uint64_t words = 0;
    std::uint64_t bytes = 0;

    bool inWord = false;
    char ch = 0;
    while (input->get(ch)) {
        ++bytes;
        if (ch == '\n') {
            ++lines;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            inWord = false;
        } else if (!inWord) {
            ++words;
            inWord = true;
        }
    }

    io.out << lines << " " << words << " " << bytes << "\n";
    return ExecResult{0, false};
}

// ---------------- External execution ----------------

auto Shell::runExternal(const std::vector<std::string> &argv, std::ostream &err) -> ExecResult {
    const auto fullPathOpt = findInPath(argv[0], env_);
    if (!fullPathOpt.has_value()) {
        err << argv[0] << ": command not found\n";
        return ExecResult{kExitCommandNotFound, false};
    }
    const std::string fullPath = *fullPathOpt;

    std::vector<std::string> argsStorage = argv;
    argsStorage[0] = fullPath;
    std::vector<const char *> args;
    args.reserve(argsStorage.size() + 1U);
    std::transform(argsStorage.begin(), argsStorage.end(), std::back_inserter(args),
                   [](const std::string &arg) { return arg.c_str(); });

    args.push_back(nullptr);

    std::vector<std::string> envStorage;
    envStorage.reserve(env_.snapshot().size());
    const auto &snapshot = env_.snapshot();
    std::transform(snapshot.begin(), snapshot.end(), std::back_inserter(envStorage),
                   [](const auto &pair) { return pair.first + "=" + pair.second; });

    std::vector<const char *> envp;
    envp.reserve(envStorage.size() + 1U);
    std::transform(envStorage.begin(), envStorage.end(), std::back_inserter(envp),
                   [](const std::string &entry) { return entry.c_str(); });
    envp.push_back(nullptr);

    const pid_t childPid = ::fork();
    if (childPid < 0) {
        err << argv[0] << ": fork failed: " << errnoMessage() << "\n";
        return ExecResult{kExitCommandNotFound, false};
    }

    if (childPid == 0) {
        ::execve(args[0], const_cast<char *const *>(args.data()),
                 const_cast<char *const *>(envp.data()));
        _exit(kExitCommandNotFound);
    }

    int status = 0;
    if (::waitpid(childPid, &status, 0) < 0) {
        err << argv[0] << ": waitpid failed: " << errnoMessage() << "\n";
        return ExecResult{kExitCommandNotFound, false};
    }

    if (WIFEXITED(status)) {
        return ExecResult{WEXITSTATUS(status), false};
    }
    if (WIFSIGNALED(status)) {
        return ExecResult{kExitSignalBase + WTERMSIG(status), false};
    }

    return ExecResult{kExitCommandNotFound, false};
}

}  // namespace minishell
