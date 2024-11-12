#include <catch2/catch_test_macros.hpp>

#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Tests by task", "[HtmlDecode]") {
    CHECK(HtmlDecode("hello"sv) == "hello"s);
    CHECK(HtmlDecode("hello&amp;"sv) == "hello&"s);
    CHECK(HtmlDecode(""sv) == ""s);
    CHECK(HtmlDecode("hello&aMp;"sv) == "hello&aMp;"s);
    CHECK(HtmlDecode("&amp;hello"sv) == "&hello"s);
    CHECK(HtmlDecode("hel&amp;lo"sv) == "hel&lo"s);
    CHECK(HtmlDecode("hello&am"sv) == "hello&am"s);
    CHECK(HtmlDecode("hello&amp"sv) == "hello&"s);
}
