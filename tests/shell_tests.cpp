#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "shell.hpp"

class ShellTest : public ::testing::Test {
protected:
    minishell::Shell shell;
    std::stringstream out;
    std::stringstream err;

    // очистить стримы после каждого теста
    void TearDown() override {
        out.str("");
        out.clear();
        err.str("");
        err.clear();
    }
};

// Тест на парсинг токенов

TEST_F(ShellTest, TokenizerQuotes) {
    auto args = minishell::Tokenizer::tokenize("echo \"hello world\" 'single quote'");
    ASSERT_EQ(args.size(), 3);
    EXPECT_EQ(args[1], "hello world");
    EXPECT_EQ(args[2], "single quote");
}

TEST_F(ShellTest, TokenizerUnterminatedQuote) {
    EXPECT_THROW(minishell::Tokenizer::tokenize("echo \"unfinished"), std::runtime_error);
}

// тесты на окружение

TEST_F(ShellTest, EnvironmentSetGet) {
    shell.env().set("MY_VAR", "123");
    auto val = shell.env().get("MY_VAR");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "123");
}

TEST_F(ShellTest, AssignmentInShell) {
    shell.executeLine("K=V", out, err);
    EXPECT_EQ(shell.env().get("K"), "V");
}

// тесты на команды

TEST_F(ShellTest, BuiltinEcho) {
    shell.executeLine("echo hello    world", out, err);
    EXPECT_EQ(out.str(), "hello world\n");
}

TEST_F(ShellTest, BuiltinPwd) {
    shell.executeLine("pwd", out, err);
    EXPECT_FALSE(out.str().empty());
    EXPECT_TRUE(std::filesystem::exists(out.str().substr(0, out.str().size() - 1)));
}

TEST_F(ShellTest, BuiltinExit) {
    minishell::ExecResult res = shell.executeLine("exit", out, err);
    EXPECT_TRUE(res.shouldExit);
    EXPECT_EQ(res.exitCode, 0);
}

class ShellFileTest : public ShellTest {
protected:
    // создаем временный файл для cat и wc
    const std::string testFile = "test_data.txt";
    const std::string content = "line1\nline2 word\n";

    void SetUp() override {
        std::ofstream f(testFile);
        f << content;
    }

    void TearDown() override {
        ShellTest::TearDown();
        std::filesystem::remove(testFile);
    }
};

TEST_F(ShellFileTest, BuiltinCat) {
    shell.executeLine("cat " + testFile, out, err);
    EXPECT_EQ(out.str(), content);
}

TEST_F(ShellFileTest, BuiltinWc) {
    shell.executeLine("wc " + testFile, out, err);
    // в файле 2 строки, 3 слова
    // line1\n (6 байт)
    // line2 word\n (11 байт)
    // всего 17 байт
    EXPECT_EQ(out.str(), "2 3 17\n");
}

TEST_F(ShellFileTest, BuiltinFileNotFound) {
    shell.executeLine("cat non_existent_file.txt", out, err);
    EXPECT_FALSE(err.str().empty());
    EXPECT_EQ(out.str(), "");
}

// тестируем внешние программы

TEST_F(ShellTest, ExternalCommandNotFound) {
    minishell::ExecResult res = shell.executeLine("this_command_should_not_exist_12345", out, err);
    EXPECT_EQ(res.exitCode, 127);
    EXPECT_FALSE(err.str().empty());
}

// выполняем exit 42 и проверяем
TEST_F(ShellTest, ExternalCommandExitCodeCapture) {
    minishell::ExecResult res = shell.executeLine("sh -c \"exit 42\"", out, err);
    EXPECT_EQ(res.exitCode, 42);
    EXPECT_FALSE(res.shouldExit);
}

// выводим версию cmake ожидаем что команда успешно выполнится
TEST_F(ShellTest, ExternalCommandExecution) {
    minishell::ExecResult res = shell.executeLine("cmake --version", out, err);
    EXPECT_NE(res.exitCode, 127);
    EXPECT_EQ(res.exitCode, 0);
}