#include "urldecode.h"

#include <charconv>
#include <stdexcept>

#include <boost/url.hpp>
namespace urls = boost::urls;

std::string UrlDecode(std::string_view str) {
  using namespace std::literals;

  urls::decode_view decoded_str;
  try {
    decoded_str = urls::decode_view(str);
  } catch (...) {
    throw std::invalid_argument("invalid URL"s);
  }
  
  std::string result(decoded_str.begin(), decoded_str.end());
  return result;
}
