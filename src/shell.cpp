#include "shell.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#if defined(__APPLE__)
#include <crt_externs.h>
#endif

#if !defined(__APPLE__)
extern "C" char **environ;
#endif

namespace minishell {

// ---------------- Environment ----------------

void Environment::set(std::string name, std::string value) {
    vars_[std::move(name)] = std::move(value);
}

std::optional<std::string> Environment::get(std::string_view name) const {
    auto it = vars_.find(std::string(name));
    if (it == vars_.end())
        return std::nullopt;
    return it->second;
}

const std::map<std::string, std::string> &Environment::snapshot() const {
    return vars_;
}

Environment Environment::fromProcessEnvironment() {
    Environment env;
    char **p = nullptr;

#if defined(__APPLE__)
    p = *_NSGetEnviron();
#else
    p = ::environ;
#endif

    for (; p && *p; ++p) {
        std::string entry(*p);
        auto pos = entry.find('=');
        if (pos == std::string::npos || pos == 0)
            continue;
        env.set(entry.substr(0, pos), entry.substr(pos + 1));
    }

    return env;
}

// ---------------- Tokenizer ----------------

std::vector<std::string> Tokenizer::tokenize(std::string_view line) {
    enum class State { Normal, InSingle, InDouble };

    State st = State::Normal;
    std::vector<std::string> argv;
    std::string cur;

    auto flush = [&]() {
        if (!cur.empty()) {
            argv.push_back(cur);
            cur.clear();
        }
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        switch (st) {
            case State::Normal:
                if (std::isspace(static_cast<unsigned char>(c))) {
                    flush();
                } else if (c == '\'') {
                    st = State::InSingle;
                } else if (c == '"') {
                    st = State::InDouble;
                } else {
                    cur.push_back(c);
                }
                break;

            case State::InSingle:
                if (c == '\'') {
                    st = State::Normal;
                } else {
                    cur.push_back(c);
                }
                break;

            case State::InDouble:
                if (c == '"') {
                    st = State::Normal;
                } else {
                    cur.push_back(c);
                }
                break;
        }
    }

    if (st != State::Normal) {
        throw std::runtime_error("unterminated quote");
    }

    flush();
    return argv;
}

std::vector<std::string> Tokenizer::tokenizeWithPipesAndExpansion(std::string_view line, const Environment &env) {
    enum class State { Normal, InSingle, InDouble };

    State st = State::Normal;
    std::vector<std::string> argv;
    std::string cur;

    auto flush = [&]() {
        if (!cur.empty()) {
            argv.push_back(cur);
            cur.clear();
        }
    };

    auto expandVarAt = [&](size_t &i) {
        size_t j = i + 1;
        if (j >= line.size()) {
            cur.push_back('$');
            return;
        }

        auto isAlphaUnd = [](unsigned char ch) { return std::isalpha(ch) || ch == '_'; };
        auto isAlnumUnd = [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; };

        if (!isAlphaUnd(static_cast<unsigned char>(line[j]))) {
            cur.push_back('$');
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
            cur += *v;
        }

        i = j - 1;
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        switch (st) {
            case State::Normal:
                if (c == '|') {
                    flush();
                    argv.emplace_back("|");
                } else if (std::isspace(static_cast<unsigned char>(c))) {
                    flush();
                } else if (c == '\'') {
                    st = State::InSingle;
                } else if (c == '"') {
                    st = State::InDouble;
                } else if (c == '$') {
                    expandVarAt(i);
                } else {
                    cur.push_back(c);
                }
                break;

            case State::InSingle:
                if (c == '\'') {
                    st = State::Normal;
                } else {
                    cur.push_back(c);
                }
                break;

            case State::InDouble:
                if (c == '"') {
                    st = State::Normal;
                } else if (c == '$') {
                    expandVarAt(i);
                } else {
                    cur.push_back(c);
                }
                break;
        }
    }

    if (st != State::Normal) {
        throw std::runtime_error("unterminated quote");
    }

    flush();
    return argv;
}

// ---------------- Helpers ----------------

static bool isValidVarName(std::string_view name) {
    if (name.empty())
        return false;

    auto isAlphaUnd = [](unsigned char ch) { return std::isalpha(ch) || ch == '_'; };
    auto isAlnumUnd = [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; };

    if (!isAlphaUnd(static_cast<unsigned char>(name[0])))
        return false;
    for (size_t i = 1; i < name.size(); ++i) {
        if (!isAlnumUnd(static_cast<unsigned char>(name[i])))
            return false;
    }
    return true;
}

static std::string errnoMessage() {
    return std::system_error(errno, std::generic_category()).what();
}

// ---------------- Shell ----------------

Shell::Shell() : env_(Environment::fromProcessEnvironment()) {
}

int Shell::run(std::istream &in, std::ostream &out, std::ostream &err) {
    std::string line;
    while (true) {
        if (!std::getline(in, line))
            break;

        ExecResult r = executeLine(line, out, err);
        if (r.shouldExit)
            return r.exitCode;
    }
    return 0;
}

ExecResult Shell::executeLine(std::string_view line, std::ostream &out, std::ostream &err) {
    std::vector<std::string> tokens;
    try {
        tokens = Tokenizer::tokenizeWithPipesAndExpansion(line, env_);
    } catch (const std::exception &e) {
        err << "parse error: " << e.what() << "\n";
        return ExecResult{2, false};
    }

    if (tokens.empty())
        return ExecResult{0, false};

    std::vector<std::vector<std::string>> stages;
    std::vector<std::string> cur;
    for (const auto &t : tokens) {
        if (t == "|") {
            if (cur.empty()) {
                err << "parse error: empty command in pipeline\n";
                return ExecResult{2, false};
            }
            stages.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(t);
        }
    }
    if (!cur.empty())
        stages.push_back(cur);
    if (stages.empty())
        return ExecResult{0, false};

    if (stages.size() == 1) {
        const auto &argv = stages[0];
        if (tryHandleAssignmentsOnly(argv, err)) {
            return ExecResult{0, false};
        }
        return executeArgv(argv, out, err);
    }

    int errPipe[2] = {-1, -1};
    if (::pipe(errPipe) != 0) {
        err << "pipe: " << errnoMessage() << "\n";
        return ExecResult{127, false};
    }

    int outPipe[2] = {-1, -1};
    if (::pipe(outPipe) != 0) {
        ::close(errPipe[0]);
        ::close(errPipe[1]);
        err << "pipe: " << errnoMessage() << "\n";
        return ExecResult{127, false};
    }

    std::vector<pid_t> pids;
    pids.reserve(stages.size());
    int prevRead = -1;

    for (size_t i = 0; i < stages.size(); ++i) {
        int nextPipe[2] = {-1, -1};
        const bool isLast = (i + 1 == stages.size());
        if (!isLast) {
            if (::pipe(nextPipe) != 0) {
                err << "pipe: " << errnoMessage() << "\n";
                if (prevRead != -1)
                    ::close(prevRead);
                ::close(errPipe[0]);
                ::close(errPipe[1]);
                ::close(outPipe[0]);
                ::close(outPipe[1]);
                return ExecResult{127, false};
            }
        }

        pid_t pid = ::fork();
        if (pid < 0) {
            err << "fork failed: " << errnoMessage() << "\n";
            if (prevRead != -1)
                ::close(prevRead);
            if (!isLast) {
                ::close(nextPipe[0]);
                ::close(nextPipe[1]);
            }
            ::close(errPipe[0]);
            ::close(errPipe[1]);
            ::close(outPipe[0]);
            ::close(outPipe[1]);
            return ExecResult{127, false};
        }

        if (pid == 0) {
            ::dup2(errPipe[1], STDERR_FILENO);

            if (prevRead != -1) {
                ::dup2(prevRead, STDIN_FILENO);
            }

            if (isLast) {
                ::dup2(outPipe[1], STDOUT_FILENO);
            } else {
                ::dup2(nextPipe[1], STDOUT_FILENO);
            }

            if (prevRead != -1)
                ::close(prevRead);
            if (!isLast) {
                ::close(nextPipe[0]);
                ::close(nextPipe[1]);
            }
            ::close(errPipe[0]);
            ::close(errPipe[1]);
            ::close(outPipe[0]);
            ::close(outPipe[1]);

            ExecResult r = executeArgv(stages[i], std::cout, std::cerr);
            _exit(r.exitCode);
        }

        pids.push_back(pid);
        if (prevRead != -1)
            ::close(prevRead);

        if (!isLast) {
            ::close(nextPipe[1]);
            prevRead = nextPipe[0];
        } else {
            prevRead = -1;
        }
    }

    ::close(errPipe[1]);
    ::close(outPipe[1]);

    auto drainFdToStream = [](int fd, std::ostream &os) {
        char buf[4096];
        while (true) {
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n <= 0)
                break;
            os.write(buf, static_cast<std::streamsize>(n));
        }
    };

    drainFdToStream(outPipe[0], out);
    drainFdToStream(errPipe[0], err);

    ::close(outPipe[0]);
    ::close(errPipe[0]);

    int lastExitCode = 0;
    for (size_t i = 0; i < pids.size(); ++i) {
        int status = 0;
        if (::waitpid(pids[i], &status, 0) < 0) {
            err << "waitpid failed: " << errnoMessage() << "\n";
            lastExitCode = 127;
            continue;
        }
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (i + 1 == pids.size())
                lastExitCode = code;
        } else if (WIFSIGNALED(status)) {
            int code = 128 + WTERMSIG(status);
            if (i + 1 == pids.size())
                lastExitCode = code;
        } else {
            if (i + 1 == pids.size())
                lastExitCode = 127;
        }
    }

    return ExecResult{lastExitCode, false};
}

Environment &Shell::env() {
    return env_;
}
const Environment &Shell::env() const {
    return env_;
}

bool Shell::tryHandleAssignmentsOnly(const std::vector<std::string> &argv, std::ostream &err) {
    for (const auto &a : argv) {
        auto pos = a.find('=');
        if (pos == std::string::npos || pos == 0)
            return false;

        std::string_view name(a.data(), pos);
        if (!isValidVarName(name)) {
            err << "assignment error: invalid variable name: " << std::string(name) << "\n";
            return false;
        }
    }

    for (const auto &a : argv) {
        auto pos = a.find('=');
        env_.set(a.substr(0, pos), a.substr(pos + 1));
    }

    return true;
}

ExecResult Shell::executeArgv(const std::vector<std::string> &argv,
                              std::ostream &out,
                              std::ostream &err) {
    const std::string &cmd = argv[0];

    if (cmd == "cat")
        return builtinCat(argv, out, err);
    if (cmd == "echo")
        return builtinEcho(argv, out, err);
    if (cmd == "wc")
        return builtinWc(argv, out, err);
    if (cmd == "pwd")
        return builtinPwd(out, err);
    if (cmd == "exit")
        return builtinExit();

    return runExternal(argv, err);
}

// ---------------- Builtins ----------------

ExecResult Shell::builtinExit() {
    return ExecResult{0, true};
}

ExecResult Shell::builtinPwd(std::ostream &out, std::ostream &err) {
    try {
        out << std::filesystem::current_path().string() << "\n";
        return ExecResult{0, false};
    } catch (const std::exception &e) {
        err << "pwd: " << e.what() << "\n";
        return ExecResult{1, false};
    }
}

ExecResult Shell::builtinEcho(const std::vector<std::string> &argv,
                              std::ostream &out,
                              std::ostream &err) {  // cppcheck-suppress constParameterReference
    (void)err;
    for (size_t i = 1; i < argv.size(); ++i) {
        if (i > 1)
            out << ' ';
        out << argv[i];
    }
    out << "\n";
    return ExecResult{0, false};
}

ExecResult Shell::builtinCat(const std::vector<std::string> &argv,
                             std::ostream &out,
                             std::ostream &err) {
    if (argv.size() == 1) {
        out << std::cin.rdbuf();
        return ExecResult{0, false};
    }

    if (argv.size() != 2) {
        err << "cat: usage: cat <FILE>\n";
        return ExecResult{2, false};
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        err << "cat: cannot open file: " << argv[1] << "\n";
        return ExecResult{1, false};
    }

    out << file.rdbuf();
    return ExecResult{0, false};
}

ExecResult Shell::builtinWc(const std::vector<std::string> &argv,
                            std::ostream &out,
                            std::ostream &err) {
    std::istream *in = &std::cin;
    std::ifstream file;

    if (argv.size() == 2) {
        file.open(argv[1], std::ios::binary);
        if (!file) {
            err << "wc: cannot open file: " << argv[1] << "\n";
            return ExecResult{1, false};
        }
        in = &file;
    } else if (argv.size() != 1) {
        err << "wc: usage: wc <FILE>\n";
        return ExecResult{2, false};
    }

    std::uint64_t lines = 0;
    std::uint64_t words = 0;
    std::uint64_t bytes = 0;

    bool inWord = false;
    char ch;
    while (in->get(ch)) {
        ++bytes;
        if (ch == '\n')
            ++lines;

        if (std::isspace(static_cast<unsigned char>(ch))) {
            inWord = false;
        } else {
            if (!inWord) {
                ++words;
                inWord = true;
            }
        }
    }

    out << lines << " " << words << " " << bytes << "\n";
    return ExecResult{0, false};
}

// ---------------- External execution ----------------

static bool isExecutableFile(const std::filesystem::path &p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec) || ec)
        return false;
    if (!std::filesystem::is_regular_file(p, ec) || ec)
        return false;
    return ::access(p.c_str(), X_OK) == 0;
}

static std::optional<std::string> findInPath(const std::string &file, const Environment &env) {
    // Если в имени есть '/', считаем что это путь и не ищем по PATH.
    if (file.find('/') != std::string::npos) {
        if (isExecutableFile(file))
            return file;
        return std::nullopt;
    }

    // Берём PATH из нашего Environment, иначе из окружения процесса.
    std::string pathValue;
    if (auto v = env.get("PATH"); v.has_value()) {
        pathValue = *v;
    } else {
        const char *p = std::getenv("PATH");
        pathValue = p ? p : "";
    }

    std::stringstream ss(pathValue);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty())
            dir = ".";
        std::filesystem::path candidate = std::filesystem::path(dir) / file;
        if (isExecutableFile(candidate)) {
            return candidate.string();
        }
    }

    return std::nullopt;
}

ExecResult Shell::runExternal(const std::vector<std::string> &argv, std::ostream &err) {
    // 1) Находим полный путь к исполняемому файлу (по PATH).
    auto fullPathOpt = findInPath(argv[0], env_);
    if (!fullPathOpt.has_value()) {
        err << argv[0] << ": command not found\n";
        return ExecResult{127, false};
    }
    const std::string fullPath = *fullPathOpt;

    // 2) Формируем argv для execve: массив char* с nullptr в конце.
    std::vector<std::string> argsStorage = argv;
    argsStorage[0] = fullPath;
    std::vector<char *> args;
    args.reserve(argsStorage.size() + 1);
    // cppcheck-suppress constVariableReference
    for (auto &s : argsStorage)
        // cppcheck-suppress useStlAlgorithm
        args.push_back(s.data());
    args.push_back(nullptr);

    // 3) Формируем envp для execve из env_.
    std::vector<std::string> envStorage;
    envStorage.reserve(env_.snapshot().size());
    for (const auto &[k, v] : env_.snapshot()) {
        envStorage.push_back(k + "=" + v);
    }

    std::vector<char *> envp;
    envp.reserve(envStorage.size() + 1);
    // cppcheck-suppress constVariableReference
    for (auto &s : envStorage)
        // cppcheck-suppress useStlAlgorithm
        envp.push_back(s.data());
    envp.push_back(nullptr);

    // 4) fork + execve + waitpid
    pid_t pid = ::fork();
    if (pid < 0) {
        err << argv[0] << ": fork failed: " << errnoMessage() << "\n";
        return ExecResult{127, false};
    }

    if (pid == 0) {
        ::execve(args[0], args.data(), envp.data());
        // Если execve вернулся — ошибка запуска.
        _exit(127);
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        err << argv[0] << ": waitpid failed: " << errnoMessage() << "\n";
        return ExecResult{127, false};
    }

    if (WIFEXITED(status)) {
        return ExecResult{WEXITSTATUS(status), false};
    }
    if (WIFSIGNALED(status)) {
        return ExecResult{128 + WTERMSIG(status), false};
    }

    return ExecResult{127, false};
}

}  // namespace minishell
