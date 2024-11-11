#include "urlencode.h"

#include <boost/url.hpp>
namespace urls = boost::urls;

#include <sstream>
#include <iostream>

std::string UrlEncode(std::string_view str) {
    static const urls::encoding_opts opt(true, true);
    return urls::encode(str, urls::unreserved_chars, opt);
}
