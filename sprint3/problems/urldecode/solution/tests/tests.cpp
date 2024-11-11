#define BOOST_TEST_MODULE urlencode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode(""sv) == ""s);
    BOOST_REQUIRE_EQUAL(UrlDecode(""sv), ""s);
    BOOST_REQUIRE_EQUAL(UrlDecode("HelloWorld"sv), "HelloWorld"s);
    BOOST_REQUIRE_EQUAL(UrlDecode("a%3A"sv), "a:"s);
    BOOST_REQUIRE_EQUAL(UrlDecode("a%3a"sv), "a:"s);
    BOOST_REQUIRE_THROW(UrlDecode("33113%"sv), std::invalid_argument);
    BOOST_REQUIRE_THROW(UrlDecode("%%%3%%1%2"sv), std::invalid_argument);
    BOOST_REQUIRE_EQUAL(UrlDecode("a+b"sv), "a+b"s);
}