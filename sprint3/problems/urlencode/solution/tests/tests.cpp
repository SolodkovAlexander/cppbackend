#include <gtest/gtest.h>

#include <algorithm>

#include "../src/urlencode.h"

using namespace std::literals;

TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode("hello"sv), "hello"s);
}

TEST(UrlEncodeTestSuite, TestsByTask) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
    EXPECT_EQ(UrlEncode("abcd"sv), "abcd"s);
    EXPECT_EQ(UrlEncode("abcd()"sv), "abcd%28%29"s);
    EXPECT_EQ(UrlEncode("abcd*"sv), "abcd%2a");
    EXPECT_EQ(UrlEncode("ab cd"sv), "ab+cd"s);
    EXPECT_EQ(UrlEncode("abcd"s + char(5)), "abcd%05"s);
    EXPECT_EQ(UrlEncode("abcdЫ"s), "abcd%d0%ab"s);
}
