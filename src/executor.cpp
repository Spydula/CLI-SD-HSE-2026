#include "executor.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstddef>
#include <iostream>
#include <system_error>
#include <vector>

namespace minishell {
namespace {

constexpr int kExitCommandNotFound = 127;
constexpr int kExitSignalBase = 128;
constexpr std::size_t kIoBufferSize = 4096;

auto errnoMessage() -> std::string {
    return std::system_error(errno, std::generic_category()).what();
}

auto makePipe(std::array<int, 2> &pipeFds) -> bool {
    pipeFds = {-1, -1};
    return ::pipe(pipeFds.data()) == 0;
}

auto closeIfOpen(int &fileDescriptor) -> void {
    if (fileDescriptor != -1) {
        ::close(fileDescriptor);
        fileDescriptor = -1;
    }
}

auto closePipe(std::array<int, 2> &pipeFds) -> void {
    closeIfOpen(pipeFds[0]);
    closeIfOpen(pipeFds[1]);
}

auto drainFdToStream(int fileDescriptor, std::ostream &outputStream) -> void {
    std::array<char, kIoBufferSize> buffer{};
    while (true) {
        const ssize_t bytesRead = ::read(fileDescriptor, buffer.data(), buffer.size());
        if (bytesRead <= 0) {
            break;
        }
        outputStream.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
    }
}

auto statusToExitCode(int status) -> int {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return kExitSignalBase + WTERMSIG(status);
    }
    return kExitCommandNotFound;
}

auto waitAll(const std::vector<pid_t> &processIds, std::ostream &err) -> int {
    int lastExitCode = 0;
    for (std::size_t processIndex = 0; processIndex < processIds.size(); ++processIndex) {
        int status = 0;
        if (::waitpid(processIds[processIndex], &status, 0) < 0) {
            err << "waitpid failed: " << errnoMessage() << "\n";
            if (processIndex + 1U == processIds.size()) {
                lastExitCode = kExitCommandNotFound;
            }
            continue;
        }
        if (processIndex + 1U == processIds.size()) {
            lastExitCode = statusToExitCode(status);
        }
    }
    return lastExitCode;
}

}  // namespace

[[noreturn]] auto Executor::execStage(const std::vector<std::string> &argv,
                                      Shell &shell,
                                      int inputFd,
                                      int outputFd,
                                      int errorFd) -> void {
    if (inputFd != STDIN_FILENO) {
        ::dup2(inputFd, STDIN_FILENO);
    }
    if (outputFd != STDOUT_FILENO) {
        ::dup2(outputFd, STDOUT_FILENO);
    }
    if (errorFd != STDERR_FILENO) {
        ::dup2(errorFd, STDERR_FILENO);
    }

    const ExecResult stageResult = shell.executeArgv(argv, IoStreams{std::cout, std::cerr});
    std::cout.flush();
    std::cerr.flush();
    _exit(stageResult.exitCode);
}

auto Executor::execute(const std::vector<std::vector<std::string>> &stages,
                       Shell &shell,
                       std::ostream &out,
                       std::ostream &err) -> ExecResult {
    if (stages.empty()) {
        return ExecResult{0, false};
    }
    if (stages.size() == 1U) {
        return shell.executeArgv(stages.front(), IoStreams{out, err});
    }

    std::array<int, 2> errorPipe{};
    std::array<int, 2> outputPipe{};
    if (!makePipe(errorPipe)) {
        err << "pipe: " << errnoMessage() << "\n";
        return ExecResult{kExitCommandNotFound, false};
    }
    if (!makePipe(outputPipe)) {
        closePipe(errorPipe);
        err << "pipe: " << errnoMessage() << "\n";
        return ExecResult{kExitCommandNotFound, false};
    }

    std::vector<pid_t> processIds;
    processIds.reserve(stages.size());

    int previousReadFd = -1;

    for (std::size_t stageIndex = 0; stageIndex < stages.size(); ++stageIndex) {
        const bool isLastStage = (stageIndex + 1U == stages.size());

        std::array<int, 2> nextPipe{};
        if (!isLastStage) {
            if (!makePipe(nextPipe)) {
                closeIfOpen(previousReadFd);
                closePipe(errorPipe);
                closePipe(outputPipe);
                err << "pipe: " << errnoMessage() << "\n";
                return ExecResult{kExitCommandNotFound, false};
            }
        }

        const pid_t processId = ::fork();
        if (processId < 0) {
            closeIfOpen(previousReadFd);
            if (!isLastStage) {
                closePipe(nextPipe);
            }
            closePipe(errorPipe);
            closePipe(outputPipe);
            err << "fork failed: " << errnoMessage() << "\n";
            return ExecResult{kExitCommandNotFound, false};
        }

        if (processId == 0) {
            closeIfOpen(errorPipe[0]);
            closeIfOpen(outputPipe[0]);

            const int inputFd = (previousReadFd != -1) ? previousReadFd : STDIN_FILENO;
            const int outputFd = isLastStage ? outputPipe[1] : nextPipe[1];
            const int errorFd = errorPipe[1];

            if (!isLastStage) {
                closeIfOpen(nextPipe[0]);
            }

            Executor::execStage(stages[stageIndex], shell, inputFd, outputFd, errorFd);
        }

        processIds.push_back(processId);
        closeIfOpen(previousReadFd);

        if (!isLastStage) {
            closeIfOpen(nextPipe[1]);
            previousReadFd = nextPipe[0];
        }
    }

    closeIfOpen(errorPipe[1]);
    closeIfOpen(outputPipe[1]);

    drainFdToStream(outputPipe[0], out);
    drainFdToStream(errorPipe[0], err);

    closeIfOpen(outputPipe[0]);
    closeIfOpen(errorPipe[0]);

    const int lastExitCode = waitAll(processIds, err);
    return ExecResult{lastExitCode, false};
}

}  // namespace minishell
