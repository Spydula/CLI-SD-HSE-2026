#include <gtest/gtest.h>
#include <sstream>
#include "shell.hpp"

using namespace minishell;

TEST(Expansion, SimpleVariableExpansion) {
    Shell shell;
    shell.env().set("X", "hello");

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo $X", out, err);

    EXPECT_EQ(r.exitCode, 0);
    EXPECT_FALSE(r.shouldExit);
    EXPECT_EQ(out.str(), "hello\n");
}

TEST(Expansion, NoExpansionInSingleQuotes) {
    Shell shell;
    shell.env().set("X", "hello");

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo '$X'", out, err);

    EXPECT_EQ(r.exitCode, 0);
    EXPECT_EQ(out.str(), "$X\n");
}

TEST(Expansion, ExpansionInDoubleQuotes) {
    Shell shell;
    shell.env().set("X", "hello world");

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo \"$X\"", out, err);

    EXPECT_EQ(r.exitCode, 0);
    EXPECT_EQ(out.str(), "hello world\n");
}

TEST(Expansion, MissingVariableExpandsToEmpty) {
    Shell shell;

    std::stringstream out, err;
    ExecResult r = shell.executeLine("echo $UNKNOWN", out, err);

    EXPECT_EQ(r.exitCode, 0);
    EXPECT_EQ(out.str(), "\n");
}
