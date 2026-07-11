#pragma once
#include "../vissrt.hpp"
#include <algorithm>
#include <cctype>

namespace viss {
    namespace str {
        inline Int len(const Str& s) {
            return (Int)s.length();
        }
        inline Str sub(const Str& s, Int start, Int len) {
            if (start < 0 || start >= (Int)s.length()) return "";
            return s.substr(start, len);
        }
        inline Int find(const Str& s, const Str& subStr) {
            auto pos = s.find(subStr);
            if (pos == std::string::npos) return -1;
            return (Int)pos;
        }
        inline Str lower(Str s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
            return s;
        }
        inline Str upper(Str s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
            return s;
        }
        inline List<Str> split(const Str& s, const Str& delimiter) {
            List<Str> tokens;
            size_t prev = 0, pos = 0;
            do {
                pos = s.find(delimiter, prev);
                if (pos == std::string::npos) pos = s.length();
                Str token = s.substr(prev, pos - prev);
                tokens.add(token);
                prev = pos + delimiter.length();
            } while (pos < s.length() && prev < s.length());
            return tokens;
        }
    }
}
