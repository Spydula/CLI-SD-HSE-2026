#include <gtest/gtest.h>
#include <sstream>
#include "shell.hpp"

class ExpansionTest : public ::testing::Test {
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

// тесты на подстановки переменных окружения

TEST_F(ExpansionTest, SimpleVariableExpansion) {
    shell.env().set("X", "hello");

    minishell::ExecResult res = shell.executeLine("echo $X", out, err);

    EXPECT_EQ(res.exitCode, 0);
    EXPECT_FALSE(res.shouldExit);
    EXPECT_EQ(out.str(), "hello\n");
}

TEST_F(ExpansionTest, NoExpansionInSingleQuotes) {
    shell.env().set("X", "hello");

    minishell::ExecResult res = shell.executeLine("echo '$X'", out, err);

    EXPECT_EQ(res.exitCode, 0);
    EXPECT_FALSE(res.shouldExit);
    EXPECT_EQ(out.str(), "$X\n");
}

TEST_F(ExpansionTest, ExpansionInDoubleQuotes) {
    shell.env().set("X", "hello world");

    minishell::ExecResult res = shell.executeLine("echo \"$X\"", out, err);

    EXPECT_EQ(res.exitCode, 0);
    EXPECT_FALSE(res.shouldExit);
    EXPECT_EQ(out.str(), "hello world\n");
}

TEST_F(ExpansionTest, MissingVariableExpandsToEmpty) {
    minishell::ExecResult res = shell.executeLine("echo $UNKNOWN", out, err);

    EXPECT_EQ(res.exitCode, 0);
    EXPECT_FALSE(res.shouldExit);
    EXPECT_EQ(out.str(), "\n");
}