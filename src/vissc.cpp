// =============================================================================
// Viss Native Compiler & Toolchain (vissc)
// Version 2.0 Native Edition — High Performance C++17 Core
// =============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include "viss_vm.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

const std::string VERSION = "0.2.1";
const std::string CODENAME = "Hambo";

// =============================================================================
// 1. UTILITY FUNCTIONS
// =============================================================================

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

std::vector<std::string> splitByChar(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            tokens.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty() || !tokens.empty()) {
        tokens.push_back(trim(cur));
    }
    return tokens;
}

// Find compiler executable
std::string getExecutableDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string p(path);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos) ? p.substr(0, pos) : ".";
#else
    char path[1024];
    ssize_t count = readlink("/proc/self/exe", path, 1024);
    if (count != -1) {
        std::string p(path, count);
        size_t pos = p.find_last_of("/");
        return (pos != std::string::npos) ? p.substr(0, pos) : ".";
    }
    return ".";
#endif
}

std::string findCompiler() {
    std::string candidate = "C:\\AGY\\TOOLS\\w64devkit\\bin\\g++.exe";
    if (fs::exists(candidate)) return candidate;
    std::string local = getExecutableDir() + "\\w64devkit\\bin\\g++.exe";
    if (fs::exists(local)) return local;
    return "g++";
}

// =============================================================================
// 2. STRING LITERAL EXTRACTION & RESTORATION
// =============================================================================

std::string extractStringLiterals(const std::string& code, std::vector<std::string>& literals) {
    std::string out;
    size_t i = 0;
    size_t n = code.size();
    while (i < n) {
        if (code[i] == '"') {
            std::string lit = "\"";
            i++;
            while (i < n) {
                if (code[i] == '\\' && i + 1 < n) {
                    lit += code[i++];
                    lit += code[i++];
                    continue;
                }
                if (code[i] == '"') {
                    lit += '"';
                    i++;
                    break;
                }
                lit += code[i++];
            }
            std::string placeholder = "__VISS_STR_LIT_" + std::to_string(literals.size()) + "__";
            literals.push_back(lit);
            out += placeholder;
        } else {
            out += code[i++];
        }
    }
    return out;
}

std::string restoreStringLiterals(const std::string& code, const std::vector<std::string>& literals) {
    std::string out = code;
    for (size_t idx = 0; idx < literals.size(); ++idx) {
        std::string placeholder = "__VISS_STR_LIT_" + std::to_string(idx) + "__";
        out = replaceAll(out, placeholder, literals[idx]);
    }
    return out;
}

// =============================================================================
// 3. STATEMENT TOKENIZER (NO-NEWLINE & SINGLE-LINE SUPPORT)
// =============================================================================

inline bool isContinuationChar(const std::string& s) {
    if (s.empty()) return true;
    char last = s.back();
    if (last == '+' || last == '-' || last == '*' || last == '/' || last == '%' ||
        last == '=' || last == ',' || last == '|' || last == '\\' || last == ':' ||
        last == '(' || last == '[' || last == '<' || last == '>') {
        return true;
    }
    if (endsWith(s, "&&") || endsWith(s, "||") || endsWith(s, "==") || endsWith(s, "!=") ||
        endsWith(s, "<=") || endsWith(s, ">=") || endsWith(s, "+=") || endsWith(s, "-=") ||
        endsWith(s, "*=") || endsWith(s, "/=") || endsWith(s, "..") || endsWith(s, "->") ||
        endsWith(s, "::") || endsWith(s, " and") || endsWith(s, " or") || endsWith(s, " not") ||
        endsWith(s, " to")) {
        return true;
    }
    return false;
}

inline bool isBlockHeaderWaitingForBrace(const std::string& s) {
    if (endsWith(s, "{")) return false;
    if (startsWith(s, "!func ") || startsWith(s, "!func(") ||
        startsWith(s, "func ") || startsWith(s, "func(") ||
        startsWith(s, "!async.func ") || startsWith(s, "!async.func(") ||
        startsWith(s, "!class ") || startsWith(s, "class ") ||
        startsWith(s, "!struct ") || startsWith(s, "struct ") ||
        s == "!main" || startsWith(s, "!main ") || s == "main" || startsWith(s, "main ") ||
        startsWith(s, "?if ") || startsWith(s, "?if(") || startsWith(s, "if ") || startsWith(s, "if(") ||
        startsWith(s, "?else if") || startsWith(s, "else if") || s == "?else" || startsWith(s, "?else ") || s == "else" || startsWith(s, "else ") ||
        startsWith(s, "!for ") || startsWith(s, "!for(") ||
        startsWith(s, "?for ") || startsWith(s, "?for(") ||
        startsWith(s, "for ") || startsWith(s, "for(") ||
        startsWith(s, "!while ") || startsWith(s, "!while(") ||
        startsWith(s, "?while ") || startsWith(s, "?while(") ||
        startsWith(s, "while ") || startsWith(s, "while(") ||
        startsWith(s, "?match ") || startsWith(s, "case ") ||
        s == "?try" || s == "try" || startsWith(s, "?expect ") ||
        (s.find("grid") != std::string::npos && s.find("edit") != std::string::npos)) {
        return true;
    }
    return false;
}

inline std::string resolveStringLiteralToken(const std::string& token, const std::vector<std::string>& string_literals) {
    std::string s = trim(token);
    std::smatch m;
    if (std::regex_match(s, m, std::regex(R"(^__VISS_STR_LIT_(\d+)__$)"))) {
        size_t idx = std::stoul(m[1].str());
        if (idx < string_literals.size()) {
            std::string lit = string_literals[idx];
            if (lit.size() >= 2 && lit.front() == '"' && lit.back() == '"') {
                return lit.substr(1, lit.size() - 2);
            }
            return lit;
        }
    }
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::vector<std::pair<std::string, int>> splitIntoStatements(const std::string& code) {
    std::vector<std::pair<std::string, int>> statements;
    std::string cur = "";
    int current_line = 1;
    int statement_start_line = 1;

    bool in_string = false;
    char str_quote = 0;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    size_t i = 0;
    size_t n = code.size();

    while (i < n) {
        char c = code[i];

        // Track newlines
        if (c == '\n') {
            current_line++;
        }

        // Single-line comment // or #
        if (!in_string && ((c == '/' && i + 1 < n && code[i + 1] == '/') || c == '#')) {
            while (i < n && code[i] != '\n') i++;
            continue;
        }

        // Block comment /* ... */
        if (!in_string && c == '/' && i + 1 < n && code[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(code[i] == '*' && code[i + 1] == '/')) {
                if (code[i] == '\n') current_line++;
                i++;
            }
            i += 2;
            continue;
        }

        // String literals
        if (!in_string && (c == '"' || (c == 'i' && i + 1 < n && code[i + 1] == '"'))) {
            in_string = true;
            if (c == 'i') {
                cur += c;
                i++;
                c = code[i];
            }
            str_quote = c;
            cur += c;
            i++;
            while (i < n && in_string) {
                if (code[i] == '\n') current_line++;
                if (code[i] == '\\' && i + 1 < n) {
                    cur += code[i++];
                    cur += code[i++];
                    continue;
                }
                if (code[i] == str_quote) {
                    cur += code[i++];
                    in_string = false;
                    break;
                }
                cur += code[i++];
            }
            continue;
        }

        // Grid edit block atomic capture: &name grid edit { ... }
        if (!in_string && c == '{') {
            std::string trimmed_cur = trim(cur);
            if (trimmed_cur.find("grid") != std::string::npos && trimmed_cur.find("edit") != std::string::npos) {
                int grid_edit_depth = 1;
                cur += c;
                i++;
                while (i < n && grid_edit_depth > 0) {
                    if (code[i] == '\n') current_line++;
                    if (code[i] == '{') grid_edit_depth++;
                    else if (code[i] == '}') {
                        grid_edit_depth--;
                        if (grid_edit_depth == 0) {
                            cur += code[i++];
                            break;
                        }
                    }
                    cur += code[i++];
                }
                statements.push_back({trim(cur), statement_start_line});
                cur.clear();
                statement_start_line = current_line;
                continue;
            }
        }

        // Dict literal atomic capture: @var = { ... } or return { ... }
        if (!in_string && c == '{') {
            std::string trimmed_cur = trim(cur);
            auto isDictAssignmentCandidate = [](const std::string& s) -> bool {
                if (startsWith(s, "?if") || startsWith(s, "if ") || startsWith(s, "if(") ||
                    startsWith(s, "?elif") || startsWith(s, "elif ") || startsWith(s, "elif(") ||
                    startsWith(s, "?else") || startsWith(s, "else ") || startsWith(s, "else{") ||
                    startsWith(s, "?while") || startsWith(s, "!while") || startsWith(s, "while ") || startsWith(s, "while(") ||
                    startsWith(s, "?for") || startsWith(s, "!for") || startsWith(s, "for ") || startsWith(s, "for(") ||
                    startsWith(s, "?try") || startsWith(s, "try") || startsWith(s, "?expect") || startsWith(s, "expect") ||
                    startsWith(s, "?match") || startsWith(s, "match") ||
                    startsWith(s, "!func") || startsWith(s, "func ") || startsWith(s, "!main") || startsWith(s, "!async") ||
                    startsWith(s, "!class") || startsWith(s, "class ") || startsWith(s, "!struct") || startsWith(s, "struct ") ||
                    startsWith(s, "!enum") || startsWith(s, "enum ")) {
                    return false;
                }
                if (startsWith(s, "!return") || startsWith(s, "return")) return true;
                for (size_t j = 0; j < s.size(); ++j) {
                    if (s[j] == '=') {
                        if (j > 0 && (s[j - 1] == '=' || s[j - 1] == '!' || s[j - 1] == '<' || s[j - 1] == '>')) continue;
                        if (j + 1 < s.size() && s[j + 1] == '=') continue;
                        return true;
                    }
                }
                return false;
            };
            if (isDictAssignmentCandidate(trimmed_cur)) {
                int dict_depth = 1;
                cur += c;
                i++;
                while (i < n && dict_depth > 0) {
                    char dc = code[i];
                    if (dc == '\n') current_line++;
                    if (dc == '"' || (dc == 'i' && i + 1 < n && code[i + 1] == '"')) {
                        if (dc == 'i') { cur += dc; i++; dc = code[i]; }
                        char q = dc;
                        cur += dc;
                        i++;
                        while (i < n) {
                            if (code[i] == '\n') current_line++;
                            if (code[i] == '\\' && i + 1 < n) {
                                cur += code[i++];
                                cur += code[i++];
                                continue;
                            }
                            if (code[i] == q) {
                                cur += code[i++];
                                break;
                            }
                            cur += code[i++];
                        }
                        continue;
                    }
                    if (dc == '{') dict_depth++;
                    else if (dc == '}') dict_depth--;
                    cur += dc;
                    i++;
                }
                continue;
            }
        }

        // Parens and brackets tracking
        if (c == '(') paren_depth++;
        else if (c == ')') { if (paren_depth > 0) paren_depth--; }
        else if (c == '[') bracket_depth++;
        else if (c == ']') { if (bracket_depth > 0) bracket_depth--; }

        if (c == '\n' && (paren_depth > 0 || bracket_depth > 0)) {
            if (!cur.empty() && cur.back() != ' ') cur += ' ';
            i++;
            continue;
        }

        // Top-level keyword boundary split if preceded by non-empty statement content
        if (paren_depth == 0 && bracket_depth == 0) {
            bool keyword_boundary = false;
            if (c == '$' && (code.compare(i, 7, "$import") == 0 || code.compare(i, 7, "$impoer") == 0 || code.compare(i, 7, "$kernel") == 0)) {
                keyword_boundary = true;
            } else if (c == '!' && (code.compare(i, 5, "!func") == 0 || code.compare(i, 5, "!main") == 0 || code.compare(i, 6, "!class") == 0 || code.compare(i, 11, "!async.func") == 0 || code.compare(i, 11, "!async func") == 0)) {
                keyword_boundary = true;
            }
            if (keyword_boundary) {
                std::string s = trim(cur);
                if (!s.empty()) {
                    if (!endsWith(s, ";") && !endsWith(s, "{") && !endsWith(s, "}")) {
                        if (!startsWith(s, "$") && !startsWith(s, "#")) s += ";";
                    }
                    statements.push_back({s, statement_start_line});
                    cur.clear();
                    statement_start_line = current_line;
                }
            }
        }

        // Statement delimiters outside parens and brackets
        if (paren_depth == 0 && bracket_depth == 0) {
            if (c == ';') {
                cur += c;
                std::string s = trim(cur);
                if (!s.empty()) {
                    statements.push_back({s, statement_start_line});
                }
                cur.clear();
                statement_start_line = current_line;
                i++;
                continue;
            } else if (c == '{') {
                cur += c;
                std::string s = trim(cur);
                if (!s.empty()) {
                    statements.push_back({s, statement_start_line});
                }
                cur.clear();
                brace_depth++;
                statement_start_line = current_line;
                i++;
                continue;
            } else if (c == '}') {
                std::string s = trim(cur);
                if (!s.empty()) {
                    if (!endsWith(s, ";") && !endsWith(s, "{")) s += ";";
                    statements.push_back({s, statement_start_line});
                    cur.clear();
                }
                statements.push_back({"}", current_line});
                if (brace_depth > 0) brace_depth--;
                statement_start_line = current_line;
                i++;
                continue;
            } else if (c == '\n') {
                std::string trimmed = trim(cur);
                if (trimmed.empty()) {
                    cur.clear();
                    statement_start_line = current_line;
                    i++;
                    continue;
                }
                if (isBlockHeaderWaitingForBrace(trimmed)) {
                    if (!cur.empty() && cur.back() != ' ') cur += ' ';
                    i++;
                    continue;
                }
                if (isContinuationChar(trimmed)) {
                    if (!cur.empty() && cur.back() != ' ') cur += ' ';
                    i++;
                    continue;
                }
                if (!endsWith(trimmed, ";") && !endsWith(trimmed, "{") && !endsWith(trimmed, "}")) {
                    if (!startsWith(trimmed, "$") && !startsWith(trimmed, "#")) {
                        trimmed += ";";
                    }
                }
                statements.push_back({trimmed, statement_start_line});
                cur.clear();
                statement_start_line = current_line;
                i++;
                continue;
            }
        }

        cur += c;
        i++;
    }

    std::string rem = trim(cur);
    if (!rem.empty()) {
        if (!endsWith(rem, ";") && !endsWith(rem, "{") && !endsWith(rem, "}")) {
            if (!startsWith(rem, "$") && !startsWith(rem, "#")) rem += ";";
        }
        statements.push_back({rem, statement_start_line});
    }

    return statements;
}

// =============================================================================
// 3.5 STRING KINDS PREPROCESSOR (r"...", b"...", """...""")
// =============================================================================

std::string preprocessStringKinds(const std::string& code) {
    std::string out;
    size_t i = 0;
    size_t n = code.size();

    while (i < n) {
        // Skip line comments
        if (code[i] == '/' && i + 1 < n && code[i + 1] == '/') {
            while (i < n && code[i] != '\n') out += code[i++];
            continue;
        }
        // Skip block comments
        if (code[i] == '/' && i + 1 < n && code[i + 1] == '*') {
            out += code[i++]; out += code[i++];
            while (i + 1 < n && !(code[i] == '*' && code[i + 1] == '/')) out += code[i++];
            if (i < n) out += code[i++];
            if (i < n) out += code[i++];
            continue;
        }

        bool is_ident_before = (i > 0 && (std::isalnum((unsigned char)code[i - 1]) || code[i - 1] == '_'));
        bool is_escaped = (i > 0 && code[i - 1] == '\\');
        if (is_escaped) {
            out += code[i++];
            continue;
        }

        // 1. Multiline strings: """...""", i"""...""", r"""..."""
        bool is_interp_multi = (!is_ident_before && code[i] == 'i' && i + 3 < n && code.compare(i + 1, 3, "\"\"\"") == 0);
        bool is_raw_multi = (!is_ident_before && code[i] == 'r' && i + 3 < n && code.compare(i + 1, 3, "\"\"\"") == 0);
        bool is_plain_multi = (code.compare(i, 3, "\"\"\"") == 0);

        if (is_interp_multi || is_raw_multi || is_plain_multi) {
            size_t start_offset = (is_interp_multi || is_raw_multi) ? 4 : 3;
            i += start_offset;
            // Trim leading newline if present
            if (i < n && code[i] == '\r') i++;
            if (i < n && code[i] == '\n') i++;

            std::string content;
            while (i < n && (code.compare(i, 3, "\"\"\"") != 0 || (i > 0 && code[i - 1] == '\\'))) {
                char c = code[i];
                if (c == '\n') {
                    content += "\\n";
                    i++;
                } else if (c == '\r') {
                    i++;
                } else if (c == '\t') {
                    content += "\\t";
                    i++;
                } else if (c == '"') {
                    content += "\\\"";
                    i++;
                } else if (c == '\\') {
                    if (is_raw_multi) {
                        content += "\\\\";
                        i++;
                    } else if (i + 1 < n) {
                        content += "\\";
                        content += code[++i];
                        i++;
                    } else {
                        content += "\\\\";
                        i++;
                    }
                } else {
                    content += c;
                    i++;
                }
            }
            if (i < n && code.compare(i, 3, "\"\"\"") == 0) {
                i += 3;
            }

            if (is_interp_multi) {
                out += "i\"" + content + "\"";
            } else {
                out += "\"" + content + "\"";
            }
            continue;
        }

        // 2. Raw string: r"..."
        if (!is_ident_before && code[i] == 'r' && i + 1 < n && code[i + 1] == '"') {
            i += 2;
            std::string content;
            while (i < n && code[i] != '"') {
                if (code[i] == '\\') {
                    content += "\\\\";
                    i++;
                    if (i < n && code[i] == '"') {
                        content += "\\\"";
                        i++;
                    }
                    continue;
                }
                content += code[i++];
            }
            if (i < n && code[i] == '"') i++;
            out += "\"" + content + "\"";
            continue;
        }

        // 3. Byte string: b"..."
        if (!is_ident_before && code[i] == 'b' && i + 1 < n && code[i + 1] == '"') {
            i += 2;
            std::string content;
            while (i < n && code[i] != '"') {
                if (code[i] == '\\' && i + 1 < n) {
                    content += code[i++];
                    content += code[i++];
                    continue;
                }
                content += code[i++];
            }
            if (i < n && code[i] == '"') i++;
            out += "viss::Bytes(\"" + content + "\")";
            continue;
        }

        // Regular string literal or non-multiline i"..."
        if (code[i] == '"' || (!is_ident_before && code[i] == 'i' && i + 1 < n && code[i + 1] == '"')) {
            if (code[i] == 'i') out += code[i++];
            out += code[i++];
            while (i < n) {
                if (code[i] == '\\' && i + 1 < n) {
                    out += code[i++];
                    out += code[i++];
                    continue;
                }
                if (code[i] == '"') {
                    out += code[i++];
                    break;
                }
                out += code[i++];
            }
            continue;
        }

        out += code[i++];
    }
    return out;
}

// =============================================================================
// 4. STRING INTERPOLATION
// =============================================================================

std::string translateInterpolation(const std::string& code) {
    std::string out;
    size_t i = 0;
    size_t n = code.size();

    while (i < n) {
        // Skip line comments
        if (code[i] == '/' && i + 1 < n && code[i + 1] == '/') {
            while (i < n && code[i] != '\n') out += code[i++];
            continue;
        }
        // Skip block comments
        if (code[i] == '/' && i + 1 < n && code[i + 1] == '*') {
            out += code[i++]; out += code[i++];
            while (i + 1 < n && !(code[i] == '*' && code[i + 1] == '/')) out += code[i++];
            if (i < n) out += code[i++];
            if (i < n) out += code[i++];
            continue;
        }
        // Interpolated string i"..."
        bool is_ident_char_before = (i > 0 && (std::isalnum((unsigned char)code[i - 1]) || code[i - 1] == '_'));
        if (code[i] == 'i' && i + 1 < n && code[i + 1] == '"' && !is_ident_char_before) {
            i += 2;
            std::vector<std::string> parts;
            std::string text_chunk;

            while (i < n && code[i] != '"') {
                if (code[i] == '\\' && i + 1 < n) {
                    text_chunk += code[i++];
                    text_chunk += code[i++];
                    continue;
                }
                if (code[i] == '{') {
                    if (!text_chunk.empty()) {
                        parts.push_back("viss::Str(\"" + text_chunk + "\")");
                        text_chunk.clear();
                    }
                    i++;
                    std::string expr;
                    while (i < n && code[i] != '}') {
                        if (code[i] == '\\' && i + 1 < n && code[i + 1] == '"') {
                            expr += '"';
                            i += 2;
                            continue;
                        }
                        expr += code[i++];
                    }
                    if (i < n && code[i] == '}') i++;
                    std::string clean_expr = replaceAll(expr, "@", "");
                    clean_expr = replaceAll(clean_expr, "&", "");
                    if (clean_expr.find("str.len") == std::string::npos) {
                        clean_expr = replaceAll(clean_expr, ".len()", ".size()");
                        clean_expr = std::regex_replace(clean_expr, std::regex(R"(\.len(?!\())"), ".size()");
                    }
                    if (clean_expr.find("sys.last") == std::string::npos) {
                        clean_expr = replaceAll(clean_expr, ".last()", ".get_last()");
                        clean_expr = std::regex_replace(clean_expr, std::regex(R"(\.last(?!\())"), ".get_last()");
                    }
                    clean_expr = replaceAll(clean_expr, ".first()", ".get_first()");
                    clean_expr = std::regex_replace(clean_expr, std::regex(R"(\.first(?!\())"), ".get_first()");
                    parts.push_back("viss::toStr(" + trim(clean_expr) + ")");
                    continue;
                }
                text_chunk += code[i++];
            }
            if (i < n && code[i] == '"') i++;
            if (!text_chunk.empty()) {
                parts.push_back("viss::Str(\"" + text_chunk + "\")");
            }
            if (parts.empty()) {
                out += "viss::Str(\"\")";
            } else {
                std::string combined = "(";
                for (size_t p = 0; p < parts.size(); ++p) {
                    combined += parts[p];
                    if (p + 1 < parts.size()) combined += " + ";
                }
                combined += ")";
                out += combined;
            }
            continue;
        }
        // Regular string literal "..." - copy as-is so contents aren't mistaken for i"..."
        if (code[i] == '"') {
            out += code[i++];
            while (i < n) {
                if (code[i] == '\\' && i + 1 < n) {
                    out += code[i++];
                    out += code[i++];
                    continue;
                }
                if (code[i] == '"') {
                    out += code[i++];
                    break;
                }
                out += code[i++];
            }
            continue;
        }
        out += code[i++];
    }
    return out;
}

// =============================================================================
// 5. PRIMITIVES & MODULE CALLS TRANSFORMERS
// =============================================================================

std::string applyStaticTransforms(std::string text) {
    // int.*
    text = replaceAll(text, "int.random(", "viss::math::random_int(");
    text = replaceAll(text, "int.parse(", "viss::toInt(");
    text = replaceAll(text, "int.to_hex(", "viss::toHex(");
    text = replaceAll(text, "int.to_bin(", "viss::toBin(");
    text = replaceAll(text, "int.abs(", "std::abs(");
    text = replaceAll(text, "int.min(", "std::min(");
    text = replaceAll(text, "int.max(", "std::max(");
    text = replaceAll(text, "int.clamp(", "viss::math::clamp(");

    // double.* / dec.*
    text = replaceAll(text, "double.sqrt(", "std::sqrt(");
    text = replaceAll(text, "dec.sqrt(", "std::sqrt(");
    text = replaceAll(text, "double.cbrt(", "std::cbrt(");
    text = replaceAll(text, "dec.cbrt(", "std::cbrt(");
    text = replaceAll(text, "double.pow(", "std::pow(");
    text = replaceAll(text, "dec.pow(", "std::pow(");
    text = replaceAll(text, "double.sin(", "std::sin(");
    text = replaceAll(text, "dec.sin(", "std::sin(");
    text = replaceAll(text, "double.cos(", "std::cos(");
    text = replaceAll(text, "dec.cos(", "std::cos(");
    text = replaceAll(text, "double.tan(", "std::tan(");
    text = replaceAll(text, "dec.tan(", "std::tan(");
    text = replaceAll(text, "double.round(", "std::round(");
    text = replaceAll(text, "dec.round(", "std::round(");
    text = replaceAll(text, "double.floor(", "std::floor(");
    text = replaceAll(text, "dec.floor(", "std::floor(");
    text = replaceAll(text, "double.ceil(", "std::ceil(");
    text = replaceAll(text, "dec.ceil(", "std::ceil(");
    text = replaceAll(text, "double.abs(", "std::abs(");
    text = replaceAll(text, "dec.abs(", "std::abs(");
    text = replaceAll(text, "double.min(", "std::min(");
    text = replaceAll(text, "dec.min(", "std::min(");
    text = replaceAll(text, "double.max(", "std::max(");
    text = replaceAll(text, "dec.max(", "std::max(");
    text = replaceAll(text, "double.clamp(", "viss::math::clamp(");
    text = replaceAll(text, "dec.clamp(", "viss::math::clamp(");
    text = replaceAll(text, "double.random(", "viss::math::random_dec(");
    text = replaceAll(text, "dec.random(", "viss::math::random_dec(");

    // str.*
    text = replaceAll(text, "str.from(", "viss::toStr(");
    text = replaceAll(text, "str::from(", "viss::toStr(");
    text = replaceAll(text, "str.len(", "viss::str::len(");
    text = replaceAll(text, "str.split(", "viss::str::split(");
    text = replaceAll(text, "str.join(", "viss::str::join(");
    text = replaceAll(text, "str.trim(", "viss::str::trim(");
    text = replaceAll(text, "str.lower(", "viss::str::lower(");
    text = replaceAll(text, "str.upper(", "viss::str::upper(");
    text = replaceAll(text, "str.contains(", "viss::str::contains(");
    text = replaceAll(text, "str.replace(", "viss::str::replace(");
    text = replaceAll(text, "str.starts_with(", "viss::str::starts_with(");
    text = replaceAll(text, "str.ends_with(", "viss::str::ends_with(");
    text = replaceAll(text, "str.sub(", "viss::str::sub(");
    text = replaceAll(text, "str.repeat(", "viss::str::repeat(");
    text = replaceAll(text, "str.pad_left(", "viss::str::pad_left(");
    text = replaceAll(text, "str.pad_right(", "viss::str::pad_right(");

    // time.*
    text = replaceAll(text, "time.sleep_ms(", "viss::time::sleep_ms(");
    text = replaceAll(text, "time.sleep(", "viss::time::sleep(");
    text = replaceAll(text, "time.now(", "viss::time::now(");
    text = replaceAll(text, "time.now_ms(", "viss::time::now_ms(");
    text = replaceAll(text, "time.format(", "viss::time::format(");

    // Generic collections
    text = replaceAll(text, "list<", "viss::List<");
    text = replaceAll(text, "List<", "viss::List<");
    text = replaceAll(text, "map<", "viss::Map<");
    text = replaceAll(text, "Map<", "viss::Map<");

    // Type casting
    text = replaceAll(text, "(dec)", "(viss::Dec)");
    text = replaceAll(text, "(int)", "(viss::Int)");
    text = replaceAll(text, "(str)", "(viss::Str)");
    text = replaceAll(text, "(bool)", "(viss::Bool)");

    // Functional-style casting: dec(...), int(...), str(...), bool(...)
    text = std::regex_replace(text, std::regex(R"((^|[^a-zA-Z0-9_.])dec\()"), "$1viss::toDec(");
    text = std::regex_replace(text, std::regex(R"((^|[^a-zA-Z0-9_.])int\()"), "$1viss::toInt(");
    text = std::regex_replace(text, std::regex(R"((^|[^a-zA-Z0-9_.])str\()"), "$1viss::toStr(");
    text = std::regex_replace(text, std::regex(R"((^|[^a-zA-Z0-9_.])bool\()"), "$1viss::toBool(");

    // Expression 'as <type>' casting:
    text = std::regex_replace(text, std::regex(R"(([a-zA-Z0-9_.]+(?:\([^)]*\))?)\s+as\s+dec)"), "viss::toDec($1)");
    text = std::regex_replace(text, std::regex(R"(([a-zA-Z0-9_.]+(?:\([^)]*\))?)\s+as\s+int)"), "viss::toInt($1)");
    text = std::regex_replace(text, std::regex(R"(([a-zA-Z0-9_.]+(?:\([^)]*\))?)\s+as\s+str)"), "viss::toStr($1)");
    text = std::regex_replace(text, std::regex(R"(([a-zA-Z0-9_.]+(?:\([^)]*\))?)\s+as\s+bool)"), "viss::toBool($1)");

    return text;
}

std::string replaceModuleCalls(std::string text, const std::vector<std::string>& imported_aliases) {
    // Strip leading dot shorthand like .draw. or .screen.
    text = std::regex_replace(text, std::regex(R"((^|\s)\.draw\.)"), "$1draw::");
    text = std::regex_replace(text, std::regex(R"((^|\s)\.screen\.)"), "$1screen::");
    text = std::regex_replace(text, std::regex(R"((^|\s)\.([a-zA-Z0-9_]+)::)"), "$1$2::");

    for (const auto& imp : imported_aliases) {
        std::string prefix_pat = "(^|[^@&a-zA-Z0-9_])" + imp;
        // Double submodule call: alias.sub.member -> alias::sub::member
        std::regex re2(prefix_pat + R"(\.([a-zA-Z0-9_]+)\.([a-zA-Z0-9_]+))");
        text = std::regex_replace(text, re2, "$1" + imp + "::$2::$3");

        // Single submodule call: alias.member -> alias::member
        std::regex re1(prefix_pat + R"(\.([a-zA-Z0-9_]+))");
        text = std::regex_replace(text, re1, "$1" + imp + "::$2");
    }
    return text;
}

// =============================================================================
// 6. MAIN TRANSPILER ENGINE
// =============================================================================

// Scope Tracker & Semantic Context for variable declaration, block isolation, and type checking
enum class VissType {
    Unknown,
    Int,
    Dec,
    Str,
    Bool,
    Bytes,
    Bits,
    Hybrid,
    Grid,
    List,
    Map,
    Any
};

inline std::string vissTypeName(VissType t) {
    switch (t) {
        case VissType::Int: return "int";
        case VissType::Dec: return "dec";
        case VissType::Str: return "str";
        case VissType::Bool: return "bool";
        case VissType::Bytes: return "bytes";
        case VissType::Bits: return "bits";
        case VissType::Hybrid: return "hybrid";
        case VissType::Grid: return "grid";
        case VissType::List: return "list";
        case VissType::Map: return "map";
        case VissType::Any: return "any";
        default: return "unknown";
    }
}

inline VissType parseVissType(const std::string& name) {
    std::string t = trim(name);
    if (t == "int" || t == "i8" || t == "u8" || t == "i16" || t == "u16" || t == "i32" || t == "u32" || t == "i64" || t == "u64" || t == "byte") return VissType::Int;
    if (t == "dec" || t == "double" || t == "float") return VissType::Dec;
    if (t == "str" || t == "string") return VissType::Str;
    if (t == "bool") return VissType::Bool;
    if (t == "bytes" || t == "bytemask" || t == "mask") return VissType::Bytes;
    if (t == "bits" || t == "bites") return VissType::Bits;
    if (t == "hybrid") return VissType::Hybrid;
    if (t == "grid") return VissType::Grid;
    if (t == "list" || startsWith(t, "[") || startsWith(t, "list<") || startsWith(t, "List<")) return VissType::List;
    if (t == "map" || t == "dict" || startsWith(t, "map<") || startsWith(t, "Map<") || startsWith(t, "dict<") || startsWith(t, "Dict<")) return VissType::Map;
    return VissType::Any;
}

inline std::string resolveVissCppType(const std::string& type_name) {
    std::string t = trim(type_name);
    std::string tl = t;
    for (auto& ch : tl) ch = std::tolower((unsigned char)ch);

    if (tl == "str" || tl == "string") return "viss::Str";
    if (tl == "int" || tl == "i8" || tl == "u8" || tl == "i16" || tl == "u16" || tl == "i32" || tl == "u32" || tl == "i64" || tl == "u64" || tl == "byte") return "viss::Int";
    if (tl == "dec" || tl == "double" || tl == "float") return "viss::Dec";
    if (tl == "bool") return "viss::Bool";
    if (tl == "bytes" || tl == "bytemask" || tl == "mask") return "viss::Bytes";
    if (tl == "bits" || tl == "bites") return "viss::Bits";
    if (tl == "hybrid") return "viss::Hybrid";
    if (tl == "grid") return "viss::Grid";
    if (tl == "var" || tl == "any") return "viss::Var";
    if (tl == "list" || tl == "[]") return "viss::List<viss::Var>";
    if (tl == "map" || tl == "dict" || tl == "{}") return "viss::Map<viss::Str, viss::Var>";
    if (tl == "inf") return "viss::Inf";

    // Typed lists: [str], list<str>, List<int>, etc.
    if ((startsWith(t, "[") && endsWith(t, "]")) || (startsWith(tl, "list<") && endsWith(tl, ">"))) {
        std::string inner;
        if (startsWith(t, "[")) inner = trim(t.substr(1, t.size() - 2));
        else inner = trim(t.substr(5, t.size() - 6));
        if (inner.empty() || inner == "any" || inner == "var") return "viss::List<viss::Var>";
        return "viss::List<" + resolveVissCppType(inner) + ">";
    }

    // Typed maps/dicts: map<str, int>, dict<str, var>, Map<str, int>
    if ((startsWith(tl, "map<") && endsWith(tl, ">")) ||
        (startsWith(tl, "dict<") && endsWith(tl, ">"))) {
        size_t start_idx = t.find('<');
        std::string inner = trim(t.substr(start_idx + 1, t.size() - start_idx - 2));
        size_t comma = inner.find(',');
        if (comma != std::string::npos) {
            std::string k_t = trim(inner.substr(0, comma));
            std::string v_t = trim(inner.substr(comma + 1));
            return "viss::Map<" + resolveVissCppType(k_t) + ", " + resolveVissCppType(v_t) + ">";
        }
        return "viss::Map<viss::Str, " + resolveVissCppType(inner) + ">";
    }

    return t;
}

inline bool isDictLiteral(const std::string& str) {
    std::string s = trim(str);
    if (s.size() < 2) return false;
    if (s.front() != '{' || s.back() != '}') return false;
    if (s == "{}") return true;
    bool in_str = false;
    int depth = 0;
    for (size_t i = 1; i < s.size() - 1; ++i) {
        char c = s[i];
        if (c == '"' && (i == 1 || s[i-1] != '\\')) in_str = !in_str;
        else if (!in_str) {
            if (c == '{' || c == '[' || c == '(') depth++;
            else if (c == '}' || c == ']' || c == ')') depth--;
            else if (depth == 0 && c == ':') return true;
        }
    }
    return false;
}

inline std::string transformDictLiteral(const std::string& dict_str, const std::string& target_type = "") {
    std::string s = trim(dict_str);
    std::string cpp_t = target_type.empty() ? "viss::Map<viss::Str, viss::Var>" : target_type;
    if (s == "{}") return cpp_t + "{}";
    std::string inner = trim(s.substr(1, s.size() - 2));
    if (inner.empty()) return cpp_t + "{}";

    std::vector<std::string> entries;
    std::string cur = "";
    bool in_str = false;
    int depth = 0;
    for (size_t i = 0; i < inner.size(); ++i) {
        char c = inner[i];
        if (c == '"' && (i == 0 || inner[i-1] != '\\')) in_str = !in_str;
        else if (!in_str) {
            if (c == '{' || c == '[' || c == '(') depth++;
            else if (c == '}' || c == ']' || c == ')') depth--;
            else if (depth == 0 && c == ',') {
                entries.push_back(trim(cur));
                cur.clear();
                continue;
            }
        }
        cur += c;
    }
    if (!cur.empty()) entries.push_back(trim(cur));

    std::string out = cpp_t + "{";
    for (size_t ei = 0; ei < entries.size(); ++ei) {
        const auto& entry = entries[ei];
        size_t col_pos = std::string::npos;
        bool q = false;
        int d = 0;
        for (size_t ci = 0; ci < entry.size(); ++ci) {
            char c = entry[ci];
            if (c == '"' && (ci == 0 || entry[ci-1] != '\\')) q = !q;
            else if (!q) {
                if (c == '{' || c == '[' || c == '(') d++;
                else if (c == '}' || c == ']' || c == ')') d--;
                else if (d == 0 && c == ':') { col_pos = ci; break; }
            }
        }
        if (col_pos != std::string::npos) {
            std::string k = trim(entry.substr(0, col_pos));
            std::string v = trim(entry.substr(col_pos + 1));
            if (!startsWith(k, "\"") && !startsWith(k, "__VISS_STR_LIT_") && !startsWith(k, "viss::Str(")) {
                k = "\"" + k + "\"";
            }
            out += "{" + k + ", " + v + "}";
        } else {
            out += "{" + entry + ", \"\"}";
        }
        if (ei + 1 < entries.size()) out += ", ";
    }
    out += "}";
    return out;
}

inline std::string transformListLiterals(const std::string& expr) {
    if (expr.find('[') == std::string::npos) return expr;
    std::string res;
    size_t i = 0;
    bool in_str = false;
    char quote_c = 0;

    auto is_ident_char = [](char c) {
        return (isalnum((unsigned char)c) || c == '_');
    };

    while (i < expr.size()) {
        char c = expr[i];

        // String literals
        if (!in_str && (c == '"' || c == '\'')) {
            in_str = true;
            quote_c = c;
            res += c;
            i++;
            continue;
        } else if (in_str && c == quote_c) {
            if (i > 0 && expr[i - 1] == '\\') {
                res += c;
                i++;
                continue;
            }
            in_str = false;
            res += c;
            i++;
            continue;
        }
        if (in_str) {
            res += c;
            i++;
            continue;
        }

        // Check for '['
        if (c == '[') {
            int p = (int)res.size() - 1;
            while (p >= 0 && std::isspace((unsigned char)res[p])) p--;

            bool is_subscript = false;
            if (p >= 0) {
                char pc = res[p];
                if (pc == ')' || pc == ']' || pc == '"' || pc == '\'') {
                    is_subscript = true;
                } else if (is_ident_char(pc)) {
                    int w_start = p;
                    while (w_start >= 0 && is_ident_char(res[w_start])) w_start--;
                    std::string word = res.substr(w_start + 1, p - w_start);
                    if (word != "return" && word != "yield" && word != "in" && word != "to" && word != "as" && word != "case") {
                        is_subscript = true;
                    }
                }
            }

            if (!is_subscript) {
                int depth = 1;
                bool inner_str = false;
                char inner_quote = 0;
                size_t j = i + 1;
                while (j < expr.size() && depth > 0) {
                    char jc = expr[j];
                    if (!inner_str && (jc == '"' || jc == '\'')) {
                        inner_str = true;
                        inner_quote = jc;
                    } else if (inner_str && jc == inner_quote) {
                        if (j > 0 && expr[j - 1] != '\\') inner_str = false;
                    } else if (!inner_str) {
                        if (jc == '[' || jc == '(' || jc == '{') depth++;
                        else if (jc == ']' || jc == ')' || jc == '}') depth--;
                    }
                    if (depth == 0) break;
                    j++;
                }

                if (depth == 0) {
                    std::string inner = expr.substr(i + 1, j - i - 1);
                    std::string transformed_inner = transformListLiterals(inner);
                    std::string trimmed_inner = trim(transformed_inner);
                    if (trimmed_inner.empty()) {
                        res += "viss::List<viss::Var>{}";
                    } else {
                        res += "viss::List{" + transformed_inner + "}";
                    }
                    i = j + 1;
                    continue;
                }
            }
        }

        res += c;
        i++;
    }
    return res;
}

inline std::string transformSlices(const std::string& expr) {
    if (expr.find("..") == std::string::npos) return expr;
    std::string res;
    size_t i = 0;
    bool in_str = false;
    while (i < expr.size()) {
        char c = expr[i];
        if (c == '"' && (i == 0 || expr[i-1] != '\\')) {
            in_str = !in_str;
            res += c;
            i++;
            continue;
        }
        if (in_str) {
            res += c;
            i++;
            continue;
        }

        if (c == '[' && i > 0 && (isalnum((unsigned char)expr[i-1]) || expr[i-1] == '_' || expr[i-1] == ')' || expr[i-1] == ']')) {
            size_t close_bracket = std::string::npos;
            int depth = 1;
            bool s_in_str = false;
            size_t dotdot_pos = std::string::npos;

            for (size_t j = i + 1; j < expr.size(); ++j) {
                char sc = expr[j];
                if (sc == '"' && (j == 0 || expr[j-1] != '\\')) s_in_str = !s_in_str;
                else if (!s_in_str) {
                    if (sc == '[') depth++;
                    else if (sc == ']') {
                        depth--;
                        if (depth == 0) { close_bracket = j; break; }
                    } else if (depth == 1 && sc == '.' && j + 1 < expr.size() && expr[j+1] == '.') {
                        dotdot_pos = j;
                    }
                }
            }

            if (close_bracket != std::string::npos && dotdot_pos != std::string::npos) {
                size_t t_end = res.size();
                size_t t_start = t_end;
                int paren_depth = 0;
                int b_depth = 0;
                while (t_start > 0) {
                    char prev = res[t_start - 1];
                    if (prev == ')') paren_depth++;
                    else if (prev == '(') {
                        if (paren_depth > 0) paren_depth--;
                        else break;
                    } else if (prev == ']') b_depth++;
                    else if (prev == '[') {
                        if (b_depth > 0) b_depth--;
                        else break;
                    } else if (paren_depth == 0 && b_depth == 0) {
                        if (!isalnum((unsigned char)prev) && prev != '_' && prev != '@' && prev != '.') {
                            break;
                        }
                    }
                    t_start--;
                }
                std::string target = res.substr(t_start, t_end - t_start);
                res.erase(t_start, t_end - t_start);

                std::string start_s = trim(expr.substr(i + 1, dotdot_pos - (i + 1)));
                std::string end_s = trim(expr.substr(dotdot_pos + 2, close_bracket - (dotdot_pos + 2)));

                if (start_s.empty()) start_s = "0";
                if (end_s.empty()) end_s = "2147483647";

                res += "viss::slice(" + target + ", " + start_s + ", " + end_s + ")";
                i = close_bracket + 1;
                continue;
            }
        }

        res += c;
        i++;
    }
    return res;
}

inline std::string transformInOperator(const std::string& expr) {
    if (expr.find("in") == std::string::npos) return expr;
    std::string s = expr;
    bool in_str = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' && (i == 0 || s[i-1] != '\\')) {
            in_str = !in_str;
            continue;
        }
        if (in_str) continue;

        if (s.compare(i, 2, "in") == 0) {
            bool left_ok = (i == 0 || (!isalnum((unsigned char)s[i-1]) && s[i-1] != '_'));
            bool right_ok = (i + 2 >= s.size() || (!isalnum((unsigned char)s[i+2]) && s[i+2] != '_'));
            if (left_ok && right_ok) {
                size_t p = i;
                while (p > 0 && (s[p-1] == ' ' || s[p-1] == '\t')) p--;
                bool is_neg = false;
                size_t op_start = i;
                if (p > 0 && s[p-1] == '!') {
                    is_neg = true;
                    op_start = p - 1;
                } else if (p >= 3 && s.substr(p - 3, 3) == "not" && (p == 3 || (!isalnum((unsigned char)s[p-4]) && s[p-4] != '_'))) {
                    is_neg = true;
                    op_start = p - 3;
                }

                size_t lhs_end = op_start;
                while (lhs_end > 0 && (s[lhs_end - 1] == ' ' || s[lhs_end - 1] == '\t')) lhs_end--;
                if (lhs_end == 0) continue;

                size_t lhs_start = lhs_end;
                int paren_d = 0;
                int brk_d = 0;
                while (lhs_start > 0) {
                    char prev = s[lhs_start - 1];
                    if (prev == ')') paren_d++;
                    else if (prev == '(') {
                        if (paren_d > 0) paren_d--;
                        else break;
                    } else if (prev == ']') brk_d++;
                    else if (prev == '[') {
                        if (brk_d > 0) brk_d--;
                        else break;
                    } else if (paren_d == 0 && brk_d == 0) {
                        if (prev == '&' || prev == '|' || prev == '=' || prev == '<' || prev == '>' || prev == '?' || prev == ',') {
                            break;
                        }
                    }
                    lhs_start--;
                }
                std::string lhs = trim(s.substr(lhs_start, lhs_end - lhs_start));

                size_t rhs_start = i + 2;
                while (rhs_start < s.size() && (s[rhs_start] == ' ' || s[rhs_start] == '\t')) rhs_start++;
                if (rhs_start >= s.size()) continue;

                size_t rhs_end = rhs_start;
                paren_d = 0;
                brk_d = 0;
                while (rhs_end < s.size()) {
                    char next = s[rhs_end];
                    if (next == '(') paren_d++;
                    else if (next == ')') {
                        if (paren_d > 0) paren_d--;
                        else break;
                    } else if (next == '[') brk_d++;
                    else if (next == ']') {
                        if (brk_d > 0) brk_d--;
                        else break;
                    } else if (paren_d == 0 && brk_d == 0) {
                        if (next == '&' || next == '|' || next == '=' || next == '<' || next == '>' || next == '?' || next == ',' || next == ';' || next == '{' || next == '}') {
                            break;
                        }
                    }
                    rhs_end++;
                }
                std::string rhs = trim(s.substr(rhs_start, rhs_end - rhs_start));

                std::string replacement;
                if (is_neg) {
                    replacement = "(!viss::contains(" + rhs + ", " + lhs + "))";
                } else {
                    replacement = "(viss::contains(" + rhs + ", " + lhs + "))";
                }

                s.replace(lhs_start, rhs_end - lhs_start, replacement);
                i = lhs_start + replacement.size();
            }
        }
    }
    return s;
}

struct VarSymbol {
    std::string name;
    VissType type;
    int line;
    bool is_buffer;
};

class ScopeTracker {
private:
    std::vector<std::map<std::string, VarSymbol>> scopes;
public:
    std::set<std::string> known_modules;
    std::set<std::string> known_functions;

    ScopeTracker() {
        scopes.emplace_back(); // global scope
        known_modules = {"io", "rt", "sys", "time", "math", "str", "draw", "screen", "fs", "async", "json", "crypto", "collections", "env", "net", "mask", "bytemask", "colormask", "audio", "snd", "sound", "media", "tag", "gui", "ui", "window", "win", "lic", "license"};
    }

    void enter_function() {
        while (scopes.size() > 1) scopes.pop_back();
        scopes.emplace_back(); // function-local scope
    }

    void enter_block() {
        scopes.emplace_back();
    }

    void exit_block() {
        if (scopes.size() > 1) scopes.pop_back();
    }

    bool count(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->find(name) != it->end()) return true;
        }
        return false;
    }

    void insert(const std::string& name, VissType type = VissType::Any, int line = 0, bool is_buf = false) {
        if (!scopes.empty()) {
            scopes.back()[name] = {name, type, line, is_buf};
        }
    }

    VarSymbol* get_var(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto fit = it->find(name);
            if (fit != it->end()) return &(fit->second);
        }
        return nullptr;
    }

    VissType infer_type_from_literal(const std::string& expr) {
        std::string s = trim(expr);
        if (s.empty()) return VissType::Unknown;
        if (startsWith(s, "__VISS_STR_LIT_") || (s.size() >= 2 && s.front() == '"' && s.back() == '"')) return VissType::Str;
        if (s == "true" || s == "false") return VissType::Bool;
        if (startsWith(s, "[") && endsWith(s, "]")) return VissType::List;
        if (startsWith(s, "{") && endsWith(s, "}")) return VissType::Map;
        if (startsWith(s, "@")) {
            auto sym = get_var(s.substr(1));
            if (sym) return sym->type;
        } else if (startsWith(s, "&")) {
            auto sym = get_var(s.substr(1));
            if (sym) return sym->type;
        }
        bool has_dot = false, all_digits = true;
        for (char c : s) {
            if (c == '.') has_dot = true;
            else if (!std::isdigit(c) && c != '-') all_digits = false;
        }
        if (all_digits) return has_dot ? VissType::Dec : VissType::Int;
        return VissType::Unknown;
    }
};

inline std::string processPipeline(const std::string& expr) {
    bool in_str = false;
    char str_quote = 0;
    int paren_depth = 0;
    std::vector<std::string> stages;
    std::string current;
    size_t n = expr.size();
    bool has_pipe = false;

    for (size_t i = 0; i < n; ++i) {
        char c = expr[i];
        if (in_str) {
            current += c;
            if (c == '\\' && i + 1 < n) {
                current += expr[++i];
            } else if (c == str_quote) {
                in_str = false;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            in_str = true;
            str_quote = c;
            current += c;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') paren_depth++;
        else if (c == ')' || c == ']' || c == '}') paren_depth--;

        if (paren_depth == 0 && c == '|' && i + 1 < n && expr[i + 1] == '>') {
            has_pipe = true;
            stages.push_back(trim(current));
            current.clear();
            i++; // skip '>'
            continue;
        }
        current += c;
    }
    if (!has_pipe) return expr;
    stages.push_back(trim(current));
    if (stages.size() < 2) return expr;

    std::string res = stages[0];
    for (size_t k = 1; k < stages.size(); ++k) {
        std::string fn = stages[k];
        if (fn.empty()) continue;
        size_t open_p = fn.find('(');
        size_t close_p = fn.rfind(')');
        if (open_p != std::string::npos && close_p != std::string::npos && close_p > open_p) {
            std::string fn_name = trim(fn.substr(0, open_p));
            std::string inner_args = trim(fn.substr(open_p + 1, close_p - (open_p + 1)));
            if (inner_args.empty()) {
                res = fn_name + "(" + res + ")";
            } else {
                res = fn_name + "(" + res + ", " + inner_args + ")";
            }
        } else {
            res = fn + "(" + res + ")";
        }
    }
    return res;
}

std::string transformExpression(
    const std::string& input_raw_expr,
    const std::vector<std::string>& imported_aliases,
    ScopeTracker& ctx,
    int line_num,
    const std::string& filename,
    std::string& err_out
) {
    std::string raw_expr = processPipeline(input_raw_expr);
    raw_expr = transformSlices(raw_expr);
    raw_expr = transformListLiterals(raw_expr);
    raw_expr = transformInOperator(raw_expr);
    while (raw_expr.find("!await") != std::string::npos || std::regex_search(raw_expr, std::regex(R"(\bawait\s+)"))) {
        std::string replaced = std::regex_replace(
            raw_expr,
            std::regex(R"(!?await\s+([@&a-zA-Z0-9_.:]+(?:\([^)]*\))?|\([^\)]+\)))"),
            "viss::async::await($1)"
        );
        if (replaced == raw_expr) break;
        raw_expr = replaced;
    }
    while (raw_expr.find("?:") != std::string::npos) {
        std::string replaced = std::regex_replace(
            raw_expr,
            std::regex(R"(([@&a-zA-Z0-9_.:]+(?:\([^)]*\))?|\([^\)]+\))\s*\?:\s*([@&a-zA-Z0-9_.:]+(?:\([^)]*\))?|\([^\)]+\)))"),
            "viss::elvis($1, $2)"
        );
        if (replaced == raw_expr) break;
        raw_expr = replaced;
    }
    std::string out;
    size_t i = 0;
    size_t n = raw_expr.size();

    VissType left_type = VissType::Unknown;
    char pending_op = 0;

    while (i < n) {
        char c = raw_expr[i];

        // Whitespace
        if (std::isspace(c)) {
            if (!out.empty() && out.back() != ' ' && out.back() != '(' && out.back() != '[' && out.back() != '{') {
                out += ' ';
            }
            i++;
            continue;
        }

        // @variable or @me
        if (c == '@') {
            i++;
            std::string var_name;
            while (i < n && (std::isalnum(raw_expr[i]) || raw_expr[i] == '_')) {
                var_name += raw_expr[i++];
            }
            if (var_name == "me") {
                if (i < n && raw_expr[i] == '.') {
                    out += "this->";
                    i++;
                } else {
                    out += "this";
                }
            } else {
                if (!ctx.count(var_name)) {
                    err_out = "Variable '@" + var_name + "' is not defined in this scope.";
                }
                auto sym = ctx.get_var(var_name);
                if (sym) {
                    if (pending_op == '+' && left_type == VissType::Str && sym->type == VissType::Bytes) {
                        err_out = "Cannot apply operator '+' between 'str' and 'bytes'.";
                    } else if (pending_op == '+' && left_type == VissType::Bytes && sym->type == VissType::Str) {
                        err_out = "Cannot apply operator '+' between 'bytes' and 'str'.";
                    }
                    left_type = sym->type;
                }
                out += var_name;
            }
            continue;
        }

        // Logical AND &&, bitwise AND &= or &, or &buffer
        if (c == '&') {
            if (i + 1 < n && raw_expr[i + 1] == '&') {
                out += "&&";
                i += 2;
                pending_op = 0;
                continue;
            }
            if (i + 1 < n && raw_expr[i + 1] == '=') {
                out += "&=";
                i += 2;
                continue;
            }
            if (i + 1 < n && (std::isalpha((unsigned char)raw_expr[i + 1]) || raw_expr[i + 1] == '_')) {
                i++;
                std::string buf_name;
                while (i < n && (std::isalnum((unsigned char)raw_expr[i]) || raw_expr[i] == '_')) {
                    buf_name += raw_expr[i++];
                }
                auto sym = ctx.get_var(buf_name);
                if (sym) {
                    if (pending_op == '+' && left_type == VissType::Str && sym->type == VissType::Bytes) {
                        err_out = "Cannot apply operator '+' between 'str' and 'bytes'.";
                    }
                    left_type = sym->type;
                } else {
                    left_type = VissType::Bytes;
                    if (pending_op == '+' && left_type == VissType::Str) {
                        err_out = "Cannot apply operator '+' between 'str' and 'bytes'.";
                    }
                }
                out += buf_name;
                continue;
            }
            out += '&';
            i++;
            continue;
        }

        // String placeholder __VISS_STR_LIT_X__
        if (c == '_' && i + 15 < n && raw_expr.compare(i, 15, "__VISS_STR_LIT_") == 0) {
            std::string lit_token;
            while (i < n && (std::isalnum(raw_expr[i]) || raw_expr[i] == '_')) {
                lit_token += raw_expr[i++];
            }
            left_type = VissType::Str;
            out += "viss::Str(" + lit_token + ")";
            continue;
        }

        // Numbers
        if (std::isdigit(c)) {
            std::string num;
            bool has_dot = false;
            while (i < n && (std::isdigit(raw_expr[i]) || raw_expr[i] == '.' || raw_expr[i] == 'x' || (raw_expr[i] >= 'a' && raw_expr[i] <= 'f') || (raw_expr[i] >= 'A' && raw_expr[i] <= 'F'))) {
                if (raw_expr[i] == '.') {
                    if (i + 1 < n && raw_expr[i + 1] == '.') break; // range 0..10
                    has_dot = true;
                }
                num += raw_expr[i++];
            }
            left_type = has_dot ? VissType::Dec : VissType::Int;
            out += num;
            continue;
        }

        // Dot member access or shorthand (.draw., .screen., .len, .first, .last)
        if (c == '.') {
            if (i + 5 <= n && raw_expr.compare(i, 5, ".len(") == 0) {
                out += ".size("; i += 5; continue;
            } else if (i + 4 <= n && raw_expr.compare(i, 4, ".len") == 0 && (i + 4 == n || !std::isalnum(raw_expr[i+4]))) {
                out += ".size()"; i += 4; continue;
            } else if (i + 6 <= n && raw_expr.compare(i, 6, ".first") == 0 && (i + 6 == n || !std::isalnum(raw_expr[i+6]))) {
                out += ".get_first()"; i += 6; continue;
            } else if (i + 5 <= n && raw_expr.compare(i, 5, ".last") == 0 && (i + 5 == n || !std::isalnum(raw_expr[i+5]))) {
                out += ".get_last()"; i += 5; continue;
            } else if (i + 6 <= n && raw_expr.compare(i, 6, ".draw.") == 0) {
                out += "draw::"; i += 6; continue;
            } else if (i + 8 <= n && raw_expr.compare(i, 8, ".screen.") == 0) {
                out += "screen::"; i += 8; continue;
            }
            out += c;
            i++;
            continue;
        }

        // Identifiers and keywords
        if (std::isalpha(c) || c == '_') {
            std::string ident;
            while (i < n && (std::isalnum(raw_expr[i]) || raw_expr[i] == '_')) {
                ident += raw_expr[i++];
            }

            // Check for keywords
            if (ident == "and") { out += "&&"; pending_op = 0; continue; }
            if (ident == "or")  { out += "||"; pending_op = 0; continue; }
            if (ident == "not") { out += "!"; continue; }
            if (ident == "Null") { out += "nullptr"; continue; }
            if (ident == "true" || ident == "false") { left_type = VissType::Bool; out += ident; continue; }

            // Module or static call checking: alias.func or alias.sub.func
            bool is_mod = ctx.known_modules.count(ident) > 0;
            for (const auto& a : imported_aliases) if (a == ident) is_mod = true;

            if (is_mod && i < n && raw_expr[i] == '.') {
                i++; // skip .
                std::string member;
                while (i < n && (std::isalnum(raw_expr[i]) || raw_expr[i] == '_')) {
                    member += raw_expr[i++];
                }
                if (i < n && raw_expr[i] == '?' && i + 1 < n && raw_expr[i+1] == '(') {
                    member += "_q";
                    i++;
                } else if (i < n && raw_expr[i] == '!' && i + 1 < n && raw_expr[i+1] == '(') {
                    member += "_bang";
                    i++;
                }

                if (i < n && raw_expr[i] == '.') {
                    i++; // skip second .
                    std::string sub_member;
                    while (i < n && (std::isalnum(raw_expr[i]) || raw_expr[i] == '_')) {
                        sub_member += raw_expr[i++];
                    }
                    if (i < n && raw_expr[i] == '?' && i + 1 < n && raw_expr[i+1] == '(') {
                        sub_member += "_q";
                        i++;
                    } else if (i < n && raw_expr[i] == '!' && i + 1 < n && raw_expr[i+1] == '(') {
                        sub_member += "_bang";
                        i++;
                    }
                    out += ident + "::" + member + "::" + sub_member;
                } else {
                    out += ident + "::" + member;
                }
                continue;
            }

            // Built-in static type transforms: int.random, str.len, etc.
            if ((ident == "int" || ident == "double" || ident == "dec" || ident == "str" || ident == "time") && i < n && raw_expr[i] == '.') {
                i++; // skip .
                std::string method;
                while (i < n && (std::isalnum(raw_expr[i]) || raw_expr[i] == '_')) {
                    method += raw_expr[i++];
                }
                if (ident == "int") {
                    if (method == "random") out += "viss::math::random_int";
                    else if (method == "parse") out += "viss::toInt";
                    else if (method == "abs") out += "std::abs";
                    else if (method == "min") out += "std::min";
                    else if (method == "max") out += "std::max";
                    else if (method == "clamp") out += "viss::math::clamp";
                    else if (method == "to_hex") out += "viss::toHex";
                    else if (method == "to_bin") out += "viss::toBin";
                    else out += "int::" + method;
                } else if (ident == "double" || ident == "dec") {
                    if (method == "sqrt") out += "std::sqrt";
                    else if (method == "abs") out += "std::abs";
                    else if (method == "min") out += "std::min";
                    else if (method == "max") out += "std::max";
                    else if (method == "sin") out += "std::sin";
                    else if (method == "cos") out += "std::cos";
                    else if (method == "pow") out += "std::pow";
                    else if (method == "clamp") out += "viss::math::clamp";
                    else if (method == "random") out += "viss::math::random_dec";
                    else out += "std::" + method;
                } else if (ident == "str") {
                    if (method == "from") out += "viss::toStr";
                    else out += "viss::str::" + method;
                } else if (ident == "time") {
                    out += "viss::time::" + method;
                }
                continue;
            }

            // Function query / mutating call: ident?( -> ident_q(
            if (i < n && raw_expr[i] == '?' && i + 1 < n && raw_expr[i + 1] == '(') {
                out += ident + "_q";
                i++; // skip ?
                continue;
            }
            // Built-in casting functions: str(val), int(val), dec(val)
            if (ident == "str" && i < n && raw_expr[i] == '(') {
                out += "viss::toStr";
                continue;
            }
            if (ident == "int" && i < n && raw_expr[i] == '(') {
                out += "viss::toInt";
                continue;
            }
            if ((ident == "dec" || ident == "double") && i < n && raw_expr[i] == '(') {
                out += "viss::toDec";
                continue;
            }

            out += ident;
            continue;
        }

        // Lambda / Closure expressions: |x| expr or |a, b| expr
        if (c == '|' && i + 1 < n && raw_expr[i + 1] != '|' && raw_expr[i + 1] != '=') {
            size_t pipe_end = raw_expr.find('|', i + 1);
            if (pipe_end != std::string::npos) {
                std::string params_raw = raw_expr.substr(i + 1, pipe_end - (i + 1));
                bool valid_params = true;
                std::vector<std::string> param_names;
                std::string cur_p;
                for (char ch : params_raw) {
                    if (ch == ',') {
                        std::string p = trim(cur_p);
                        if (startsWith(p, "@")) p = p.substr(1);
                        if (!p.empty()) param_names.push_back(p);
                        cur_p.clear();
                    } else if (std::isalnum(ch) || ch == '_' || ch == ' ' || ch == '@') {
                        cur_p += ch;
                    } else {
                        valid_params = false;
                        break;
                    }
                }
                std::string p = trim(cur_p);
                if (startsWith(p, "@")) p = p.substr(1);
                if (!p.empty()) param_names.push_back(p);

                if (valid_params && !param_names.empty()) {
                    size_t body_start = pipe_end + 1;
                    size_t body_end = body_start;
                    int inner_paren = 0;
                    int inner_brace = 0;
                    while (body_end < n) {
                        char bc = raw_expr[body_end];
                        if (bc == '(') inner_paren++;
                        else if (bc == ')') {
                            if (inner_paren == 0) break;
                            inner_paren--;
                        } else if (bc == '{') inner_brace++;
                        else if (bc == '}') {
                            if (inner_brace == 0) break;
                            inner_brace--;
                        } else if (bc == ',' && inner_paren == 0 && inner_brace == 0) {
                            break;
                        }
                        body_end++;
                    }

                    std::string lambda_body = trim(raw_expr.substr(body_start, body_end - body_start));
                    std::string sub_err;
                    ctx.enter_block();
                    for (const auto& pn : param_names) ctx.insert(pn, VissType::Any, line_num);
                    std::string trans_body = transformExpression(lambda_body, imported_aliases, ctx, line_num, filename, sub_err);
                    ctx.exit_block();

                    std::string lambda_cpp = "[=](";
                    for (size_t pi = 0; pi < param_names.size(); ++pi) {
                        lambda_cpp += "auto " + param_names[pi] + (pi + 1 < param_names.size() ? ", " : "");
                    }
                    lambda_cpp += ") { ";
                    if (startsWith(trans_body, "{") && endsWith(trans_body, "}")) {
                        lambda_cpp += trans_body.substr(1, trans_body.size() - 2) + " }";
                    } else {
                        lambda_cpp += "return (" + trans_body + "); }";
                    }

                    out += lambda_cpp;
                    i = body_end;
                    continue;
                }
            }
        }

        // Operators & delimiters
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            pending_op = c;
        }
        out += c;
        i++;
    }

    return out;
}

std::string transpile(const std::string& raw_viss_code, const std::string& filename, const std::string& current_dir = ".", bool is_module = false) {
    std::string viss_code = translateInterpolation(preprocessStringKinds(raw_viss_code));
    std::vector<std::string> string_literals;
    viss_code = extractStringLiterals(viss_code, string_literals);

    auto statements = splitIntoStatements(viss_code);

    // Pass 1: Forward Declarations for typed functions
    std::vector<std::string> forward_decl_section;
    for (const auto& item : statements) {
        std::string s = trim(item.first);
        std::smatch m_f;
        if (std::regex_match(s, m_f, std::regex(R"(^!?(?:async[\.\s]+)?func\s+([a-zA-Z0-9_?!]+)\s*\(([^)]*)\)(?:\s+(?:to|as|->)\s+([a-zA-Z0-9_<>]+))?\s*\{$)"))) {
            std::string raw_name = m_f[1].str();
            if (raw_name == "main") continue;
            std::string fname = replaceAll(replaceAll(raw_name, "?", "_q"), "!", "_bang");
            std::string params_str = m_f[2].str();
            std::string ret_type = m_f[3].str();

            std::vector<std::string> param_tokens = splitByChar(params_str, ',');
            std::vector<std::string> cpp_params;
            std::vector<std::string> tmpl_params;
            for (size_t p_idx = 0; p_idx < param_tokens.size(); ++p_idx) {
                std::string p = trim(param_tokens[p_idx]);
                if (p.empty()) continue;
                size_t eq_pos = p.find('=');
                if (eq_pos != std::string::npos) p = trim(p.substr(0, eq_pos));
                std::smatch m_as;
                if (std::regex_match(p, m_as, std::regex(R"(^([a-zA-Z0-9_]+)\s*(?:as|:)\s*([a-zA-Z0-9_<>]+)$)"))) {
                    std::string pname = m_as[1].str();
                    std::string ptype = resolveVissCppType(m_as[2].str());
                    cpp_params.push_back(ptype + " " + pname);
                } else {
                    std::string pname = replaceAll(replaceAll(p, "@", ""), "&", "");
                    std::string tname = "_T" + std::to_string(p_idx) + "_" + pname;
                    tmpl_params.push_back("typename " + tname);
                    cpp_params.push_back(tname + " " + pname);
                }
            }

            std::string tmpl_clause = "";
            if (!tmpl_params.empty()) {
                tmpl_clause = "template<";
                for (size_t ti = 0; ti < tmpl_params.size(); ++ti) {
                    tmpl_clause += tmpl_params[ti] + (ti + 1 < tmpl_params.size() ? ", " : "> ");
                }
            }
            std::string joined_params = "";
            for (size_t pi = 0; pi < cpp_params.size(); ++pi) {
                joined_params += cpp_params[pi] + (pi + 1 < cpp_params.size() ? ", " : "");
            }

            if (!ret_type.empty()) {
                std::string cpp_ret = resolveVissCppType(ret_type);
                bool is_async_f = startsWith(s, "!async") || startsWith(s, "async");
                if (is_async_f) {
                    cpp_ret = "viss::async::Task<" + cpp_ret + ">";
                }
                forward_decl_section.push_back(tmpl_clause + cpp_ret + " " + fname + "(" + joined_params + ");");
            }
        }
    }

    std::vector<std::string> includes_section;
    std::vector<std::string> aliases_section;
    std::vector<std::string> classes_section;
    std::vector<std::string> functions_section;
    std::vector<std::string> main_section;

    includes_section.push_back("#include \"libs/vissrt.hpp\"");
    includes_section.push_back("#include \"libs/std/mask.hpp\"");
    aliases_section.push_back("using namespace viss;");
    aliases_section.push_back("namespace io = viss::io;");
    aliases_section.push_back("namespace sys = viss::sys;");
    aliases_section.push_back("namespace fs = viss::fs;");
    aliases_section.push_back("namespace math = viss::math;");
    aliases_section.push_back("namespace str = viss::str;");
    aliases_section.push_back("namespace rt = viss::retrotech;");
    aliases_section.push_back("namespace async = viss::async;");
    aliases_section.push_back("namespace json = viss::json;");
    aliases_section.push_back("namespace crypto = viss::crypto;");
    aliases_section.push_back("namespace collections = viss::collections;");
    aliases_section.push_back("namespace env = viss::env;");
    aliases_section.push_back("namespace net = viss::net;");
    aliases_section.push_back("namespace mask = viss::bytemask;");
    aliases_section.push_back("namespace bytemask = viss::bytemask;");
    aliases_section.push_back("namespace colormask = viss::bytemask;");
    aliases_section.push_back("namespace audio = viss::audio;");
    aliases_section.push_back("namespace snd = viss::audio;");
    aliases_section.push_back("namespace sound = viss::audio;");
    aliases_section.push_back("namespace media = viss::media;");
    aliases_section.push_back("namespace tag = viss::media;");
    aliases_section.push_back("namespace gui = viss::gui;");
    aliases_section.push_back("namespace ui = viss::gui;");
    aliases_section.push_back("namespace window = viss::gui;");
    aliases_section.push_back("namespace win = viss::gui;");
    includes_section.push_back("#include \"libs/std/audio.hpp\"");
    includes_section.push_back("#include \"libs/std/media.hpp\"");
    includes_section.push_back("#include \"libs/std/gui.hpp\"");

    std::vector<std::string> imported_aliases = {
        "io", "sys", "fs", "math", "str", "rt", "async", "json",
        "crypto", "collections", "env", "net", "mask", "bytemask", "colormask",
        "audio", "snd", "sound", "media", "tag", "gui", "ui", "window", "win", "lic", "license"
    };

    ScopeTracker declared_vars;
    std::set<std::string> known_classes;
    std::string current_class_name = "";
    std::map<std::string, std::map<std::string, std::string>> class_fields;

    std::string current_struct_name = "";
    struct StructFieldInfo {
        std::string name;
        std::string cpp_type;
        std::string default_val;
    };
    std::map<std::string, std::vector<StructFieldInfo>> struct_fields;

    std::string current_enum_name = "";
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> enum_fields;

    std::vector<std::pair<std::string, std::string>> block_stack;
    std::vector<std::string>* current_target = &main_section;

    for (const auto& item : statements) {
        std::string stripped = trim(item.first);
        int line_num = item.second;
        if (stripped.empty()) continue;

        // 1.0 Relative/Local Viss file import: $import file "module.viss" as alias or $import "module.viss" as alias
        std::smatch m_vimp;
        if (std::regex_match(stripped, m_vimp, std::regex(R"(^\$import(?:\s+file)?\s+([^\s]+)\s+as\s+([a-zA-Z0-9_]+);?$)"))) {
            std::string file_token = resolveStringLiteralToken(m_vimp[1].str(), string_literals);
            std::string alias = m_vimp[2].str();
            if (endsWith(file_token, ".viss")) {
                fs::path target_file = fs::path(current_dir) / file_token;
                if (!fs::exists(target_file)) {
                    target_file = fs::current_path() / file_token;
                }
                if (fs::exists(target_file)) {
                    std::ifstream inf(target_file);
                    std::stringstream ssb; ssb << inf.rdbuf();
                    std::string mod_cpp = transpile(ssb.str(), target_file.filename().string(), target_file.parent_path().string(), true);
                    classes_section.push_back("namespace " + alias + " {");
                    classes_section.push_back(mod_cpp);
                    classes_section.push_back("}");
                    imported_aliases.push_back(alias);
                    declared_vars.known_modules.insert(alias);
                    continue;
                } else {
                    std::cerr << "[Viss Import Error] Cannot find imported file '" << file_token << "'\n";
                }
            }
        }

        // 1. Directives: $import lib from "system" ... as ...
        std::smatch m_imp;
        if (std::regex_match(stripped, m_imp, std::regex(R"(^\$import\s+lib\s+from\s+([^\s\[]+)(?:\s*\[[^\]]*\])?\s+as\s+([a-zA-Z0-9_]+);?$)"))) {
            std::string alias = m_imp[2].str();
            imported_aliases.push_back(alias);
            includes_section.push_back("#include \"libs/std/sys.hpp\"");
            aliases_section.push_back("namespace " + alias + " = viss::sys;");
            continue;
        }

        // $import lib "..." as ... or $impoer lib "..." as ...
        if (std::regex_match(stripped, m_imp, std::regex(R"(^\$impo(?:rt|er)\s+lib\s+([^\s]+)\s+as\s+([a-zA-Z0-9_]+);?$)"))) {
            std::string lib_name = resolveStringLiteralToken(m_imp[1].str(), string_literals);
            std::string alias = m_imp[2].str();
            imported_aliases.push_back(alias);
            if (lib_name == "asyncIO" || lib_name == "async") {
                aliases_section.push_back("namespace " + alias + " = viss::async;");
            } else if (lib_name == "iostream" || lib_name == "io") {
                includes_section.push_back("#include \"libs/std/io.hpp\"");
                aliases_section.push_back("namespace " + alias + " = viss::io;");
            } else if (lib_name == "retrotech" || lib_name == "rt") {
                includes_section.push_back("#include \"libs/std/retrotech.hpp\"");
                aliases_section.push_back("namespace " + alias + " = viss::retrotech;");
                aliases_section.push_back("namespace draw = viss::retrotech::draw;");
                aliases_section.push_back("namespace screen = viss::retrotech::screen;");
                imported_aliases.push_back("draw");
                imported_aliases.push_back("screen");
            } else if (lib_name == "mask" || lib_name == "bytemask" || lib_name == "colormask") {
                includes_section.push_back("#include \"libs/std/mask.hpp\"");
                aliases_section.push_back("namespace " + alias + " = viss::bytemask;");
            } else if (lib_name == "crypto") {
                includes_section.push_back("#include \"libs/std/crypto.hpp\"");
                aliases_section.push_back("namespace " + alias + " = viss::crypto;");
            } else if (lib_name == "json") {
                includes_section.push_back("#include \"libs/std/json.hpp\"");
                aliases_section.push_back("namespace " + alias + " = viss::json;");
            } else if (lib_name == "time") {
                if (alias != "time") {
                    aliases_section.push_back("namespace " + alias + " = viss::time;");
                }
            } else {
                includes_section.push_back("#include \"libs/std/" + lib_name + ".hpp\"");
                if (alias != "gui" && alias != "ui" && alias != "window" && alias != "win" && alias != "media" && alias != "tag" && alias != "audio" && alias != "snd" && alias != "sound") {
                    aliases_section.push_back("namespace " + alias + " = viss::" + lib_name + ";");
                }
            }
            continue;
        }

        if (std::regex_match(stripped, m_imp, std::regex(R"(^\$import\s+cpp\s+<([^>]+)>\s+as\s+([a-zA-Z0-9_]+);?$)"))) {
            includes_section.push_back("#include <" + m_imp[1].str() + ">");
            aliases_section.push_back("namespace " + m_imp[2].str() + " = ::" + m_imp[1].str() + ";");
            imported_aliases.push_back(m_imp[2].str());
            continue;
        }

        if (std::regex_match(stripped, m_imp, std::regex(R"(^@use\s+<?([a-zA-Z0-9_]+)>?\s+for\s+\*;?$)"))) {
            continue;
        }

        // 1.5 Struct definition: struct Name { or !struct Name {
        std::smatch m_strc;
        if (std::regex_match(stripped, m_strc, std::regex(R"(^(?:!struct|struct)\s+([a-zA-Z0-9_]+)\s*\{$)"))) {
            std::string sname = m_strc[1].str();
            current_struct_name = sname;
            known_classes.insert(sname);
            declared_vars.insert(sname);
            declared_vars.enter_block();
            block_stack.push_back({"struct", sname});
            struct_fields[sname] = {};
            current_target = &classes_section;
            current_target->push_back("struct " + sname + " {");
            continue;
        }

        // Inside struct: parse fields if at top-level of struct and not a function or block closure
        if (!current_struct_name.empty() && !block_stack.empty() && block_stack.back().first == "struct" && stripped != "}" && !startsWith(stripped, "!func") && !startsWith(stripped, "func")) {
            // Typed field: @x: dec = 0.0 or x: dec = 0.0 or x: dec
            std::smatch m_tfld;
            if (std::regex_match(stripped, m_tfld, std::regex(R"(^@?([a-zA-Z0-9_]+)\s*:\s*([a-zA-Z0-9_<>]+)(?:\s*=\s*([^;]+))?;?$)"))) {
                std::string fname = m_tfld[1].str();
                std::string ftype = m_tfld[2].str();
                std::string def_val = m_tfld[3].matched ? trim(m_tfld[3].str()) : "";
                std::string cpp_type = ftype;
                if (ftype == "dec" || ftype == "float" || ftype == "double") cpp_type = "viss::Dec";
                else if (ftype == "int" || ftype == "i32" || ftype == "i64") cpp_type = "viss::Int";
                else if (ftype == "str" || ftype == "string") cpp_type = "viss::Str";
                else if (ftype == "bool") cpp_type = "viss::Bool";
                else if (ftype == "bytes") cpp_type = "viss::Bytes";
                else if (ftype == "bits") cpp_type = "viss::Bits";
                else if (ftype == "list") cpp_type = "viss::List<viss::Int>";

                if (def_val.empty()) {
                    if (cpp_type == "viss::Dec") def_val = "0.0";
                    else if (cpp_type == "viss::Int") def_val = "0";
                    else if (cpp_type == "viss::Str") def_val = "\"\"";
                    else if (cpp_type == "viss::Bool") def_val = "false";
                    else def_val = "{}";
                } else {
                    def_val = replaceModuleCalls(def_val, imported_aliases);
                    def_val = replaceAll(replaceAll(def_val, "@", ""), "&", "");
                    def_val = applyStaticTransforms(def_val);
                }
                struct_fields[current_struct_name].push_back({fname, cpp_type, def_val});
                declared_vars.insert(fname);
                current_target->push_back("    " + cpp_type + " " + fname + " = " + def_val + ";");
                continue;
            }

            // Inferred field: @x = 0.0 or x = 0.0
            std::smatch m_ifld;
            if (std::regex_match(stripped, m_ifld, std::regex(R"(^@?([a-zA-Z0-9_]+)\s*=\s*([^;]+);?$)"))) {
                std::string fname = m_ifld[1].str();
                std::string val = trim(m_ifld[2].str());
                std::string cpp_type = "viss::Dec";
                std::string def_val = replaceModuleCalls(val, imported_aliases);
                def_val = replaceAll(replaceAll(def_val, "@", ""), "&", "");
                def_val = applyStaticTransforms(def_val);
                if (val == "true" || val == "false") cpp_type = "viss::Bool";
                else if (startsWith(val, "\"") || startsWith(val, "__VISS_STR_LIT_")) cpp_type = "viss::Str";
                else if (val.find('.') != std::string::npos) cpp_type = "viss::Dec";
                else {
                    bool all_dig = true;
                    for (char c : val) {
                        if (!std::isdigit((unsigned char)c) && c != '-') { all_dig = false; break; }
                    }
                    if (all_dig) cpp_type = "viss::Int";
                }
                struct_fields[current_struct_name].push_back({fname, cpp_type, def_val});
                declared_vars.insert(fname);
                current_target->push_back("    " + cpp_type + " " + fname + " = " + def_val + ";");
                continue;
            }
        }

        // 1.6 Enum definition: enum Name { or !enum Name {
        std::smatch m_enm;
        if (std::regex_match(stripped, m_enm, std::regex(R"(^(?:!enum|enum)\s+([a-zA-Z0-9_]+)\s*\{$)"))) {
            std::string ename = m_enm[1].str();
            current_enum_name = ename;
            known_classes.insert(ename);
            declared_vars.insert(ename);
            imported_aliases.push_back(ename);
            block_stack.push_back({"enum", ename});
            enum_fields[ename] = {};
            current_target = &classes_section;
            continue;
        }

        // Inside enum: parse enum items
        if (!current_enum_name.empty() && stripped != "}") {
            std::string line_clean = stripped;
            if (endsWith(line_clean, ",") || endsWith(line_clean, ";")) {
                line_clean = line_clean.substr(0, line_clean.size() - 1);
            }
            line_clean = trim(line_clean);
            if (!line_clean.empty()) {
                size_t eq = line_clean.find('=');
                std::string k, v;
                if (eq != std::string::npos) {
                    k = trim(line_clean.substr(0, eq));
                    v = trim(line_clean.substr(eq + 1));
                } else {
                    k = line_clean;
                    v = "";
                }
                k = replaceAll(replaceAll(k, "@", ""), "&", "");
                enum_fields[current_enum_name].push_back({k, v});
            }
            continue;
        }

        // 2. Class definition: !class Name [:: Parent] { or class Name [:: Parent] {
        std::smatch m_cls;
        if (std::regex_match(stripped, m_cls, std::regex(R"(^(?:!class|class)\s+([a-zA-Z0-9_]+)(?:\s*::\s*([a-zA-Z0-9_]+))?\s*\{$)"))) {
            std::string cname = m_cls[1].str();
            std::string pnames = m_cls[2].str();
            current_class_name = cname;
            known_classes.insert(cname);
            declared_vars.enter_block();
            block_stack.push_back({"class", cname});
            current_target = &classes_section;
            if (!pnames.empty()) {
                current_target->push_back("struct _cls_" + cname + " : public " + pnames + " {");
            } else {
                current_target->push_back("struct _cls_" + cname + " {");
            }
            continue;
        }

        // 3. Function definition: !main {, !func main(), !func name(...) or func name(...)
        if (std::regex_match(stripped, std::regex(R"(^(!main|main|!func\s+main\s*\(\s*\)|func\s+main\s*\(\s*\))\s*\{$)"))) {
            declared_vars.enter_function();
            block_stack.push_back({"func", "main"});
            current_target = &main_section;
            current_target->push_back("int main() {");
            continue;
        }

        std::smatch m_fn;
        if (std::regex_match(stripped, m_fn, std::regex(R"(^!?(?:async[\.\s]+)?func\s+([a-zA-Z0-9_?!]+)\s*\(([^)]*)\)(?:\s+(?:to|as|->)\s+([a-zA-Z0-9_<>]+))?\s*\{$)"))) {
            bool is_async = (stripped.find("async.") != std::string::npos || stripped.find("async ") != std::string::npos);
            std::string raw_name = m_fn[1].str();
            std::string fname = replaceAll(replaceAll(raw_name, "?", "_q"), "!", "_bang");
            std::string params_str = m_fn[2].str();
            std::string ret_type = m_fn[3].str();
            declared_vars.enter_function();
            if (!current_struct_name.empty()) {
                for (const auto& f : struct_fields[current_struct_name]) {
                    declared_vars.insert(f.name);
                }
            }

            // Constructor inside class
            if (!current_class_name.empty() && fname == "main") {
                block_stack.push_back({"func", "ctor"});
                current_target->push_back("    _cls_" + current_class_name + "(" + params_str + ") {");
                continue;
            }

            // Parse parameters
            std::vector<std::string> param_tokens = splitByChar(params_str, ',');
            std::vector<std::string> cpp_params;
            std::vector<std::string> tmpl_params;
            for (size_t p_idx = 0; p_idx < param_tokens.size(); ++p_idx) {
                std::string p = trim(param_tokens[p_idx]);
                if (p.empty()) continue;
                std::string default_val = "";
                size_t eq_pos = p.find('=');
                if (eq_pos != std::string::npos) {
                    default_val = trim(p.substr(eq_pos + 1));
                    p = trim(p.substr(0, eq_pos));
                }
                std::smatch m_as;
                if (std::regex_match(p, m_as, std::regex(R"(^@?([a-zA-Z0-9_]+)\s*(?:as|:)\s*([a-zA-Z0-9_<>]+)$)"))) {
                    std::string pname = m_as[1].str();
                    std::string ptype = resolveVissCppType(m_as[2].str());
                    declared_vars.insert(pname);
                    std::string decl = ptype + " " + pname + (default_val.empty() ? "" : " = " + default_val);
                    cpp_params.push_back(decl);
                } else {
                    std::string pname = replaceAll(replaceAll(p, "@", ""), "&", "");
                    declared_vars.insert(pname);
                    std::string tname = "_T" + std::to_string(p_idx) + "_" + pname;
                    tmpl_params.push_back("typename " + tname);
                    std::string decl = tname + " " + pname + (default_val.empty() ? "" : " = " + default_val);
                    cpp_params.push_back(decl);
                }
            }

            std::string tmpl_clause = "";
            if (!tmpl_params.empty()) {
                tmpl_clause = "template<";
                for (size_t ti = 0; ti < tmpl_params.size(); ++ti) {
                    tmpl_clause += tmpl_params[ti] + (ti + 1 < tmpl_params.size() ? ", " : "> ");
                }
            }

            std::string joined_params = "";
            for (size_t pi = 0; pi < cpp_params.size(); ++pi) {
                joined_params += cpp_params[pi] + (pi + 1 < cpp_params.size() ? ", " : "");
            }

            std::string cpp_ret = "auto";
            if (!ret_type.empty()) {
                cpp_ret = resolveVissCppType(ret_type);
            }

            if (is_async) {
                block_stack.push_back({"async_func", fname});
                std::string resolved_ret = (!ret_type.empty()) ? resolveVissCppType(ret_type) : "";
                std::string async_ret = (!resolved_ret.empty()) ? ("viss::async::Task<" + resolved_ret + ">") : "auto";
                std::string lambda_ret = (!resolved_ret.empty()) ? (" -> " + resolved_ret) : "";
                current_target->push_back(tmpl_clause + "inline " + async_ret + " " + fname + "(" + joined_params + ") { return viss::async::spawn([=]()" + lambda_ret + " {");
            } else if (!current_struct_name.empty()) {
                block_stack.push_back({"struct_func", fname});
                current_target = &classes_section;
                current_target->push_back("    " + tmpl_clause + "inline " + cpp_ret + " " + fname + "(" + joined_params + ") {");
            } else {
                block_stack.push_back({"func", fname});
                current_target = current_class_name.empty() ? &functions_section : &classes_section;
                std::string prefix = current_class_name.empty() ? "" : "    static ";
                current_target->push_back(prefix + tmpl_clause + "inline " + cpp_ret + " " + fname + "(" + joined_params + ") {");
            }
            continue;
        }

        // 4. Block closure: }
        if (stripped == "}") {
            declared_vars.exit_block();
            if (!block_stack.empty()) {
                auto top = block_stack.back();
                block_stack.pop_back();
                if (top.first == "struct") {
                    std::string sname = top.second;
                    auto& flds = struct_fields[sname];
                    current_target->push_back("    " + sname + "() = default;");
                    if (!flds.empty()) {
                        std::string ctor_params = "";
                        std::string ctor_inits = "";
                        for (size_t fi = 0; fi < flds.size(); ++fi) {
                            std::string p = "_p" + std::to_string(fi) + "_" + flds[fi].name;
                            ctor_params += flds[fi].cpp_type + " " + p + (fi + 1 < flds.size() ? ", " : "");
                            ctor_inits += flds[fi].name + "(" + p + ")" + (fi + 1 < flds.size() ? ", " : "");
                        }
                        current_target->push_back("    " + sname + "(" + ctor_params + ") : " + ctor_inits + " {}");
                    }
                    current_target->push_back("};");
                    current_struct_name = "";
                    current_target = block_stack.empty() ? &main_section : &functions_section;
                    continue;
                } else if (top.first == "struct_func") {
                    current_target->push_back("    }");
                    current_target = &classes_section;
                    continue;
                } else if (top.first == "enum") {
                    std::string ename = top.second;
                    auto& flds = enum_fields[ename];
                    current_target->push_back("struct _enum_" + ename + " {");
                    current_target->push_back("    int _v = 0;");
                    current_target->push_back("    _enum_" + ename + "() : _v(0) {}");
                    current_target->push_back("    _enum_" + ename + "(int v) : _v(v) {}");
                    current_target->push_back("    operator int() const { return _v; }");
                    int cur_idx = 0;
                    for (auto& f : flds) {
                        std::string val = f.second.empty() ? std::to_string(cur_idx) : f.second;
                        if (!f.second.empty()) {
                            try { cur_idx = std::stoi(f.second); } catch(...) {}
                        }
                        current_target->push_back("    static const int " + f.first + " = " + val + ";");
                        cur_idx++;
                    }
                    current_target->push_back("};");
                    current_target->push_back("using " + ename + " = _enum_" + ename + ";");
                    current_enum_name = "";
                    current_target = block_stack.empty() ? &main_section : &functions_section;
                    continue;
                } else if (top.first == "class") {
                    current_target->push_back("};");
                    current_target->push_back("using " + top.second + " = _cls_" + top.second + ";");
                    current_class_name = "";
                    current_target = block_stack.empty() ? &main_section : &functions_section;
                    continue;
                } else if (top.first == "async_func") {
                    current_target->push_back("}); }");
                    current_target = block_stack.empty() ? &main_section : &functions_section;
                    continue;
                } else if (top.first == "match_case") {
                    current_target->push_back("break; }");
                    continue;
                }
            }
            current_target->push_back("}");
            if (block_stack.empty()) {
                current_target = &main_section;
            } else if (block_stack.back().first == "struct" || block_stack.back().first == "class" || block_stack.back().first == "enum") {
                current_target = &classes_section;
            }
            continue;
        }

        // 5. Pattern Matching: ?match expr {, match expr {, switch expr {
        std::smatch m_match;
        if (std::regex_match(stripped, m_match, std::regex(R"(^(?:\?match|match|switch)\s+([^\{]+)\{$)"))) {
            declared_vars.enter_block();
            std::string expr = trim(replaceAll(replaceAll(m_match[1].str(), "@", ""), "&", ""));
            expr = replaceModuleCalls(expr, imported_aliases);
            expr = applyStaticTransforms(expr);
            block_stack.push_back({"match", expr});
            current_target->push_back("switch (" + expr + ") {");
            continue;
        }
        if (std::regex_match(stripped, m_match, std::regex(R"(^case\s+([^\{]+)\{$)"))) {
            declared_vars.enter_block();
            std::string val = trim(m_match[1].str());
            val = replaceModuleCalls(val, imported_aliases);
            val = replaceAll(replaceAll(val, "@", ""), "&", "");
            val = applyStaticTransforms(val);
            if (val == "_") {
                block_stack.push_back({"match_case", "default"});
                current_target->push_back("default: {");
            } else {
                block_stack.push_back({"match_case", val});
                current_target->push_back("case " + val + ": {");
            }
            continue;
        }
        if (std::regex_match(stripped, m_match, std::regex(R"(^(?:default|else)\s*\{$)"))) {
            if (!block_stack.empty() && block_stack.back().first == "match") {
                declared_vars.enter_block();
                block_stack.push_back({"match_case", "default"});
                current_target->push_back("default: {");
                continue;
            }
        }

        // 6. Loops: !for @i in 0..10 {, ?for, for, !while, ?while, while
        std::smatch m_loop;
        if (std::regex_match(stripped, m_loop, std::regex(R"(^[!?]?for\s+@?([a-zA-Z0-9_]+)\s+in\s+([^.]+)\.\.([^\{]+)\{$)"))) {
            std::string v = m_loop[1].str();
            declared_vars.enter_block();
            declared_vars.insert(v);
            std::string s = trim(replaceAll(replaceAll(m_loop[2].str(), "@", ""), "&", ""));
            std::string e = trim(replaceAll(replaceAll(m_loop[3].str(), "@", ""), "&", ""));
            block_stack.push_back({"for", v});
            current_target->push_back("for (viss::Int " + v + " = (" + s + "); " + v + " < (" + e + "); ++" + v + ") {");
            continue;
        }
        if (std::regex_match(stripped, m_loop, std::regex(R"(^[!?]?for\s+@?([a-zA-Z0-9_]+)\s+in\s+([^\{]+)\{$)"))) {
            std::string v = m_loop[1].str();
            declared_vars.enter_block();
            declared_vars.insert(v);
            std::string coll = trim(replaceAll(replaceAll(m_loop[2].str(), "@", ""), "&", ""));
            block_stack.push_back({"for", v});
            current_target->push_back("for (auto& " + v + " : " + coll + ") {");
            continue;
        }
        if (std::regex_match(stripped, m_loop, std::regex(R"(^[!?]?while(?:\s*\((.*)\)|\s+(.+?))\s*\{$)"))) {
            declared_vars.enter_block();
            std::string cond = m_loop[1].matched ? m_loop[1].str() : m_loop[2].str();
            std::string err;
            cond = transformExpression(cond, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            block_stack.push_back({"while", "while"});
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("while (" + cond + ") {");
            continue;
        }

        // 7. Logic: ?if, ?else if, ?else, ?try, ?expect, ?error
        if (stripped == "?try {" || stripped == "try {") {
            declared_vars.enter_block();
            block_stack.push_back({"try", "try"});
            current_target->push_back("try {");
            continue;
        }
        std::smatch m_exp;
        if (std::regex_match(stripped, m_exp, std::regex(R"(^\?expect\s+([a-zA-Z0-9_]+)\s+as\s+@?([a-zA-Z0-9_]+)\s*\{$)"))) {
            std::string ename = m_exp[2].str();
            declared_vars.enter_block();
            declared_vars.insert(ename);
            block_stack.push_back({"expect", ename});
            current_target->push_back("catch (const std::exception& _err) { viss::Exception " + ename + "(_err.what());");
            continue;
        }
        std::smatch m_if;
        if (std::regex_match(stripped, m_if, std::regex(R"(^(?:\}\s*)?(?:\?else\s+if|else\s+if|\?elif|elif)(?:\s*\((.*)\)|\s+(.+?))\s*\{$)"))) {
            declared_vars.enter_block();
            std::string cond = m_if[1].matched ? m_if[1].str() : m_if[2].str();
            std::string err;
            cond = transformExpression(cond, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            bool has_leading_brace = startsWith(stripped, "}");
            if (has_leading_brace && !block_stack.empty() && (block_stack.back().first == "if" || block_stack.back().first == "else_if")) {
                block_stack.pop_back();
            }
            block_stack.push_back({"else_if", "else_if"});
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            if (has_leading_brace) {
                current_target->push_back("} else if (" + cond + ") {");
            } else {
                current_target->push_back("else if (" + cond + ") {");
            }
            continue;
        }
        if (std::regex_match(stripped, m_if, std::regex(R"(^(?:\?if|if)(?:\s*\((.*)\)|\s+(.+?))\s*\{$)"))) {
            declared_vars.enter_block();
            std::string cond = m_if[1].matched ? m_if[1].str() : m_if[2].str();
            std::string err;
            cond = transformExpression(cond, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            block_stack.push_back({"if", "if"});
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("if (" + cond + ") {");
            continue;
        }
        if (stripped == "} else {" || stripped == "?else {" || stripped == "else {" || stripped == "} ?else {") {
            declared_vars.enter_block();
            bool has_leading_brace = startsWith(stripped, "}");
            if (has_leading_brace && !block_stack.empty() && (block_stack.back().first == "if" || block_stack.back().first == "else_if")) {
                block_stack.pop_back();
            }
            block_stack.push_back({"else", "else"});
            if (has_leading_brace) {
                current_target->push_back("} else {");
            } else {
                current_target->push_back("else {");
            }
            continue;
        }
        std::smatch m_err;
        if (std::regex_match(stripped, m_err, std::regex(R"(^\?error\s+([^;]+);?$)"))) {
            current_target->push_back("throw viss::Exception(" + m_err[1].str() + ");");
            continue;
        }

        // 8. Buffer Creation: &name create | bytes/bits/hybrid/grid/bytemask/colormask, ...;
        std::smatch m_buf;
        if (std::regex_match(stripped, m_buf, std::regex(R"(^&([a-zA-Z0-9_]+)\s+create\s*\|\s*(?:hybrid\s*,\s*)?bytes\s*,\s*(\d+)\s*,\s*bi(?:t|te)s\s*,\s*(\d+);?$)"))) {
            current_target->push_back("viss::Hybrid " + m_buf[1].str() + "(" + m_buf[2].str() + ", " + m_buf[3].str() + ");");
            declared_vars.insert(m_buf[1].str());
            continue;
        }
        if (std::regex_match(stripped, m_buf, std::regex(R"(^&([a-zA-Z0-9_]+)\s+create\s*\|\s*(?:hybrid\s*,\s*)?bi(?:t|te)s\s*,\s*(\d+)\s*,\s*bytes\s*,\s*(\d+);?$)"))) {
            current_target->push_back("viss::Hybrid " + m_buf[1].str() + "(" + m_buf[3].str() + ", " + m_buf[2].str() + ");");
            declared_vars.insert(m_buf[1].str());
            continue;
        }
        if (std::regex_match(stripped, m_buf, std::regex(R"(^&([a-zA-Z0-9_]+)\s+create\s*\|\s*grid(?:\s*,\s*|\s+)([^;]+);?$)"))) {
            std::string vname = m_buf[1].str();
            std::string args_str = m_buf[2].str();
            std::regex num_re(R"(\d+)");
            std::sregex_iterator next(args_str.begin(), args_str.end(), num_re);
            std::sregex_iterator end;
            std::vector<int> nums;
            while (next != end) { nums.push_back(std::stoi((next++)->str())); }
            std::string pairs = "{";
            for (size_t ni = 0; ni < nums.size(); ni += 2) {
                if (ni + 1 < nums.size()) pairs += "{" + std::to_string(nums[ni]) + ", " + std::to_string(nums[ni+1]) + "}";
                else pairs += "{" + std::to_string(nums[ni]) + ", 1}";
                if (ni + 2 < nums.size()) pairs += ", ";
            }
            pairs += "}";
            current_target->push_back("viss::Grid " + vname + "(" + pairs + ");");
            declared_vars.insert(vname);
            continue;
        }
        if (std::regex_match(stripped, m_buf, std::regex(R"(^&([a-zA-Z0-9_]+)\s+create\s*\|\s*(bytes|bits|bites|bytemask|colormask|mask)(?:\s*,\s*([^;]+))?;?$)"))) {
            std::string vname = m_buf[1].str();
            std::string btype = m_buf[2].str();
            std::string sz = m_buf[3].str();
            declared_vars.insert(vname);
            if (btype == "bytes" || btype == "bytemask" || btype == "mask") {
                std::string size_val = sz.empty() ? "1024" : sz;
                current_target->push_back("viss::Bytes " + vname + "(" + size_val + ");");
            } else if (btype == "colormask") {
                std::string size_val = sz.empty() ? "48" : "(" + sz + ") * 3";
                current_target->push_back("viss::Bytes " + vname + "(" + size_val + ");");
            } else if (btype == "bits" || btype == "bites") {
                std::string size_val = sz.empty() ? "8192" : sz;
                current_target->push_back("viss::Bits " + vname + "(" + size_val + ");");
            }
            continue;
        }

        // 9. Grid Operations: &name grid set, &name grid edit, etc.
        std::smatch m_gset;
        if (std::regex_match(stripped, m_gset, std::regex(R"(^&([a-zA-Z0-9_]+)\s+grid\s+set(?:\s*,\s*|\s+)([^;]+);?$)"))) {
            std::string vname = m_gset[1].str();
            std::string args_str = m_gset[2].str();
            std::regex num_re(R"(\d+)");
            std::sregex_iterator next(args_str.begin(), args_str.end(), num_re);
            std::sregex_iterator end;
            std::vector<int> nums;
            while (next != end) { nums.push_back(std::stoi((next++)->str())); }
            std::string pairs = "{";
            for (size_t ni = 0; ni < nums.size(); ni += 2) {
                if (ni + 1 < nums.size()) pairs += "{" + std::to_string(nums[ni]) + ", " + std::to_string(nums[ni+1]) + "}";
                else pairs += "{" + std::to_string(nums[ni]) + ", 1}";
                if (ni + 2 < nums.size()) pairs += ", ";
            }
            pairs += "}";
            current_target->push_back(vname + ".grid_set(" + pairs + ");");
            continue;
        }
        std::smatch m_gedit;
        if (std::regex_match(stripped, m_gedit, std::regex(R"(^&([a-zA-Z0-9_]+)\s+grid\s+edit\s*\{([\s\S]+)\};?$)"))) {
            std::string vname = m_gedit[1].str();
            std::string body = m_gedit[2].str();
            std::vector<std::string> raw_rows = splitByChar(body, ';');
            std::vector<std::string> rows_cpp;
            for (const auto& r : raw_rows) {
                std::vector<std::string> sub_rows = splitByChar(r, ':');
                for (const auto& sr : sub_rows) {
                    std::string row_clean = trim(sr);
                    if (row_clean.empty()) continue;
                    std::vector<std::string> tok_list = splitByChar(row_clean, ',');
                    std::string row_str = "{";
                    for (size_t ti = 0; ti < tok_list.size(); ++ti) {
                        std::string tok = trim(tok_list[ti]);
                        tok = replaceAll(replaceAll(tok, "@", ""), "&", "");
                        tok = applyStaticTransforms(tok);
                        row_str += tok + (ti + 1 < tok_list.size() ? ", " : "");
                    }
                    row_str += "}";
                    rows_cpp.push_back(row_str);
                }
            }
            std::string all_rows = "{";
            for (size_t ri = 0; ri < rows_cpp.size(); ++ri) {
                all_rows += rows_cpp[ri] + (ri + 1 < rows_cpp.size() ? ", " : "");
            }
            all_rows += "}";
            current_target->push_back(vname + ".grid_edit(" + all_rows + ");");
            continue;
        }
        std::smatch m_gcmd;
        if (std::regex_match(stripped, m_gcmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+grid\s+(print|show|dump|clear|invert|flip_h|flip_v);?$)"))) {
            std::string vname = m_gcmd[1].str();
            std::string cmd = m_gcmd[2].str();
            if (cmd == "print" || cmd == "show" || cmd == "dump") current_target->push_back(vname + ".grid_print();");
            else if (cmd == "clear") current_target->push_back(vname + ".grid_clear();");
            else if (cmd == "invert") current_target->push_back(vname + ".grid_invert();");
            else current_target->push_back(vname + "." + cmd + "();");
            continue;
        }
        if (std::regex_match(stripped, m_gcmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+grid\s+fill\s+([^;]+);?$)"))) {
            current_target->push_back(m_gcmd[1].str() + ".grid_fill(" + trim(m_gcmd[2].str()) + ");");
            continue;
        }
        if (std::regex_match(stripped, m_gcmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+grid\s+resize\s+([^;]+);?$)"))) {
            current_target->push_back(m_gcmd[1].str() + ".grid_resize(" + trim(m_gcmd[2].str()) + ");");
            continue;
        }

        // 10. Stream Write: &name write <type> <val> [at <offset>];
        std::smatch m_wr;
        if (std::regex_match(stripped, m_wr, std::regex(R"(^&([a-zA-Z0-9_]+)\s+write\s+(str|string|int|dec|double|float|bool|byte|u8|i8|u16|i16|u32|i32|u64|i64|bytes)\s+(.+?)(?:\s+at\s+([^;]+))?;?$)"))) {
            std::string vname = m_wr[1].str();
            std::string wtype = m_wr[2].str();
            std::string val = trim(m_wr[3].str());
            std::string offset = m_wr[4].str();
            val = replaceModuleCalls(val, imported_aliases);
            val = replaceAll(replaceAll(val, "@", ""), "&", "");
            val = applyStaticTransforms(val);
            std::string off_arg = offset.empty() ? "" : ", " + trim(replaceAll(replaceAll(offset, "@", ""), "&", ""));

            if (wtype == "str" || wtype == "string") current_target->push_back(vname + ".write_str(" + val + off_arg + ");");
            else if (wtype == "int") current_target->push_back(vname + ".write_int(" + val + off_arg + ");");
            else if (wtype == "dec" || wtype == "double") current_target->push_back(vname + ".write_dec(" + val + off_arg + ");");
            else if (wtype == "float") current_target->push_back(vname + ".write_float(" + val + off_arg + ");");
            else if (wtype == "bool") current_target->push_back(vname + ".write_bool(" + val + off_arg + ");");
            else if (wtype == "byte" || wtype == "u8") current_target->push_back(vname + ".write_u8(" + val + off_arg + ");");
            else if (wtype == "i8") current_target->push_back(vname + ".write_i8(" + val + off_arg + ");");
            else if (wtype == "u16") current_target->push_back(vname + ".write_u16(" + val + off_arg + ");");
            else if (wtype == "i16") current_target->push_back(vname + ".write_i16(" + val + off_arg + ");");
            else if (wtype == "u32") current_target->push_back(vname + ".write_u32(" + val + off_arg + ");");
            else if (wtype == "i32") current_target->push_back(vname + ".write_i32(" + val + off_arg + ");");
            else if (wtype == "u64") current_target->push_back(vname + ".write_u64(" + val + off_arg + ");");
            else if (wtype == "i64") current_target->push_back(vname + ".write_i64(" + val + off_arg + ");");
            else if (wtype == "bytes") current_target->push_back(vname + ".write_bytes(" + val + off_arg + ");");
            continue;
        }

        // Auto Stream Write: &name write <val> [at <offset>];
        if (std::regex_match(stripped, m_wr, std::regex(R"(^&([a-zA-Z0-9_]+)\s+write\s+(.+?)(?:\s+at\s+([^;]+))?;?$)"))) {
            std::string vname = m_wr[1].str();
            std::string val = trim(m_wr[2].str());
            std::string offset = m_wr[3].str();
            val = replaceModuleCalls(val, imported_aliases);
            val = replaceAll(replaceAll(val, "@", ""), "&", "");
            val = applyStaticTransforms(val);
            std::string off_arg = offset.empty() ? "" : ", " + trim(replaceAll(replaceAll(offset, "@", ""), "&", ""));
            if (startsWith(val, "\"") || startsWith(val, "i\"") || startsWith(val, "viss::Str")) {
                current_target->push_back(vname + ".write_str(" + val + off_arg + ");");
            } else if (val == "true" || val == "false") {
                current_target->push_back(vname + ".write_bool(" + val + off_arg + ");");
            } else if (val.find('.') != std::string::npos && std::regex_match(val, std::regex(R"(^-?\d+\.\d+$)"))) {
                current_target->push_back(vname + ".write_dec(" + val + off_arg + ");");
            } else {
                current_target->push_back(vname + ".write_int(" + val + off_arg + ");");
            }
            continue;
        }

        // 11. Stream Cursor & Buffer Commands: seek, rewind, dump, clear, invert, fill, bitfield
        std::smatch m_cmd;
        if (std::regex_match(stripped, m_cmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+seek\s+([^;]+);?$)"))) {
            current_target->push_back(m_cmd[1].str() + ".seek(" + trim(replaceAll(replaceAll(m_cmd[2].str(), "@", ""), "&", "")) + ");");
            continue;
        }
        if (std::regex_match(stripped, m_cmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+rewind;?$)"))) {
            current_target->push_back(m_cmd[1].str() + ".rewind();");
            continue;
        }
        if (std::regex_match(stripped, m_cmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+(dump|hexdump);?$)"))) {
            current_target->push_back(m_cmd[1].str() + ".dump();");
            continue;
        }
        if (std::regex_match(stripped, m_cmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+clear;?$)"))) {
            current_target->push_back(m_cmd[1].str() + ".clear();");
            continue;
        }
        if (std::regex_match(stripped, m_cmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+invert;?$)"))) {
            current_target->push_back(m_cmd[1].str() + ".invert();");
            continue;
        }
        if (std::regex_match(stripped, m_cmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+fill\s+([^;]+);?$)"))) {
            current_target->push_back(m_cmd[1].str() + ".fill(" + trim(m_cmd[2].str()) + ");");
            continue;
        }
        if (std::regex_match(stripped, m_cmd, std::regex(R"(^&([a-zA-Z0-9_]+)\s+bitfield\s+set\s+([^,]+),\s*([^,]+),\s*([^;]+);?$)"))) {
            current_target->push_back(m_cmd[1].str() + ".set_bitfield(" + trim(m_cmd[2].str()) + ", " + trim(m_cmd[3].str()) + ", " + trim(m_cmd[4].str()) + ");");
            continue;
        }

        // 12. Stream Read: @var = &name read <type>[(<len>)] [at <offset>] [| <ptype>];
        std::smatch m_rd;
        if (std::regex_match(stripped, m_rd, std::regex(R"(^@([a-zA-Z0-9_]+)\s*=\s*&([a-zA-Z0-9_]+)\s+read\s+(str|string|int|dec|double|float|bool|byte|u8|i8|u16|i16|u32|i32|u64|i64|bytes)(?:\s*\(\s*([^)]*)\s*\)|\s+len\s+([a-zA-Z0-9_]+))?(?:\s+at\s+([^;|]+))?(?:\s*\|\s*([a-zA-Z0-9_]+))?;?$)"))) {
            std::string var_name = m_rd[1].str();
            std::string buf_name = m_rd[2].str();
            std::string rtype = m_rd[3].str();
            std::string len_arg = m_rd[4].str().empty() ? m_rd[5].str() : m_rd[4].str();
            std::string at_arg = m_rd[6].str();
            std::string pipe_type = m_rd[7].str();

            std::string off_clean = at_arg.empty() ? "-1" : trim(replaceAll(replaceAll(at_arg, "@", ""), "&", ""));
            std::string len_clean = len_arg.empty() ? "" : trim(replaceAll(replaceAll(len_arg, "@", ""), "&", ""));

            std::string call_expr = "";
            std::string cpp_type = "auto";

            if (rtype == "str" || rtype == "string") {
                std::string l_param = len_clean.empty() ? "-1" : len_clean;
                call_expr = buf_name + ".read_str(" + l_param + ", " + off_clean + ")";
                cpp_type = "viss::Str";
            } else if (rtype == "int") {
                call_expr = buf_name + ".read_int(" + off_clean + ")";
                cpp_type = "viss::Int";
            } else if (rtype == "dec" || rtype == "double") {
                call_expr = buf_name + ".read_dec(" + off_clean + ")";
                cpp_type = "viss::Dec";
            } else if (rtype == "float") {
                call_expr = buf_name + ".read_float(" + off_clean + ")";
                cpp_type = "viss::Dec";
            } else if (rtype == "bool") {
                call_expr = buf_name + ".read_bool(" + off_clean + ")";
                cpp_type = "viss::Bool";
            } else if (rtype == "byte" || rtype == "u8") {
                call_expr = "(viss::Int)" + buf_name + ".read_u8(" + off_clean + ")";
                cpp_type = "viss::Int";
            } else if (rtype == "i8") {
                call_expr = "(viss::Int)" + buf_name + ".read_i8(" + off_clean + ")";
                cpp_type = "viss::Int";
            } else if (rtype == "u16" || rtype == "i16" || rtype == "u32" || rtype == "i32" || rtype == "u64" || rtype == "i64") {
                call_expr = "(viss::Int)" + buf_name + ".read_" + rtype + "(" + off_clean + ")";
                cpp_type = "viss::Int";
            } else if (rtype == "bytes") {
                std::string l_param = len_clean.empty() ? "0" : len_clean;
                call_expr = buf_name + ".read_bytes(" + l_param + ", " + off_clean + ")";
                cpp_type = "viss::Bytes";
            }

            if (!pipe_type.empty()) {
                if (pipe_type == "str") cpp_type = "viss::Str";
                else if (pipe_type == "int") cpp_type = "viss::Int";
                else if (pipe_type == "dec" || pipe_type == "double" || pipe_type == "float") cpp_type = "viss::Dec";
                else if (pipe_type == "bool") cpp_type = "viss::Bool";
                else if (pipe_type == "bytes") cpp_type = "viss::Bytes";
            }

            if (declared_vars.count(var_name)) {
                current_target->push_back(var_name + " = " + call_expr + ";");
            } else {
                declared_vars.insert(var_name);
                current_target->push_back(cpp_type + " " + var_name + " = " + call_expr + ";");
            }
            continue;
        }

        // Bitfield Get: @var = &name bitfield get <start>, <count> [| <ptype>];
        std::smatch m_bfg;
        if (std::regex_match(stripped, m_bfg, std::regex(R"(^@([a-zA-Z0-9_]+)\s*=\s*&([a-zA-Z0-9_]+)\s+bitfield\s+get\s+([^,]+),\s*([^;|]+)(?:\s*\|\s*([a-zA-Z0-9_]+))?;?$)"))) {
            std::string var_name = m_bfg[1].str();
            std::string buf_name = m_bfg[2].str();
            std::string s_bit = trim(replaceAll(replaceAll(m_bfg[3].str(), "@", ""), "&", ""));
            std::string c_bit = trim(replaceAll(replaceAll(m_bfg[4].str(), "@", ""), "&", ""));
            std::string call_expr = "(viss::Int)" + buf_name + ".get_bitfield(" + s_bit + ", " + c_bit + ")";
            if (declared_vars.count(var_name)) {
                current_target->push_back(var_name + " = " + call_expr + ";");
            } else {
                declared_vars.insert(var_name);
                current_target->push_back("viss::Int " + var_name + " = " + call_expr + ";");
            }
            continue;
        }

        // 13. Raw Buffer Index Assignment: &name[idx] = val;
        std::smatch m_raw_idx;
        if (std::regex_match(stripped, m_raw_idx, std::regex(R"(^&([a-zA-Z0-9_]+)((?:\[[^\]]+\])+)\s*=\s*(.+?);?$)"))) {
            std::string vname = m_raw_idx[1].str();
            std::string idx_expr = replaceAll(replaceAll(m_raw_idx[2].str(), "@", ""), "&", "");
            std::string val_expr = trim(m_raw_idx[3].str());
            val_expr = replaceModuleCalls(val_expr, imported_aliases);
            val_expr = replaceAll(replaceAll(val_expr, "@", ""), "&", "");
            val_expr = applyStaticTransforms(val_expr);
            current_target->push_back(vname + idx_expr + " = " + val_expr + ";");
            continue;
        }

        // 14. Raw Buffer Assignment: &name = val;
        std::smatch m_raw_assign;
        if (std::regex_match(stripped, m_raw_assign, std::regex(R"(^&([a-zA-Z0-9_]+)\s*=\s*([\s\S]+?);?$)"))) {
            std::string vname = m_raw_assign[1].str();
            std::string val_expr = trim(m_raw_assign[2].str());
            std::string err;
            val_expr = transformExpression(val_expr, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            if (startsWith(val_expr, "[") && endsWith(val_expr, "]")) {
                val_expr = "{" + val_expr.substr(1, val_expr.size() - 2) + "}";
            }
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            if (declared_vars.count(vname)) {
                current_target->push_back(vname + " = " + val_expr + ";");
            } else {
                declared_vars.insert(vname, VissType::Bytes, line_num, true);
                current_target->push_back("auto " + vname + " = " + val_expr + ";");
            }
            continue;
        }

        // 15. Pipe variable declaration: @var = val | type, const; or @var = val | type;
        std::smatch m_pipe;
        if (std::regex_match(stripped, m_pipe, std::regex(R"(^@([a-zA-Z0-9_]+)\s*=\s*([\s\S]+?)\s*\|\s*([a-zA-Z0-9_]+)\s*,\s*const;?$)"))) {
            std::string vname = m_pipe[1].str();
            std::string val = trim(m_pipe[2].str());
            std::string ptype = m_pipe[3].str();
            std::string err;
            val = transformExpression(val, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            declared_vars.insert(vname, parseVissType(ptype), line_num);
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("const auto " + vname + " = " + val + ";");
            continue;
        }

        // 14.4 Multi-assignment / Tuple unpacking: (@a, @b) = (1, 2) or (@a, @b) = @coll or (@a, @b) = (@b, @a)
        std::smatch m_tuple_asn;
        if (std::regex_match(stripped, m_tuple_asn, std::regex(R"(^\(\s*(@[a-zA-Z0-9_]+(?:\s*,\s*@[a-zA-Z0-9_]+)+)\s*\)\s*=\s*([\s\S]+?);?$)"))) {
            std::string vars_str = m_tuple_asn[1].str();
            std::string rhs = trim(m_tuple_asn[2].str());

            std::vector<std::string> var_names;
            std::stringstream ss_vars(vars_str);
            std::string v_item;
            while (std::getline(ss_vars, v_item, ',')) {
                v_item = trim(v_item);
                if (startsWith(v_item, "@")) v_item = v_item.substr(1);
                if (!v_item.empty()) var_names.push_back(v_item);
            }

            if (startsWith(rhs, "(") && endsWith(rhs, ")")) {
                std::string inner_rhs = trim(rhs.substr(1, rhs.size() - 2));
                std::vector<std::string> rhs_items;
                std::string cur = "";
                bool in_q = false;
                int depth = 0;
                for (size_t ci = 0; ci < inner_rhs.size(); ++ci) {
                    char c = inner_rhs[ci];
                    if (c == '"' && (ci == 0 || inner_rhs[ci-1] != '\\')) in_q = !in_q;
                    else if (!in_q) {
                        if (c == '(' || c == '[' || c == '{') depth++;
                        else if (c == ')' || c == ']' || c == '}') depth--;
                        else if (depth == 0 && c == ',') {
                            rhs_items.push_back(trim(cur));
                            cur.clear();
                            continue;
                        }
                    }
                    cur += c;
                }
                if (!cur.empty()) rhs_items.push_back(trim(cur));

                current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
                for (size_t i = 0; i < var_names.size() && i < rhs_items.size(); ++i) {
                    std::string err;
                    std::string t_item = transformExpression(rhs_items[i], imported_aliases, declared_vars, line_num, filename, err);
                    current_target->push_back("auto _t_unpack_" + std::to_string(line_num) + "_" + std::to_string(i) + " = " + t_item + ";");
                }
                for (size_t i = 0; i < var_names.size() && i < rhs_items.size(); ++i) {
                    const auto& v = var_names[i];
                    std::string tmp_val = "_t_unpack_" + std::to_string(line_num) + "_" + std::to_string(i);
                    if (declared_vars.count(v)) {
                        current_target->push_back(v + " = " + tmp_val + ";");
                    } else {
                        declared_vars.insert(v);
                        current_target->push_back("auto " + v + " = " + tmp_val + ";");
                    }
                }
            } else {
                std::string err;
                rhs = transformExpression(rhs, imported_aliases, declared_vars, line_num, filename, err);
                current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
                std::string coll_tmp = "_unpack_coll_" + std::to_string(line_num);
                current_target->push_back("auto " + coll_tmp + " = " + rhs + ";");
                for (size_t i = 0; i < var_names.size(); ++i) {
                    const auto& v = var_names[i];
                    std::string elem_expr = coll_tmp + "[" + std::to_string(i) + "]";
                    if (declared_vars.count(v)) {
                        current_target->push_back(v + " = " + elem_expr + ";");
                    } else {
                        declared_vars.insert(v);
                        current_target->push_back("auto " + v + " = " + elem_expr + ";");
                    }
                }
            }
            continue;
        }

        // 14.5 Explicitly typed variable declaration: @var as type = val; or @var: type = val;
        std::smatch m_as_var;
        if (std::regex_match(stripped, m_as_var, std::regex(R"(^@([a-zA-Z0-9_]+)(?:\s+as\s+|\s*:\s*)([a-zA-Z0-9_<>[\]]+)\s*=\s*([\s\S]+?);?$)"))) {
            std::string vname = m_as_var[1].str();
            std::string ptype = trim(m_as_var[2].str());
            std::string val = trim(m_as_var[3].str());
            std::string cpp_t = resolveVissCppType(ptype);
            if (startsWith(val, "[") && endsWith(val, "]")) {
                std::string inner = trim(val.substr(1, val.size() - 2));
                val = inner.empty() ? (cpp_t + "{}") : (cpp_t + "{" + inner + "}");
            } else if (isDictLiteral(val)) {
                val = transformDictLiteral(val, cpp_t);
            }
            std::string err;
            val = transformExpression(val, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            bool already = declared_vars.count(vname) > 0;
            declared_vars.insert(vname, parseVissType(ptype), line_num);
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back(already ? (vname + " = " + val + ";") : (cpp_t + " " + vname + " = " + val + ";"));
            continue;
        }

        // 15. Pipe variable declaration: @var = val | type, const; or @var = val | type;
        if (std::regex_match(stripped, m_pipe, std::regex(R"(^@([a-zA-Z0-9_]+)\s*=\s*([\s\S]+?)\s*\|\s*([a-zA-Z0-9_<>, \t[\]]+?)(?:,\s*(const))?\s*;?$)"))) {
            std::string vname = m_pipe[1].str();
            std::string val = trim(m_pipe[2].str());
            std::string ptype = trim(m_pipe[3].str());
            bool is_const = m_pipe[4].matched;
            std::string cpp_t = resolveVissCppType(ptype);
            if (is_const) cpp_t = "const " + cpp_t;
            if (startsWith(val, "[") && endsWith(val, "]")) {
                std::string inner = trim(val.substr(1, val.size() - 2));
                val = inner.empty() ? (cpp_t + "{}") : (cpp_t + "{" + inner + "}");
            } else if (isDictLiteral(val)) {
                val = transformDictLiteral(val, cpp_t);
            }
            std::string err;
            val = transformExpression(val, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            bool already = declared_vars.count(vname) > 0;
            declared_vars.insert(vname, parseVissType(ptype), line_num);
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back(already ? (vname + " = " + val + ";") : (cpp_t + " " + vname + " = " + val + ";"));
            continue;
        }

        // 16. Operator assignment: @var += val;
        std::smatch m_op;
        if (std::regex_match(stripped, m_op, std::regex(R"(^@([a-zA-Z0-9_]+)\s*(\+=|-=|\*=|/=|%=)\s*(.+?);?$)"))) {
            std::string vname = m_op[1].str();
            std::string op = m_op[2].str();
            std::string val = trim(m_op[3].str());
            std::string err;
            val = transformExpression(val, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back(vname + " " + op + " " + val + ";");
            continue;
        }

        // 17. Generic variable assignment: @var = val;
        std::smatch m_asn;
        if (std::regex_match(stripped, m_asn, std::regex(R"(^@([a-zA-Z0-9_]+)\s*=\s*([\s\S]+?);?$)"))) {
            std::string vname = m_asn[1].str();
            std::string val = trim(m_asn[2].str());
            if (startsWith(val, "[") && endsWith(val, "]")) {
                std::string inner = trim(val.substr(1, val.size() - 2));
                val = inner.empty() ? "viss::List<viss::Var>{}" : "viss::List{" + inner + "}";
            } else if (isDictLiteral(val)) {
                val = transformDictLiteral(val, "viss::Map<viss::Str, viss::Var>");
            }
            std::string err;
            val = transformExpression(val, imported_aliases, declared_vars, line_num, filename, err);
            if (!err.empty()) {
                std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
                std::cerr << "    " << stripped << "\n";
                std::cerr << "    " << err << "\n";
                return "";
            }
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            if (declared_vars.count(vname)) {
                current_target->push_back(vname + " = " + val + ";");
            } else {
                declared_vars.insert(vname, declared_vars.infer_type_from_literal(val), line_num);
                current_target->push_back("auto " + vname + " = " + val + ";");
            }
            continue;
        }

        // Await statement: !await @task; or await @task;
        std::smatch m_await_stmt;
        if (std::regex_match(stripped, m_await_stmt, std::regex(R"(^!?await\s+([\s\S]+?);?$)"))) {
            std::string task_expr = m_await_stmt[1].str();
            std::string err;
            task_expr = transformExpression(task_expr, imported_aliases, declared_vars, line_num, filename, err);
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("viss::async::await(" + task_expr + ");");
            continue;
        }

        // Control flow keywords: !return, !break, !continue
        if (startsWith(stripped, "!return") || startsWith(stripped, "return")) {
            std::string ret_expr = "";
            if (startsWith(stripped, "!return")) ret_expr = trim(stripped.substr(7));
            else ret_expr = trim(stripped.substr(6));
            if (!ret_expr.empty() && ret_expr.back() == ';') ret_expr.pop_back();
            if (!ret_expr.empty()) {
                ret_expr = transformListLiterals(ret_expr);
                if (isDictLiteral(ret_expr)) {
                    ret_expr = transformDictLiteral(ret_expr, "viss::Map<viss::Str, viss::Var>");
                }
                std::string err;
                ret_expr = transformExpression(ret_expr, imported_aliases, declared_vars, line_num, filename, err);
            }
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("return " + ret_expr + ";");
            continue;
        }
        if (stripped == "!break;" || stripped == "!break" || stripped == "break;" || stripped == "break") {
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("break;");
            continue;
        }
        if (stripped == "!continue;" || stripped == "!continue" || stripped == "continue;" || stripped == "continue") {
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("continue;");
            continue;
        }

        // Control flow keywords: !defer, defer
        std::smatch m_def;
        if (std::regex_match(stripped, m_def, std::regex(R"(^(?:!defer|defer)\s+([\s\S]+?);?$)"))) {
            std::string def_expr = trim(m_def[1].str());
            std::string err;
            def_expr = transformExpression(def_expr, imported_aliases, declared_vars, line_num, filename, err);
            if (!endsWith(def_expr, ";")) def_expr += ";";
            current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
            current_target->push_back("VISS_DEFER([&]() { " + def_expr + " });");
            continue;
        }

        // 18. General statements, method calls & control keywords
        std::string line = stripped;
        std::string err;
        line = transformExpression(line, imported_aliases, declared_vars, line_num, filename, err);
        if (!err.empty()) {
            std::cerr << "\n[Viss " << (err.find("Cannot apply") != std::string::npos ? "TypeError" : "NameError") << "] " << filename << ":" << line_num << "\n";
            std::cerr << "    " << stripped << "\n";
            std::cerr << "    " << err << "\n";
            return "";
        }

        if (!endsWith(line, ";") && !endsWith(line, "{") && !endsWith(line, "}")) {
            line += ";";
        }

        current_target->push_back("#line " + std::to_string(line_num) + " \"" + filename + "\"");
        current_target->push_back(line);
    }

    // Assemble C++ code
    std::string out;
    if (!is_module) {
        for (const auto& l : includes_section) out += l + "\n";
        for (const auto& l : aliases_section) out += l + "\n";
        out += "\n";
    }
    for (const auto& l : classes_section) out += l + "\n";
    out += "\n";
    for (const auto& l : forward_decl_section) out += l + "\n";
    out += "\n";
    for (const auto& l : functions_section) out += l + "\n";
    out += "\n";
    if (!is_module) {
        for (const auto& l : main_section) out += l + "\n";
    }

    // Restore string literals
    out = restoreStringLiterals(out, string_literals);
    return out;
}

// =============================================================================
// 7. SYNTAX VALIDATOR & DIAGNOSTICS
// =============================================================================

bool validateSyntax(const std::string& code, const std::string& filename) {
    int line = 1, col = 1;
    std::vector<std::pair<char, std::pair<int, int>>> delim_stack;
    bool in_string = false;
    char quote_char = 0;

    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];

        // Comments
        if (!in_string && c == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            while (i < code.size() && code[i] != '\n') { i++; col++; }
            if (i < code.size()) { line++; col = 1; }
            continue;
        }

        // Strings
        if (!in_string && (c == '"' || c == '\'')) {
            in_string = true;
            quote_char = c;
            col++;
            continue;
        } else if (in_string && c == quote_char) {
            if (i > 0 && code[i - 1] == '\\') {
                col++;
                continue;
            }
            in_string = false;
            col++;
            continue;
        }

        if (in_string) {
            if (c == '\n') { line++; col = 1; }
            else col++;
            continue;
        }

        // Delimiters
        if (c == '(' || c == '[' || c == '{') {
            delim_stack.push_back({c, {line, col}});
        } else if (c == ')' || c == ']' || c == '}') {
            if (delim_stack.empty()) {
                std::cerr << "[Viss Syntax Error] " << filename << ":" << line << ":" << col << "\n";
                std::cerr << "    Unexpected closing '" << c << "' without matching opening delimiter.\n";
                return false;
            }
            char open_char = delim_stack.back().first;
            auto open_pos = delim_stack.back().second;
            delim_stack.pop_back();

            bool mismatch = (c == ')' && open_char != '(') ||
                            (c == ']' && open_char != '[') ||
                            (c == '}' && open_char != '{');
            if (mismatch) {
                std::cerr << "[Viss Syntax Error] " << filename << ":" << line << ":" << col << "\n";
                std::cerr << "    Mismatched delimiter: found '" << c << "' but expected match for '" 
                          << open_char << "' from line " << open_pos.first << ":" << open_pos.second << ".\n";
                return false;
            }
        }

        if (c == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }

    if (!delim_stack.empty()) {
        auto unclosed = delim_stack.back();
        std::cerr << "[Viss Syntax Error] " << filename << ":" << unclosed.second.first << ":" << unclosed.second.second << "\n";
        std::cerr << "    Unclosed opening delimiter '" << unclosed.first << "' reaching end of file.\n";
        return false;
    }

    return true;
}

// =============================================================================
// 8. CODE FORMATTER
// =============================================================================

std::string formatCode(const std::string& code) {
    auto statements = splitIntoStatements(code);
    std::string out;
    int indent_level = 0;
    auto get_indent = [](int level) -> std::string {
        if (level < 0) level = 0;
        return std::string(level * 4, ' ');
    };

    for (const auto& item : statements) {
        std::string s = trim(item.first);
        if (s.empty()) continue;

        if (startsWith(s, "}")) {
            if (indent_level > 0) indent_level--;
        }

        out += get_indent(indent_level) + s + "\n";

        if (endsWith(s, "{")) {
            indent_level++;
        }
    }
    return out;
}

void sanitizeAndPrintCompilerErrors(const std::string& err_log_path, const std::string& source_viss_path) {
    std::ifstream in(err_log_path);
    if (!in.is_open()) return;

    std::vector<std::string> source_lines;
    std::ifstream src(source_viss_path);
    if (src.is_open()) {
        std::string l;
        while (std::getline(src, l)) source_lines.push_back(l);
    }

    std::string line;
    bool printed_any = false;
    std::regex err_regex(R"(^([^:\s]+):(\d+):(\d+):\s+(error|fatal error):\s+(.*)$)");

    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_match(line, m, err_regex)) {
            std::string file = m[1].str();
            int lnum = std::stoi(m[2].str());
            int col = std::stoi(m[3].str());
            std::string raw_msg = m[5].str();

            std::string clean_msg = raw_msg;
            clean_msg = replaceAll(clean_msg, "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >", "str");
            clean_msg = replaceAll(clean_msg, "std::__cxx11::basic_string<char>", "str");
            clean_msg = replaceAll(clean_msg, "const char*", "str");
            clean_msg = replaceAll(clean_msg, "viss::Str", "str");
            clean_msg = replaceAll(clean_msg, "viss::Int", "int");
            clean_msg = replaceAll(clean_msg, "long long", "int");
            clean_msg = replaceAll(clean_msg, "viss::Dec", "dec");
            clean_msg = replaceAll(clean_msg, "double", "dec");
            clean_msg = replaceAll(clean_msg, "viss::Bool", "bool");
            clean_msg = replaceAll(clean_msg, "viss::Bytes", "bytes");
            clean_msg = replaceAll(clean_msg, "viss::Bits", "bits");
            clean_msg = replaceAll(clean_msg, "viss::Grid", "grid");
            clean_msg = replaceAll(clean_msg, "viss::List", "list");
            clean_msg = replaceAll(clean_msg, "viss::Map", "map");
            clean_msg = replaceAll(clean_msg, "viss::Hybrid", "hybrid");

            std::string err_type = "CompilationError";
            if (clean_msg.find("was not declared in this scope") != std::string::npos) {
                err_type = "NameError";
                std::regex id_re(R"('([^']+)'\s+was not declared)");
                std::smatch id_m;
                if (std::regex_search(clean_msg, id_m, id_re)) {
                    clean_msg = "Identifier '@" + id_m[1].str() + "' is not declared in this scope.";
                }
            } else if (clean_msg.find("no match for 'operator") != std::string::npos) {
                err_type = "TypeError";
                std::regex op_re(R"(no match for 'operator([^']+)'\s+\(operand types are '([^']+)' and '([^']+)'\))");
                std::smatch op_m;
                if (std::regex_search(clean_msg, op_m, op_re)) {
                    clean_msg = "Cannot apply operator '" + op_m[1].str() + "' between '" + op_m[2].str() + "' and '" + op_m[3].str() + "'.";
                }
            } else if (clean_msg.find("cannot convert") != std::string::npos) {
                err_type = "TypeError";
            } else if (clean_msg.find("expected ';'") != std::string::npos) {
                err_type = "SyntaxError";
                clean_msg = "Expected ';' before end of statement.";
            }

            std::cerr << "\n[Viss " << err_type << "] " << file << ":" << lnum << ":" << col << "\n";
            if (lnum > 0 && lnum <= (int)source_lines.size()) {
                std::string code_line = source_lines[lnum - 1];
                std::cerr << "    " << lnum << " | " << code_line << "\n";
                std::string indent(std::to_string(lnum).size() + 3 + (col > 1 ? col - 1 : 0), ' ');
                std::cerr << "    " << indent << "^\n";
            }
            std::cerr << "    " << clean_msg << "\n";
            printed_any = true;
        }
    }

    if (!printed_any) {
        in.clear();
        in.seekg(0);
        std::string raw;
        while (std::getline(in, raw)) std::cerr << raw << "\n";
    }
}

int formatFile(const std::string& path, bool check_only = false) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "[Viss Formatter] Error: File '" << path << "' not found.\n";
        return 1;
    }
    std::stringstream buf; buf << in.rdbuf();
    std::string orig = buf.str();
    std::string formatted = formatCode(orig);
    if (check_only) {
        if (orig == formatted) {
            std::cout << "[Viss Formatter] File '" << path << "' is cleanly formatted. ^_^\n";
            return 0;
        } else {
            std::cerr << "[Viss Formatter] File '" << path << "' needs formatting.\n";
            return 1;
        }
    }
    std::ofstream out(path);
    out << formatted;
    std::cout << "[Viss Formatter] Formatted '" << path << "' successfully! ^_^\n";
    return 0;
}

// =============================================================================
// 9. INTERACTIVE REPL
// =============================================================================

void runRepl();

void printHelp() {
    std::cout << "Viss Programming Language Compiler & Toolchain v" << VERSION << " \"" << CODENAME << "\" (Native)\n\n"
              << "Usage: viss [command] <file.viss> [options]\n\n"
              << "Commands:\n"
              << "  exec <file.viss>          Direct native VM execution (instant 0ms, zero C++/g++ needed!)\n"
              << "  vm <file.viss>            Alias for 'exec'\n"
              << "  run <file.viss>           Transpile, compile, and execute with zero-overhead\n"
              << "  -e \"<code>\"              Evaluate inline Viss expression directly\n"
              << "  eval \"<code>\"            Alias for '-e'\n"
              << "  build <file.viss>         Transpile and compile to standard executable\n"
              << "  bundle <file.viss> [-o]   Produce 100% standalone binary (Zero DLL dependencies)\n"
              << "  sfx <file.viss> [-p dir]  Build single-file SFX installer with embedded archive\n"
              << "  transpile <file.viss>     Emit clean C++17 source code\n"
              << "  check <file.viss>         Fast syntax validator and linter\n"
              << "  fmt [--check] <file.viss> Format Viss source code to canonical style\n"
              << "  repl                      Start interactive Viss shell (or run 'viss' with no args)\n"
              << "  clean                     Remove cached binaries (.viss_cache/)\n"
              << "  version                   Print Viss version\n\n"
              << "Options:\n"
              << "  -o <output.exe>           Specify output binary path\n"
              << "  --gui, --windowed, -w     Build Windows GUI/windowed application (no console window)\n"
              << "  --icon <icon.ico>         Embed application icon via Windows resource (.ico)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        runRepl();
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "repl") {
        runRepl();
        return 0;
    }

    std::string viss_file = "";
    bool run_after = false;

    if (cmd == "-e" || cmd == "eval" || (cmd == "run" && argc > 2 && std::string(argv[2]) == "-e")) {
        int code_start = (cmd == "run") ? 3 : 2;
        if (code_start >= argc) {
            std::cerr << "Usage: viss -e \"<code to evaluate>\"\n";
            return 1;
        }
        std::string raw_code = "";
        for (int i = code_start; i < argc; ++i) {
            if (!raw_code.empty()) raw_code += " ";
            raw_code += argv[i];
        }
        std::string full_code;
        if (raw_code.find("!main") == std::string::npos) {
            full_code = "$import lib \"iostream\" as io\n"
                        "$import lib \"str\" as str\n"
                        "$import lib \"math\" as math\n"
                        "$import lib \"sys\" as sys\n"
                        "$import lib \"fs\" as fs\n"
                        "$import lib \"time\" as time\n"
                        "$import lib \"net\" as net\n\n"
                        "!main {\n    " + raw_code + "\n}\n";
        } else {
            full_code = raw_code;
        }
        fs::path temp_file = fs::temp_directory_path() / "viss_eval_tmp.viss";
        std::ofstream tof(temp_file);
        tof << full_code;
        tof.close();

        viss_file = temp_file.string();
        run_after = true;
    }

    if (cmd == "sfx") {
        if (argc < 3) {
            std::cerr << "Usage: viss sfx <installer.viss> [--payload <folder|zip>] [-o <output.exe>]\n";
            return 1;
        }
        std::string target_file = argv[2];
        if (!fs::exists(target_file)) {
            std::cerr << "Error: File '" << target_file << "' not found.\n";
            return 1;
        }
        std::string payload_path = "";
        std::string out_exe = "";
        for (int ai = 3; ai < argc; ++ai) {
            std::string a = argv[ai];
            if ((a == "--payload" || a == "-p") && ai + 1 < argc) {
                payload_path = argv[++ai];
            } else if (a == "-o" && ai + 1 < argc) {
                out_exe = argv[++ai];
            }
        }
        fs::path p(target_file);
        std::string stem = p.stem().string();
        if (out_exe.empty()) {
            out_exe = stem + "_setup.exe";
        }

        fs::path stub_exe = fs::temp_directory_path() / (stem + "_stub.exe");
        std::string viss_root = getExecutableDir();
        std::string bundle_cmd = "cmd.exe /s /c \"\"" + viss_root + "\\viss.exe\" bundle \"" + target_file + "\" -o \"" + stub_exe.string() + "\"\"";
        std::cout << "[Viss SFX] Compiling installer stub binary...\n";
        int b_ret = std::system(bundle_cmd.c_str());
        if (b_ret != 0 || !fs::exists(stub_exe)) {
            std::cerr << "[Viss SFX Error] Failed to compile installer stub.\n";
            return 1;
        }

        fs::path temp_zip = "";
        bool delete_temp_zip = false;
        if (!payload_path.empty()) {
            if (!fs::exists(payload_path)) {
                std::cerr << "[Viss SFX Error] Payload path '" << payload_path << "' does not exist.\n";
                fs::remove(stub_exe);
                return 1;
            }
            if (fs::is_directory(payload_path)) {
                temp_zip = fs::temp_directory_path() / (stem + "_payload.zip");
                std::cout << "[Viss SFX] Compressing payload folder into ZIP archive...\n";
                std::string zip_cmd = "powershell -WindowStyle Hidden -Command \"Compress-Archive -Path '" + payload_path + "\\*' -DestinationPath '" + temp_zip.string() + "' -Force\"";
                int z_ret = std::system(zip_cmd.c_str());
                if (z_ret != 0 || !fs::exists(temp_zip)) {
                    std::cerr << "[Viss SFX Error] Failed to compress payload folder.\n";
                    fs::remove(stub_exe);
                    return 1;
                }
                delete_temp_zip = true;
            } else {
                temp_zip = payload_path;
            }
        }

        std::cout << "[Viss SFX] Combining PE executable and archive overlay into " << out_exe << "...\n";
        std::ofstream out(out_exe, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "[Viss SFX Error] Cannot open output file: " << out_exe << "\n";
            fs::remove(stub_exe);
            if (delete_temp_zip) fs::remove(temp_zip);
            return 1;
        }
        std::ifstream stub_in(stub_exe, std::ios::binary);
        out << stub_in.rdbuf();
        stub_in.close();

        if (!temp_zip.empty() && fs::exists(temp_zip)) {
            std::ifstream zip_in(temp_zip, std::ios::binary);
            out << zip_in.rdbuf();
            zip_in.close();
        }
        out.close();

        fs::remove(stub_exe);
        if (delete_temp_zip) fs::remove(temp_zip);

        std::uintmax_t out_size = fs::file_size(out_exe);
        std::cout << "[Viss SFX] Standalone self-extracting installer created: " << out_exe
                  << " (" << out_size << " bytes, zero dependencies) ^_^\n";
        return 0;
    }

    bool force_vm = false;
    for (int ai = 1; ai < argc; ++ai) {
        if (std::string(argv[ai]) == "--vm") force_vm = true;
    }
    if (force_vm) {
        std::string target = "";
        std::vector<std::string> pass_args;
        for (int ai = 1; ai < argc; ++ai) {
            std::string a = argv[ai];
            if (a == "--vm" || a == "run") continue;
            if (target.empty()) target = a;
            else pass_args.push_back(a);
        }
        if (!target.empty()) {
            return viss::vm::VissVM::executeFile(target, pass_args);
        }
    }

    if (cmd == "exec" || cmd == "vm") {
        if (argc < 3) { std::cerr << "Usage: viss " << cmd << " <file.viss>\n"; return 1; }
        std::vector<std::string> pass_args;
        for (int ai = 3; ai < argc; ++ai) pass_args.push_back(argv[ai]);
        return viss::vm::VissVM::executeFile(argv[2], pass_args);
    }

    if (cmd == "bundle") {
        if (argc < 3) { std::cerr << "Usage: viss bundle <file.viss> [-o <output.exe>]\n"; return 1; }
        std::string target_file = argv[2];
        if (!fs::exists(target_file)) {
            std::cerr << "Error: File '" << target_file << "' not found.\n";
            return 1;
        }
        std::string out_exe = "";
        for (int ai = 3; ai < argc; ++ai) {
            std::string a = argv[ai];
            if (a == "-o" && ai + 1 < argc) {
                out_exe = argv[++ai];
            }
        }
        if (out_exe.empty()) {
            out_exe = fs::path(target_file).stem().string() + ".exe";
        }

        fs::path p(target_file);
        std::string stem = p.stem().string();
        fs::path file_dir = fs::absolute(p).parent_path();
        fs::path cache_dir = file_dir / ".viss_cache" / stem;
        fs::create_directories(cache_dir);
        fs::path cpp_file = cache_dir / (stem + "_bundle.cpp");

        std::ifstream in(target_file);
        std::stringstream buffer; buffer << in.rdbuf();
        std::string viss_code = buffer.str();

        std::cout << "[Viss Bundler v" << VERSION << " \"" << CODENAME << "\"] Bundling standalone binary: " << out_exe << "...\n";
        std::string cpp_code = transpile(viss_code, p.filename().string(), file_dir.string());
        if (cpp_code.empty()) return 1;

        std::ofstream out(cpp_file); out << cpp_code; out.close();

        std::string viss_root = getExecutableDir();
        std::string cxx = findCompiler();
        fs::path cxx_dir = fs::path(cxx).parent_path();
#ifdef _WIN32
        std::string orig_path = "";
        char p_buf[32767];
        if (GetEnvironmentVariableA("PATH", p_buf, sizeof(p_buf))) orig_path = p_buf;
        std::string new_path = cxx_dir.string() + ";" + orig_path;
        SetEnvironmentVariableA("PATH", new_path.c_str());
#endif
        fs::path err_log = cache_dir / "bundle_err.log";
        std::string bundle_cmd = std::string("g++ -std=c++17 -O3 -pipe -static -static-libgcc -static-libstdc++ -s ")
                                + "-I\"" + viss_root + "\" "
                                + "-I\"" + file_dir.string() + "\" "
                                + "-I. \"" + cpp_file.string() + "\" "
                                + "-o \"" + out_exe + "\"";
#ifdef _WIN32
        bool is_gui_app = (viss_code.find("\"gui\"") != std::string::npos ||
                           viss_code.find("\"ui\"") != std::string::npos ||
                           viss_code.find("\"window\"") != std::string::npos ||
                           viss_code.find("ui.create") != std::string::npos ||
                           viss_code.find("gui.create") != std::string::npos ||
                           viss_code.find("ui.window") != std::string::npos ||
                           viss_code.find("gui.window") != std::string::npos ||
                           cpp_code.find("viss::gui") != std::string::npos);
        for (int ai = 2; ai < argc; ++ai) {
            std::string arg = argv[ai];
            if (arg == "-w" || arg == "--gui" || arg == "--windowed" || arg == "-mwindows") is_gui_app = true;
        }
        if (is_gui_app) bundle_cmd += " -mwindows";

        // Embedded Windows Icon
        fs::path icon_candidate = "";
        for (int ai = 2; ai < argc; ++ai) {
            std::string arg = argv[ai];
            if (arg == "--icon" && ai + 1 < argc) {
                icon_candidate = argv[ai + 1];
                break;
            }
        }
        if (icon_candidate.empty() || !fs::exists(icon_candidate)) {
            if (fs::exists(file_dir / "app_icon.ico")) icon_candidate = file_dir / "app_icon.ico";
            else if (fs::exists(file_dir / "icon.ico")) icon_candidate = file_dir / "icon.ico";
            else if (fs::exists(fs::current_path() / "app_icon.ico")) icon_candidate = fs::current_path() / "app_icon.ico";
        }
        if (!icon_candidate.empty() && fs::exists(icon_candidate)) {
            fs::path rc_path = cache_dir / "app_icon.rc";
            fs::path res_obj = cache_dir / "app_icon.res.o";
            std::string ico_fwd = replaceAll(icon_candidate.string(), "\\", "/");
            std::ofstream rcf(rc_path);
            rcf << "1 ICON \"" << ico_fwd << "\"\n";
            rcf.close();
            std::string wr_cmd = "windres -i \"" + rc_path.string() + "\" -o \"" + res_obj.string() + "\" -O coff";
            if (std::system(wr_cmd.c_str()) == 0 && fs::exists(res_obj)) {
                bundle_cmd += " \"" + res_obj.string() + "\"";
            }
        }

        bundle_cmd += " -lwinmm -lws2_32 -lwininet -lgdi32 -lgdiplus -lcomdlg32 -lole32 -loleaut32 -luuid -lshell32";
#endif
        bundle_cmd += " 2> \"" + err_log.string() + "\"";

        int ret = std::system(bundle_cmd.c_str());
        if (ret != 0) {
            sanitizeAndPrintCompilerErrors(err_log.string(), target_file);
            return ret;
        }

        uintmax_t bsz = fs::exists(out_exe) ? fs::file_size(out_exe) : 0;
        std::cout << "[Viss Bundler] Standalone binary created: " << out_exe << " (" << bsz << " bytes, zero dependencies) ^_^\n";
        return 0;
    }
    if (cmd == "fmt") {
        if (argc < 3) { std::cerr << "Usage: viss fmt [--check] <file.viss>\n"; return 1; }
        bool check_only = false;
        std::string target_file = argv[2];
        if (target_file == "--check") {
            if (argc < 4) { std::cerr << "Usage: viss fmt --check <file.viss>\n"; return 1; }
            check_only = true;
            target_file = argv[3];
        }
        return formatFile(target_file, check_only);
    }

    if (cmd == "-v" || cmd == "--version" || cmd == "version") {
        std::cout << "Viss Programming Language Compiler & Toolchain v" << VERSION << " \"" << CODENAME << "\" (Native C++20)\n";
        return 0;
    }
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        printHelp();
        return 0;
    }
    if (cmd == "clean") {
        if (fs::exists(".viss_cache")) {
            fs::remove_all(".viss_cache");
            std::cout << "Viss build cache cleaned.\n";
        }
        return 0;
    }

    if (viss_file.empty()) {
        if (cmd == "run") {
        if (argc < 3) { std::cerr << "Usage: viss run <file.viss>\n"; return 1; }
        viss_file = argv[2];
        run_after = true;
    } else if (cmd == "build" || cmd == "compile") {
        if (argc < 3) { std::cerr << "Usage: viss build <file.viss>\n"; return 1; }
        viss_file = argv[2];
        run_after = false;
    } else if (cmd == "transpile") {
        if (argc < 3) { std::cerr << "Usage: viss transpile <file.viss> [output.cpp]\n"; return 1; }
        viss_file = argv[2];
        std::ifstream in(viss_file);
        if (!in.is_open()) { std::cerr << "Error: File '" << viss_file << "' not found.\n"; return 1; }
        std::stringstream buffer; buffer << in.rdbuf();
        std::string cpp_code = transpile(buffer.str(), fs::path(viss_file).filename().string(), fs::absolute(fs::path(viss_file)).parent_path().string());
        std::string out_name = (argc > 3) ? argv[3] : fs::path(viss_file).stem().string() + ".cpp";
        std::ofstream out(out_name); out << cpp_code;
        std::cout << "Transpiled to " << out_name << "\n";
        return 0;
    } else if (cmd == "check") {
        if (argc < 3) { std::cerr << "Usage: viss check <file.viss>\n"; return 1; }
        viss_file = argv[2];
        std::ifstream in(viss_file);
        if (!in.is_open()) { std::cerr << "Error: File '" << viss_file << "' not found.\n"; return 1; }
        std::stringstream buffer; buffer << in.rdbuf();
        transpile(buffer.str(), fs::path(viss_file).filename().string(), fs::absolute(fs::path(viss_file)).parent_path().string());
        std::cout << "[Viss Checker] File '" << viss_file << "' syntax verified cleanly! ^_^\n";
        return 0;
    } else {
        viss_file = cmd;
        run_after = true;
    }
    }

    if (!fs::exists(viss_file)) {
        std::cerr << "Error: File '" << viss_file << "' not found.\n";
        return 1;
    }

    fs::path p(viss_file);
    std::string stem = p.stem().string();
    fs::path file_dir = fs::absolute(p).parent_path();
    fs::path cache_dir = file_dir / ".viss_cache" / stem;
    fs::create_directories(cache_dir);

    fs::path cpp_file = cache_dir / (stem + ".cpp");
#ifdef _WIN32
    fs::path exe_file = file_dir / (stem + ".exe");
#else
    fs::path exe_file = file_dir / stem;
#endif

    for (int ai = 2; ai < argc; ++ai) {
        std::string a = argv[ai];
        if (a == "-o" && ai + 1 < argc) {
            exe_file = argv[++ai];
        }
    }

    // Fast incremental execution: if binary exists and is newer than source, run immediately!
    bool needs_rebuild = true;
    if (run_after && fs::exists(exe_file) && fs::exists(viss_file)) {
        try {
            auto src_time = fs::last_write_time(viss_file);
            auto exe_time = fs::last_write_time(exe_file);
            if (exe_time >= src_time) {
                needs_rebuild = false;
            }
        } catch (...) {
            needs_rebuild = true;
        }
    }

    if (!needs_rebuild) {
        std::cout << "[Viss Runner] Binary up-to-date. Running " << exe_file.filename().string() << " (0ms compile overhead)...\n";
        std::string run_cmd = "\"" + exe_file.string() + "\"";
        for (int ai = (cmd == "run" ? 3 : 2); ai < argc; ++ai) {
            run_cmd += " \"" + std::string(argv[ai]) + "\"";
        }
        return std::system(run_cmd.c_str());
    }

    // Read source
    std::ifstream in(viss_file);
    std::stringstream buffer; buffer << in.rdbuf();
    std::string viss_code = buffer.str();

    std::cout << "[Viss Compiler v" << VERSION << " \"" << CODENAME << "\"] Transpiling " << viss_file << "...\n";
    std::string cpp_code = transpile(viss_code, p.filename().string(), file_dir.string());
    if (cpp_code.empty()) {
        return 1;
    }

    std::ofstream out(cpp_file);
    out << cpp_code;
    out.close();

    // Compiler path
    std::string cxx = findCompiler();
    std::string viss_root = getExecutableDir();

    bool has_compiler = fs::exists(cxx);
    if (!has_compiler) {
        has_compiler = (std::system("g++ --version > nul 2>&1") == 0);
    }

    if (!has_compiler) {
        std::cout << "[Viss Runner] C++ Compiler (g++) not found on system.\n";
        std::cout << "[Viss Runner] Executing via native VissVM engine (0ms compile overhead, no C++ needed!)...\n";
        std::vector<std::string> pass_args;
        for (int ai = (cmd == "run" ? 3 : 2); ai < argc; ++ai) pass_args.push_back(argv[ai]);
        return viss::vm::VissVM::executeFile(viss_file, pass_args);
    }

    // Ensure PATH has compiler directory for subprocesses
    fs::path cxx_dir = fs::path(cxx).parent_path();
#ifdef _WIN32
    std::string orig_path = "";
    char p_buf[32767];
    if (GetEnvironmentVariableA("PATH", p_buf, sizeof(p_buf))) orig_path = p_buf;
    std::string new_path = cxx_dir.string() + ";" + orig_path;
    SetEnvironmentVariableA("PATH", new_path.c_str());
#endif

    fs::path err_log = cache_dir / "build_err.log";
    std::string compile_cmd = std::string("g++ -std=c++17 -O2 -pipe ")
                            + "-I\"" + viss_root + "\" "
                            + "-I\"" + file_dir.string() + "\" "
                            + "-I. \"" + cpp_file.string() + "\" "
                            + "-o \"" + exe_file.string() + "\"";
#ifdef _WIN32
    bool is_gui_app = (viss_code.find("window.create") != std::string::npos ||
                       viss_code.find("ui.create") != std::string::npos ||
                       viss_code.find("gui.create") != std::string::npos ||
                       viss_code.find("ui.window") != std::string::npos ||
                       viss_code.find("gui.window") != std::string::npos);
    for (int ai = 2; ai < argc; ++ai) {
        std::string arg = argv[ai];
        if (arg == "-w" || arg == "--gui" || arg == "--windowed" || arg == "-mwindows") is_gui_app = true;
    }
    if (is_gui_app) {
        compile_cmd += " -mwindows";
    }

    // Embedded Windows Icon
    fs::path icon_candidate = "";
    for (int ai = 2; ai < argc; ++ai) {
        std::string arg = argv[ai];
        if (arg == "--icon" && ai + 1 < argc) {
            icon_candidate = argv[ai + 1];
            break;
        }
    }
    if (icon_candidate.empty() || !fs::exists(icon_candidate)) {
        if (fs::exists(file_dir / "app_icon.ico")) icon_candidate = file_dir / "app_icon.ico";
        else if (fs::exists(file_dir / "icon.ico")) icon_candidate = file_dir / "icon.ico";
        else if (fs::exists(fs::current_path() / "app_icon.ico")) icon_candidate = fs::current_path() / "app_icon.ico";
    }
    if (!icon_candidate.empty() && fs::exists(icon_candidate)) {
        fs::path rc_path = cache_dir / "app_icon.rc";
        fs::path res_obj = cache_dir / "app_icon.res.o";
        std::string ico_fwd = replaceAll(icon_candidate.string(), "\\", "/");
        std::ofstream rcf(rc_path);
        rcf << "1 ICON \"" << ico_fwd << "\"\n";
        rcf.close();
        std::string wr_cmd = "windres -i \"" + rc_path.string() + "\" -o \"" + res_obj.string() + "\" -O coff";
        if (std::system(wr_cmd.c_str()) == 0 && fs::exists(res_obj)) {
            compile_cmd += " \"" + res_obj.string() + "\"";
        }
    }

    compile_cmd += " -lwinmm -lws2_32 -lwininet -lgdi32 -lgdiplus -lcomdlg32 -lole32 -lshell32";
#endif
    compile_cmd += " 2> \"" + err_log.string() + "\"";

    int ret = std::system(compile_cmd.c_str());
    if (ret != 0) {
        sanitizeAndPrintCompilerErrors(err_log.string(), viss_file);
        return ret;
    }

    std::cout << "[Viss Compiler v" << VERSION << " \"" << CODENAME << "\"] Build successful: " << exe_file.filename().string() << "\n";

    if (run_after) {
        std::cout << "[Viss Compiler v" << VERSION << " \"" << CODENAME << "\"] Running " << exe_file.filename().string() << "...\n";
        std::string run_cmd = "\"" + exe_file.string() + "\"";
        if (cmd != "-e" && cmd != "eval") {
            for (int ai = (cmd == "run" ? 3 : 2); ai < argc; ++ai) {
                run_cmd += " \"" + std::string(argv[ai]) + "\"";
            }
        }
#ifdef _WIN32
        std::string final_cmd = "cmd.exe /s /c \"" + run_cmd + "\"";
        return std::system(final_cmd.c_str());
#else
        return std::system(run_cmd.c_str());
#endif
    }

    return 0;
}

void runRepl() {
    std::cout << "====================================================\n";
    std::cout << "  Viss v" << VERSION << " \"" << CODENAME << "\" Interactive REPL ^_^\n";
    std::cout << "  Type 'help' for commands, 'clear' to reset, 'exit' to quit.\n";
    std::cout << "====================================================\n\n";

    fs::create_directories(".viss_cache/repl");
    fs::path repl_file = ".viss_cache/repl/repl_session.viss";

    std::vector<std::string> session_statements;
    std::string multiline_acc = "";
    int brace_depth = 0;

    while (true) {
        std::cout << (brace_depth > 0 ? "... " : ">>> ") << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) break;
        std::string trimmed = trim(line);
        if (trimmed.empty() && brace_depth == 0) continue;

        if (brace_depth == 0) {
            if (trimmed == "exit" || trimmed == "quit" || trimmed == "!exit") {
                std::cout << "Bye! :3\n";
                break;
            }
            if (trimmed == "clear" || trimmed == "!clear") {
                session_statements.clear();
                std::cout << "REPL session cleared.\n";
                continue;
            }
            if (trimmed == "help" || trimmed == "!help") {
                std::cout << "REPL Commands:\n"
                          << "  help       Show this help\n"
                          << "  clear      Clear all session variables\n"
                          << "  exit       Exit REPL\n"
                          << "Examples:\n"
                          << "  @x = 42\n"
                          << "  @x * 2\n"
                          << "  &buf create | bytes, 16\n"
                          << "  io.println(\"Hello Viss!\")\n\n";
                continue;
            }
        }

        for (char c : line) {
            if (c == '{') brace_depth++;
            else if (c == '}') { if (brace_depth > 0) brace_depth--; }
        }

        multiline_acc += (multiline_acc.empty() ? "" : "\n") + line;
        if (brace_depth > 0) {
            continue; // Wait for multiline block to finish
        }

        std::string stmt = trim(multiline_acc);
        multiline_acc.clear();
        if (stmt.empty()) continue;

        // Determine if line is an expression or statement
        bool is_expr = true;
        if (startsWith(stmt, "@") && stmt.find('=') != std::string::npos) is_expr = false;
        else if (startsWith(stmt, "&")) is_expr = false;
        else if (startsWith(stmt, "!") || startsWith(stmt, "?") || startsWith(stmt, "$")) is_expr = false;
        else if (startsWith(stmt, "io.") || startsWith(stmt, "rt.") || startsWith(stmt, "sys.")) is_expr = false;

        std::string line_to_run = stmt;
        if (is_expr) {
            line_to_run = "io.println(" + stmt + ");";
        }

        // Generate temporary viss file
        std::ofstream out(repl_file);
        out << "$import lib \"iostream\" as io\n";
        out << "$import lib \"retrotech\" as rt\n\n";
        out << "!main {\n";
        for (const auto& s : session_statements) {
            out << "    " << s << "\n";
        }
        out << "    " << line_to_run << "\n";
        out << "}\n";
        out.close();

        // Run via transpile and system compile
        fs::path cpp_out = ".viss_cache/repl/repl_session.cpp";
        fs::path exe_out = ".viss_cache/repl/repl_session.exe";
        fs::path err_log = ".viss_cache/repl/repl_err.log";
        std::ifstream in(repl_file);
        std::stringstream buf; buf << in.rdbuf();
        std::string code = transpile(buf.str(), "repl_session.viss", ".viss_cache/repl");
        if (code.empty()) continue;

        std::ofstream out_cpp(cpp_out); out_cpp << code; out_cpp.close();

        std::string viss_root = getExecutableDir();
        std::string cxx = findCompiler();
        fs::path cxx_dir = fs::path(cxx).parent_path();
#ifdef _WIN32
        std::string orig_path = "";
        char p_buf[32767];
        if (GetEnvironmentVariableA("PATH", p_buf, sizeof(p_buf))) orig_path = p_buf;
        std::string new_path = cxx_dir.string() + ";" + orig_path;
        SetEnvironmentVariableA("PATH", new_path.c_str());
#endif
        std::string cmd = "g++ -std=c++17 -O1 -pipe -I\"" + viss_root + "\" -I. \"" + cpp_out.string() + "\" -o \"" + exe_out.string() + "\" -lwinmm -lws2_32 -lwininet -lgdi32 -lgdiplus -lcomdlg32 -lole32 -lshell32 2> \"" + err_log.string() + "\"";
        int ret = std::system(cmd.c_str());
        if (ret == 0) {
            std::system(exe_out.string().c_str());
            if (!is_expr) {
                session_statements.push_back(stmt);
            }
        } else {
            sanitizeAndPrintCompilerErrors(err_log.string(), repl_file.string());
        }
    }
}
