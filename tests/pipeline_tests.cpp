#include <gtest/gtest.h>
#include <sstream>
#include "shell.hpp"

using namespace minishell;

TEST(Pipeline, EchoToWc) {
    Shell shell;

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo 123 | wc", out, err);

    EXPECT_EQ(r.exitCode, 0);
    EXPECT_FALSE(r.shouldExit);

    // wc должен выдать: 1 строка, 1 слово, >=4 байт
    EXPECT_NE(out.str().find("1 1"), std::string::npos);
}

TEST(Pipeline, MultipleStages) {
    Shell shell;

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo hello | cat | wc", out, err);

    EXPECT_EQ(r.exitCode, 0);
    EXPECT_NE(out.str().find("1 1"), std::string::npos);
}

TEST(Pipeline, ExitInsidePipelineDoesNotTerminateShell) {
    Shell shell;

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo 1 | exit", out, err);

    EXPECT_FALSE(r.shouldExit);
}

TEST(Pipeline, SyntaxErrorEmptyStage) {
    Shell shell;

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo 1 || wc", out, err);

    EXPECT_EQ(r.exitCode, 2);
    EXPECT_FALSE(r.shouldExit);
}

TEST(Pipeline, LeadingPipeError) {
    Shell shell;

    std::stringstream out, err;
    ExecResult r = shell.executeLine("| echo", out, err);

    EXPECT_EQ(r.exitCode, 2);
}
