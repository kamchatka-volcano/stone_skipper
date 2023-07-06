#include "utils.h"
#include "errors.h"
#include <fmt/format.h>
#include <sfun/string_utils.h>
#include <gsl/util>

namespace stone_skipper {

namespace {

class StringStream {
public:
    explicit StringStream(const std::string& str)
        : stream_{str}
    {
    }

    std::optional<char> read()
    {
        auto ch = char{};
        if (!stream_.get(ch))
            return std::nullopt;
        return ch;
    }

    std::optional<char> peek()
    {
        auto ch = char{};
        auto pos = stream_.tellg();
        auto restorePosition = gsl::finally(
                [&]
                {
                    stream_.seekg(pos);
                });

        if (!stream_.get(ch)) {
            stream_.clear();
            return std::nullopt;
        }
        return ch;
    }

    void skip()
    {
        [[maybe_unused]] auto res = read();
    }

    bool atEnd()
    {
        return !peek().has_value();
    }

    std::string readUntil(std::function<bool(char ch)> pred)
    {
        auto discard = false;
        return readUntil(pred, discard);
    }

    std::string readUntil(std::function<bool(char ch)> pred, bool& stoppedAtEnd)
    {
        auto result = std::string();
        while (!atEnd()) {
            auto ch = peek().value();
            if (pred(ch)) {
                stoppedAtEnd = false;
                return result;
            }
            result.push_back(ch);
            skip();
        }
        stoppedAtEnd = true;
        return result;
    }

private:
    std::stringstream stream_;
};

} //namespace

std::vector<std::string> splitCommand(const std::string& str)
{
    auto result = std::vector<std::string>{};
    auto stream = StringStream{str};
    auto prevCh = char{};
    while (!stream.atEnd()) {
        auto ch = stream.read().value();
        if (ch == '\"' || ch == '\'' || ch == '`') {
            auto stoppedAtEnd = false;
            auto quotedText = stream.readUntil(
                    [quotationMark = ch](char ch)
                    {
                        return ch == quotationMark;
                    },
                    stoppedAtEnd);
            if (stoppedAtEnd)
                throw Error{fmt::format("Command '{}' has an unclosed quotation mark", str)};

            if (!prevCh || sfun::isspace(prevCh))
                result.emplace_back(std::move(quotedText));
            else
                result.back() += quotedText;
            stream.skip();
        }
        else if (!sfun::isspace(ch)) {
            if (!prevCh || sfun::isspace(prevCh))
                result.emplace_back();
            result.back() += ch;
        }
        prevCh = ch;
    }
    return result;
}

} //namespace stone_skipper