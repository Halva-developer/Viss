#pragma once
#include "../vissrt.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace viss {
    namespace str {
        // UTF-8 Helpers
        inline uint32_t decode_utf8(const Str& s, size_t& i) {
            unsigned char c = (unsigned char)s[i];
            if (c < 0x80) { i += 1; return c; }
            if ((c & 0xE0) == 0xC0) {
                if (i + 1 < s.size()) {
                    uint32_t cp = ((c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
                    i += 2; return cp;
                }
            } else if ((c & 0xF0) == 0xE0) {
                if (i + 2 < s.size()) {
                    uint32_t cp = ((c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) | ((unsigned char)s[i + 2] & 0x3F);
                    i += 3; return cp;
                }
            } else if ((c & 0xF8) == 0xF0) {
                if (i + 3 < s.size()) {
                    uint32_t cp = ((c & 0x07) << 18) | (((unsigned char)s[i + 1] & 0x3F) << 12) | (((unsigned char)s[i + 2] & 0x3F) << 6) | ((unsigned char)s[i + 3] & 0x3F);
                    i += 4; return cp;
                }
            }
            i += 1; return c;
        }

        inline void encode_utf8(uint32_t cp, Str& out) {
            if (cp <= 0x7F) {
                out.push_back((char)cp);
            } else if (cp <= 0x7FF) {
                out.push_back((char)(0xC0 | ((cp >> 6) & 0x1F)));
                out.push_back((char)(0x80 | (cp & 0x3F)));
            } else if (cp <= 0xFFFF) {
                out.push_back((char)(0xE0 | ((cp >> 12) & 0x0F)));
                out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back((char)(0x80 | (cp & 0x3F)));
            } else {
                out.push_back((char)(0xF0 | ((cp >> 18) & 0x07)));
                out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back((char)(0x80 | (cp & 0x3F)));
            }
        }

        inline uint32_t cp_upper(uint32_t cp) {
            if (cp >= 'a' && cp <= 'z') return cp - ('a' - 'A');
            if (cp >= 0x0430 && cp <= 0x044F) return cp - 0x20;
            if (cp == 0x0451) return 0x0401; // ё -> Ё
            if (cp == 0x0454) return 0x0404; // є -> Є
            if (cp == 0x0456) return 0x0406; // і -> І
            if (cp == 0x0457) return 0x0407; // ї -> Ї
            if (cp == 0x045E) return 0x040E; // ў -> Ў
            return cp;
        }

        inline uint32_t cp_lower(uint32_t cp) {
            if (cp >= 'A' && cp <= 'Z') return cp + ('a' - 'A');
            if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;
            if (cp == 0x0401) return 0x0451; // Ё -> ё
            if (cp == 0x0404) return 0x0454; // Є -> є
            if (cp == 0x0406) return 0x0456; // І -> і
            if (cp == 0x0407) return 0x0457; // Ї -> ї
            if (cp == 0x040E) return 0x045E; // Ў -> ў
            return cp;
        }

        inline Int len(const Str& s) {
            Int count = 0;
            for (char c : s) {
                if (((unsigned char)c & 0xC0) != 0x80) count++;
            }
            return count;
        }
        inline Int size(const Str& s) {
            return len(s);
        }
        inline Int byte_len(const Str& s) {
            return (Int)s.length();
        }
        inline Str sub(const Str& s, Int start, Int length) {
            if (start < 0 || length <= 0) return "";
            size_t b_start = s.size(), b_end = s.size();
            Int cp = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                if (((unsigned char)s[i] & 0xC0) != 0x80) {
                    if (cp == start) b_start = i;
                    if (cp == start + length) { b_end = i; break; }
                    cp++;
                }
            }
            if (b_start >= s.size()) return "";
            return s.substr(b_start, b_end - b_start);
        }
        inline Str substr(const Str& s, Int start, Int length) {
            return sub(s, start, length);
        }
        inline Str at(const Str& s, Int index) {
            return sub(s, index, 1);
        }
        inline Str get(const Str& s, Int index) {
            return at(s, index);
        }
        inline Str from_int(Int v) {
            return std::to_string(v);
        }
        inline Int find(const Str& s, const Str& subStr) {
            auto pos = s.find(subStr);
            if (pos == std::string::npos) return -1;
            return (Int)pos;
        }
        inline Int rfind(const Str& s, const Str& subStr) {
            auto pos = s.rfind(subStr);
            if (pos == std::string::npos) return -1;
            return (Int)pos;
        }
        inline Int count(const Str& s, const Str& subStr) {
            if (subStr.empty()) return 0;
            Int cnt = 0;
            size_t pos = 0;
            while ((pos = s.find(subStr, pos)) != std::string::npos) {
                cnt++;
                pos += subStr.length();
            }
            return cnt;
        }
        inline Bool is_digit(const Str& s) {
            if (s.empty()) return false;
            return std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
        }
        inline Bool is_alpha(const Str& s) {
            if (s.empty()) return false;
            return std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isalpha(c); });
        }
        inline Bool is_alnum(const Str& s) {
            if (s.empty()) return false;
            return std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isalnum(c); });
        }
        inline Bool is_space(const Str& s) {
            if (s.empty()) return false;
            return std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); });
        }
        inline Str capitalize(Str s) {
            if (s.empty()) return s;
            s[0] = (char)std::toupper((unsigned char)s[0]);
            for (size_t i = 1; i < s.length(); ++i) {
                s[i] = (char)std::tolower((unsigned char)s[i]);
            }
            return s;
        }
        inline List<Str> lines(const Str& s) {
            List<Str> res;
            std::stringstream ss(s);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                res.add(line);
            }
            return res;
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
        inline Str lower(const Str& s) {
            Str res;
            res.reserve(s.size());
            size_t i = 0;
            while (i < s.size()) {
                uint32_t cp = decode_utf8(s, i);
                encode_utf8(cp_lower(cp), res);
            }
            return res;
        }
        inline Str upper(const Str& s) {
            Str res;
            res.reserve(s.size());
            size_t i = 0;
            while (i < s.size()) {
                uint32_t cp = decode_utf8(s, i);
                encode_utf8(cp_upper(cp), res);
            }
            return res;
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
        template<typename T>
        inline Str from(const T& val) {
            return viss::toStr(val);
        }
        inline Int to_int(const Str& s, Int def_v = 0) {
            try { return (Int)std::stoll(s); } catch (...) { return def_v; }
        }
        inline double to_float(const Str& s, double def_v = 0.0) {
            try { return std::stod(s); } catch (...) { return def_v; }
        }
    }
}
