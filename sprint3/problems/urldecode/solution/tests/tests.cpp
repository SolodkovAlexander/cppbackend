#define BOOST_TEST_MODULE leap year application tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(IsLeapYear_test) {
    using namespace std::literals;

    BOOST_REQUIRE_EQUAL(UrlDecode(""s), ""s);
    BOOST_REQUIRE_EQUAL(UrlDecode("HelloWorld"s), "HelloWorld"s);
    BOOST_REQUIRE_EQUAL(UrlDecode("a%3A"s), "a:"s);
    BOOST_REQUIRE_EQUAL(UrlDecode("a%3a"s), "a:"s);
    BOOST_REQUIRE_THROW(UrlDecode("33113%"s), std::invalid_argument);
    BOOST_REQUIRE_THROW(UrlDecode("%%%3%%1%2"s), std::invalid_argument);
    BOOST_REQUIRE_EQUAL(UrlDecode("a+b"s), "a+b"s);
}
