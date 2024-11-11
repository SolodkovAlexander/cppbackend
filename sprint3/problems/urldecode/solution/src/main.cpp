#include "urldecode.h"

#include <iostream>
#include <string>

int main(int argc, const char* argv[]) {
    using namespace std::literals;

    std::string url;
    std::getline(std::cin, url);
    std::cout << UrlDecode(url);

    return 0;
}
