#include "htmldecode.h"

#include <algorithm>
#include <set>
#include <unordered_map>

std::string HtmlDecode(std::string_view str) {
    using namespace std::literals;

    static const std::set<std::string> mnemonics = {
        "&lt"s, "&LT"s,
        "&gt"s, "&GT"s,
        "&amp"s, "&AMP"s,
        "&apos"s, "&APOS"s,
        "&quot"s, "&QUOT"s
    };
    static const std::unordered_map<std::string, char> mnemonic_to_char = {
        {"&lt"s, '<'},
        {"&gt"s, '>'},
        {"&amp"s, '&'},
        {"&apos"s, '\''},
        {"&quot"s, '"'}
    };    

    std::string result;
    result.reserve(str.size());

    std::string mnemonic_buffer;
    for (size_t i = 0; i < str.size(); ++i) {
        if (mnemonic_buffer.empty()) {
            if (str.at(i) != '&') {
                result += str.at(i);
            } else {
                mnemonic_buffer += str.at(i);
            }
            continue;        
        }

        mnemonic_buffer += str.at(i);

        auto mnemonic_it = std::find_if(mnemonics.begin(), mnemonics.end(),
                                        [&mnemonic_buffer](const auto& nemonic){ 
                                            return nemonic.rfind(mnemonic_buffer, 0) == 0; 
                                        });
        if (mnemonic_it != mnemonics.end()) {
            if (mnemonic_buffer.size() != mnemonic_it->size()) {
                continue;
            }
            if (i + 1 < str.size() && str.at(i + 1) == ';') {
                ++i;
            }

            std::transform(mnemonic_buffer.begin(), mnemonic_buffer.end(), mnemonic_buffer.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            result += mnemonic_to_char.at(mnemonic_buffer);
        } else {
            result += mnemonic_buffer;
        }
        mnemonic_buffer = ""s;
    }
    result += mnemonic_buffer;

    return result;
}
