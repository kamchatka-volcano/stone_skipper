#include "assert_exception.h"
#include "errors.h"
#include <utils.h>
#include <gtest/gtest.h>
#include <functional>

TEST(Utils, SplitCommand)
{
    auto parts = stone_skipper::splitCommand("command -param \"hello world\"");
    ASSERT_EQ(parts, (std::vector<std::string>{"command", "-param", "hello world"}));
}

TEST(Utils, SplitCommand2)
{
    auto parts = stone_skipper::splitCommand("command -param \"hello world\" -param2 \"hello world 2\"");
    ASSERT_EQ(parts, (std::vector<std::string>{"command", "-param", "hello world", "-param2", "hello world 2"}));
}

TEST(Utils, SplitCommand3)
{
    auto parts = stone_skipper::splitCommand("command -param=\"hello world\" -param2=\"hello world 2\"");
    ASSERT_EQ(parts, (std::vector<std::string>{"command", "-param=hello world", "-param2=hello world 2"}));
}

TEST(Utils, SplitCommand4)
{
    auto parts = stone_skipper::splitCommand("\"hello world\" command");
    ASSERT_EQ(parts, (std::vector<std::string>{"hello world", "command"}));
}

TEST(Utils, SplitCommand5)
{
    auto parts = stone_skipper::splitCommand("\"  hello world  \" command");
    ASSERT_EQ(parts, (std::vector<std::string>{"  hello world  ", "command"}));
}

TEST(Utils, SplitCommand6)
{
    auto parts = stone_skipper::splitCommand("\" hello world \" --param2 \"hello world 2\" -param3=\"hello world 3\"");
    ASSERT_EQ(parts, (std::vector<std::string>{" hello world ", "--param2", "hello world 2", "-param3=hello world 3"}));
}

TEST(Utils, SplitCommand7)
{
    auto parts = stone_skipper::splitCommand("' hello world ' --param2 'hello world 2' -param3='hello world 3'");
    ASSERT_EQ(parts, (std::vector<std::string>{" hello world ", "--param2", "hello world 2", "-param3=hello world 3"}));
}

TEST(Utils, SplitCommand8)
{
    auto parts = stone_skipper::splitCommand("` hello world ` --param2 `hello world 2` -param3=`hello world 3`");
    ASSERT_EQ(parts, (std::vector<std::string>{" hello world ", "--param2", "hello world 2", "-param3=hello world 3"}));
}

TEST(Utils, SplitCommand9)
{
    auto parts = stone_skipper::splitCommand("-param=\"hello\" hello_world -param2=world");
    ASSERT_EQ(parts, (std::vector<std::string>{"-param=hello", "hello_world", "-param2=world"}));
}

TEST(Utils, SplitCommandNoWhitespace)
{
    auto parts = stone_skipper::splitCommand("command");
    ASSERT_EQ(parts, (std::vector<std::string>{"command"}));
}

TEST(Utils, SplitCommandEmpty)
{
    auto parts = stone_skipper::splitCommand("");
    ASSERT_EQ(parts, (std::vector<std::string>{}));
}

TEST(Utils, SplitCommandUnclosedString)
{
    assert_exception<stone_skipper::Error>(
            []
            {
                [[maybe_unused]] auto parts = stone_skipper::splitCommand("command -param \"hello world");
            },
            [](const auto& e)
            {
                ASSERT_EQ(
                        std::string{e.what()},
                        "Command 'command -param \"hello world' has an unclosed quotation mark");
            });
}

TEST(Utils, SplitCommandUnclosedString2)
{
    assert_exception<stone_skipper::Error>(
            []
            {
                [[maybe_unused]] auto parts = stone_skipper::splitCommand("command -param \"");
            },
            [](const auto& e)
            {
                ASSERT_EQ(std::string{e.what()}, "Command 'command -param \"' has an unclosed quotation mark");
            });
}
