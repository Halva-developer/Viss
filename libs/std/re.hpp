#pragma once
#include "../vissrt.hpp"
#include <regex>
#include <string>
#include <vector>

namespace viss {
    namespace re {

        // re.match: check if entire string matches (or starts with) pattern
        inline bool match(const Str& pattern, const Str& s) {
            try {
                std::regex rx(pattern.c_str());
                return std::regex_search(s.c_str(), rx, std::regex_constants::match_continuous);
            } catch (...) {
                return false;
            }
        }

        // re.search: find first match substring
        inline Str search(const Str& pattern, const Str& s) {
            try {
                std::regex rx(pattern.c_str());
                std::cmatch m;
                if (std::regex_search(s.c_str(), m, rx)) {
                    return Str(m.str().c_str());
                }
            } catch (...) {}
            return Str("");
        }

        // re.findall: return list of all non-overlapping matches
        inline List<Str> findall(const Str& pattern, const Str& s) {
            List<Str> results;
            try {
                std::regex rx(pattern.c_str());
                std::string std_s = s.c_str();
                auto words_begin = std::sregex_iterator(std_s.begin(), std_s.end(), rx);
                auto words_end = std::sregex_iterator();
                for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
                    results.add(Str(it->str().c_str()));
                }
            } catch (...) {}
            return results;
        }

        // re.sub: replace matches with replacement
        inline Str sub(const Str& pattern, const Str& repl, const Str& s) {
            try {
                std::regex rx(pattern.c_str());
                std::string std_s = s.c_str();
                std::string res = std::regex_replace(std_s, rx, repl.c_str());
                return Str(res.c_str());
            } catch (...) {
                return s;
            }
        }

        // re.split: split string by pattern
        inline List<Str> split(const Str& pattern, const Str& s) {
            List<Str> results;
            try {
                std::regex rx(pattern.c_str());
                std::string std_s = s.c_str();
                std::sregex_token_iterator it(std_s.begin(), std_s.end(), rx, -1);
                std::sregex_token_iterator end;
                for (; it != end; ++it) {
                    results.add(Str(it->str().c_str()));
                }
            } catch (...) {
                results.add(s);
            }
            return results;
        }

    } // namespace re
} // namespace viss
