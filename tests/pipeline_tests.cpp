#include <gtest/gtest.h>
#include <sstream>
#include "shell.hpp"

class PipelineTest : public ::testing::Test {
protected:
    minishell::Shell shell;
    std::stringstream out;
    std::stringstream err;

    void TearDown() override {
        out.str("");
        out.clear();
        err.str("");
        err.clear();
    }
};

// тесты на пайплайны

TEST_F(PipelineTest, EchoToWc) {
    minishell::ExecResult res = shell.executeLine("echo 123 | wc", out, err);

    EXPECT_EQ(res.exitCode, 0);
    EXPECT_FALSE(res.shouldExit);

    // wc должен выдать: 1 строка, 1 слово, >=4 байт
    EXPECT_NE(out.str().find("1 1"), std::string::npos);
}

TEST_F(PipelineTest, MultipleStages) {
    minishell::ExecResult res = shell.executeLine("echo hello | cat | wc", out, err);

    EXPECT_EQ(res.exitCode, 0);
    EXPECT_FALSE(res.shouldExit);

    EXPECT_NE(out.str().find("1 1"), std::string::npos);
}

TEST_F(PipelineTest, ExitInsidePipelineDoesNotTerminateShell) {
    minishell::ExecResult res = shell.executeLine("echo 1 | exit", out, err);

    EXPECT_FALSE(res.shouldExit);
}

TEST_F(PipelineTest, SyntaxErrorEmptyStage) {
    minishell::ExecResult res = shell.executeLine("echo 1 || wc", out, err);

    EXPECT_EQ(res.exitCode, 2);
    EXPECT_FALSE(res.shouldExit);
}

TEST_F(PipelineTest, LeadingPipeError) {
    minishell::ExecResult res = shell.executeLine("| echo", out, err);

    EXPECT_EQ(res.exitCode, 2);
    EXPECT_FALSE(res.shouldExit);
}