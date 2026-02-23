#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "../include/grep.hpp"

using namespace minishell;

TEST(GrepParseArgsTest, ValidPatternOnly) {
    std::vector<std::string> argv = {"grep", "pattern"};
    auto options = Grep::parseArgs(argv);
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->pattern, "pattern");
    EXPECT_TRUE(options->files.empty());
    EXPECT_FALSE(options->caseInsensitive);
    EXPECT_FALSE(options->wholeWord);
    EXPECT_EQ(options->afterLines, 0);
}

TEST(GrepParseArgsTest, WithFile) {
    std::vector<std::string> argv = {"grep", "pattern", "file.txt"};
    auto options = Grep::parseArgs(argv);
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->pattern, "pattern");
    ASSERT_EQ(options->files.size(), 1);
    EXPECT_EQ(options->files[0], "file.txt");
}

TEST(GrepParseArgsTest, WithCaseInsensitiveFlag) {
    std::vector<std::string> argv = {"grep", "-i", "pattern"};
    auto options = Grep::parseArgs(argv);
    ASSERT_TRUE(options.has_value());
    EXPECT_TRUE(options->caseInsensitive);
    EXPECT_EQ(options->pattern, "pattern");
}

TEST(GrepParseArgsTest, WithWholeWordFlag) {
    std::vector<std::string> argv = {"grep", "-w", "pattern"};
    auto options = Grep::parseArgs(argv);
    ASSERT_TRUE(options.has_value());
    EXPECT_TRUE(options->wholeWord);
    EXPECT_EQ(options->pattern, "pattern");
}

TEST(GrepParseArgsTest, WithAfterLinesFlag) {
    std::vector<std::string> argv = {"grep", "-A", "5", "pattern"};
    auto options = Grep::parseArgs(argv);
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->afterLines, 5);
    EXPECT_EQ(options->pattern, "pattern");
}

TEST(GrepParseArgsTest, WithMultipleFlags) {
    std::vector<std::string> argv = {"grep", "-i", "-w", "-A", "3", "pattern", "file1.txt", "file2.txt"};
    auto options = Grep::parseArgs(argv);
    ASSERT_TRUE(options.has_value());
    EXPECT_TRUE(options->caseInsensitive);
    EXPECT_TRUE(options->wholeWord);
    EXPECT_EQ(options->afterLines, 3);
    EXPECT_EQ(options->pattern, "pattern");
    ASSERT_EQ(options->files.size(), 2);
    EXPECT_EQ(options->files[0], "file1.txt");
    EXPECT_EQ(options->files[1], "file2.txt");
}

TEST(GrepParseArgsTest, InvalidArgs) {
    std::vector<std::string> argv = {"grep", "-A", "pattern"};
    auto options = Grep::parseArgs(argv);
    EXPECT_FALSE(options.has_value());
}

TEST(GrepParseArgsTest, InvalidFlag) {
    std::vector<std::string> argv = {"grep", "-x", "pattern"};
    auto options = Grep::parseArgs(argv);
    EXPECT_FALSE(options.has_value());
}

TEST(GrepWordBoundaryTest, WordBoundaries) {
    EXPECT_TRUE(Grep::isWordBoundary(' '));
    EXPECT_TRUE(Grep::isWordBoundary('\t'));
    EXPECT_TRUE(Grep::isWordBoundary('\n'));
    EXPECT_TRUE(Grep::isWordBoundary('.'));
    EXPECT_TRUE(Grep::isWordBoundary(','));
    EXPECT_TRUE(Grep::isWordBoundary('!'));
    EXPECT_TRUE(Grep::isWordBoundary('?'));
    EXPECT_FALSE(Grep::isWordBoundary('_'));
    EXPECT_FALSE(Grep::isWordBoundary('a'));
    EXPECT_FALSE(Grep::isWordBoundary('Z'));
    EXPECT_FALSE(Grep::isWordBoundary('0'));
}

TEST(GrepSearchWordTest, ExactMatch) {
    GrepOptions options;
    options.pattern = "hello";
    options.wholeWord = true;

    EXPECT_TRUE(Grep::searchWord("hello world", "hello", options));
    EXPECT_TRUE(Grep::searchWord("say hello everyone", "hello", options));
    EXPECT_TRUE(Grep::searchWord("hello", "hello", options));

    EXPECT_FALSE(Grep::searchWord("helloWorld", "hello", options));
    EXPECT_FALSE(Grep::searchWord("say helloWorld", "hello", options));
}

TEST(GrepSearchWordTest, CaseInsensitive) {
    GrepOptions options;
    options.pattern = "HELLO";
    options.wholeWord = true;
    options.caseInsensitive = true;
    
    EXPECT_TRUE(Grep::searchWord("hello world", "HELLO", options));
    EXPECT_TRUE(Grep::searchWord("Say HeLLo Everyone", "hello", options));
}

TEST(GrepSearchLineTest, RegexSearch) {
    GrepOptions options;
    options.pattern = "hello";
    options.wholeWord = false;

    EXPECT_TRUE(Grep::searchLine("hello world", std::regex("hello"), options));
    EXPECT_TRUE(Grep::searchLine("say hello everyone", std::regex("hello*"), options));
    EXPECT_TRUE(Grep::searchLine("helloWorld", std::regex("hello*"), options));
}
