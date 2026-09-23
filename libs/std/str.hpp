#pragma once
#include "../vissrt.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace viss {
    namespace str {
        inline Int len(const Str& s) {
            return (Int)s.length();
        }
        inline Str sub(const Str& s, Int start, Int length) {
            if (start < 0 || start >= (Int)s.length()) return "";
            return s.substr(start, length);
        }
        inline Int find(const Str& s, const Str& subStr) {
            auto pos = s.find(subStr);
            if (pos == std::string::npos) return -1;
            return (Int)pos;
        }
        inline Bool contains(const Str& s, const Str& subStr) {
            return s.find(subStr) != std::string::npos;
        }
        inline Bool starts_with(const Str& s, const Str& prefix) {
            return s.rfind(prefix, 0) == 0;
        }
        inline Bool ends_with(const Str& s, const Str& suffix) {
            if (s.length() < suffix.length()) return false;
            return s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0;
        }
        inline Str lower(Str s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
            return s;
        }
        inline Str upper(Str s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
            return s;
        }
        inline Str trim(const Str& s) {
            auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
            auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
            return (wsback <= wsfront ? Str() : Str(wsfront, wsback));
        }
        inline Str replace(Str s, const Str& from, const Str& to) {
            if (from.empty()) return s;
            size_t start_pos = 0;
            while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
                s.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
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
        inline Str join(const List<Str>& list, const Str& delimiter) {
            return list.join(delimiter);
        }
        inline Str repeat(const Str& s, Int count) {
            Str res = "";
            for (Int i = 0; i < count; ++i) res += s;
            return res;
        }
        inline Str pad_left(const Str& s, Int total_len, char ch = ' ') {
            if ((Int)s.length() >= total_len) return s;
            return Str(total_len - s.length(), ch) + s;
        }
        inline Str pad_right(const Str& s, Int total_len, char ch = ' ') {
            if ((Int)s.length() >= total_len) return s;
            return s + Str(total_len - s.length(), ch);
        }
    }
}
