#include "executor.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <system_error>
#include <vector>

namespace minishell {

static std::string errnoMessage() {
    return std::system_error(errno, std::generic_category()).what();
}

ExecResult Executor::execute(const std::vector<std::vector<std::string>> &stages,
                             Shell &shell,
                             std::ostream &out,
                             std::ostream &err) const {
    if (stages.empty()) {
        return ExecResult{0, false};
    }
    if (stages.size() == 1) {
        return shell.executeArgv(stages[0], out, err);
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
    for (std::size_t i = 0; i < stages.size(); ++i) {
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

            ExecResult r = shell.executeArgv(stages[i], std::cout, std::cerr);
            std::cout.flush();
            std::cerr.flush();
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
    for (std::size_t i = 0; i < pids.size(); ++i) {
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

}  // namespace minishell
