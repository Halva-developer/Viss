#pragma once

// =============================================================================
// VSC - Viss Script Core (formerly SSI)
// Lightweight Fast Scripting Engine & Toolchain Front-End for Viss
// =============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include "viss_vm.hpp"

namespace fs = std::filesystem;

namespace vsc {

const std::string VSC_VERSION = "0.3.0";
const std::string VSC_CODENAME = "Viss Script Core";
const std::string SSI_VERSION = VSC_VERSION;
const std::string SSI_CODENAME = VSC_CODENAME;

inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

inline bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

inline bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

// Split expression by delimiter outside quotes and parens
inline std::vector<std::string> splitTopLevel(const std::string& s, const std::string& delim) {
    std::vector<std::string> parts;
    std::string cur;
    int paren = 0, brace = 0, bracket = 0;
    bool in_str = false;
    char quote = 0;
    size_t dlen = delim.size();

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (in_str) {
            cur += c;
            if (c == quote && (i == 0 || s[i - 1] != '\\')) in_str = false;
        } else {
            if (c == '"' || c == '\'') {
                in_str = true; quote = c; cur += c;
            } else if (c == '(') { paren++; cur += c; }
            else if (c == ')') { if (paren > 0) paren--; cur += c; }
            else if (c == '{') { brace++; cur += c; }
            else if (c == '}') { if (brace > 0) brace--; cur += c; }
            else if (c == '[') { bracket++; cur += c; }
            else if (c == ']') { if (bracket > 0) bracket--; cur += c; }
            else if (paren == 0 && brace == 0 && bracket == 0 &&
                     i + dlen <= s.size() && s.substr(i, dlen) == delim) {
                parts.push_back(trim(cur));
                cur.clear();
                i += dlen - 1;
            } else {
                cur += c;
            }
        }
    }
    if (!cur.empty()) parts.push_back(trim(cur));
    return parts;
}

// Unroll pipeline expression: x |> f() |> g() -> g(f(x))
inline std::string unrollPipeline(const std::string& expr) {
    auto steps = splitTopLevel(expr, "|>");
    if (steps.size() <= 1) return expr;

    std::string cur = trim(steps[0]);
    for (size_t si = 1; si < steps.size(); ++si) {
        std::string step = trim(steps[si]);
        size_t p_open = step.find('(');
        if (p_open != std::string::npos && step.back() == ')') {
            std::string fname = trim(step.substr(0, p_open));
            std::string inner_args = trim(step.substr(p_open + 1, step.size() - p_open - 2));
            if (inner_args.empty()) {
                cur = fname + "(" + cur + ")";
            } else {
                cur = fname + "(" + cur + ", " + inner_args + ")";
            }
        } else {
            cur = step + "(" + cur + ")";
        }
    }
    return cur;
}

// Preprocessor from VSC to standard Viss AST source
inline std::string transpileVSCtoViss(const std::string& source, bool is_repl = false) {
    std::stringstream in_ss(source);
    std::string line;
    std::vector<std::string> raw_lines;
    bool is_first = true;

    while (std::getline(in_ss, line)) {
        // Strip shebang line
        if (is_first && startsWith(line, "#!")) {
            is_first = false;
            continue;
        }
        is_first = false;

        // Replace '#,' with '; '
        size_t sharp_comma = 0;
        while ((sharp_comma = line.find("#,", sharp_comma)) != std::string::npos) {
            line.replace(sharp_comma, 2, "; ");
            sharp_comma += 2;
        }

        // Strip single line comments outside quotes
        std::string clean = "";
        bool in_str = false;
        char q = 0;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (in_str) {
                clean += c;
                if (c == q && (i == 0 || line[i - 1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') {
                    in_str = true; q = c; clean += c;
                } else if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
                    break;
                } else if (c == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t' || line[i - 1] == ';')) {
                    break;
                } else {
                    clean += c;
                }
            }
        }
        clean = trim(clean);
        if (!clean.empty()) raw_lines.push_back(clean);
    }

    std::vector<std::string> funcs;

    // Extract single-line function definitions from raw_lines before splitting by ';'
    // Matches e.g. a(i)->return(i+10);END or $a(i)->return(i+10);END or a(i)-return(i+10);END or a(i): return(i+10); END
    std::regex single_fn_re(R"((?:\$|!func\s+|func\s+|def\s+)?([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*(?:->|:|-)\s*([\s\S]*?)\bEND(?:;|,)?(?:\s*|$))");

    // Split raw lines by statement delimiters outside quotes and braces
    std::vector<std::string> statements;
    for (auto& rline : raw_lines) {
        std::smatch m_sfn;
        while (std::regex_search(rline, m_sfn, single_fn_re)) {
            std::string fname = m_sfn[1].str();
            if (fname == "if" || fname == "while" || fname == "for" || fname == "elif" || fname == "switch" || fname == "println" || fname == "print") break;
            std::string params = m_sfn[2].str();
            std::string body = trim(m_sfn[3].str());
            if (!body.empty() && !endsWith(body, ";")) body += ";";
            if (startsWith(body, "return(") || startsWith(body, "return ")) {
                body = "!" + body;
            }
            if (body.find("|>") != std::string::npos) body = unrollPipeline(body);
            funcs.push_back("!func " + fname + "(" + params + ") {\n" + body + "\n}");
            rline = rline.substr(0, m_sfn.position()) + " ; " + rline.substr(m_sfn.position() + m_sfn.length());
        }

        // Handle chained 'END,' e.g. $jopa(i)->return(i+10);END,$joja(i)->return(i+2);END
        size_t end_comma = 0;
        while ((end_comma = rline.find("END,", end_comma)) != std::string::npos) {
            rline.replace(end_comma, 4, "END; ");
            end_comma += 5;
        }

        std::string cur = "";
        bool in_str = false;
        char q = 0;
        int paren = 0, bracket = 0, brace = 0;

        for (size_t i = 0; i < rline.size(); ++i) {
            char c = rline[i];
            if (in_str) {
                cur += c;
                if (c == q && (i == 0 || rline[i - 1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') {
                    in_str = true; q = c; cur += c;
                } else if (c == '(') { paren++; cur += c; }
                else if (c == ')') { if (paren > 0) paren--; cur += c; }
                else if (c == '[') { bracket++; cur += c; }
                else if (c == ']') { if (bracket > 0) bracket--; cur += c; }
                else if (c == '{') { brace++; cur += c; }
                else if (c == '}') { if (brace > 0) brace--; cur += c; }
                else if (c == ';' && paren == 0 && bracket == 0 && brace == 0) {
                    std::string t = trim(cur);
                    if (!t.empty()) statements.push_back(t + ";");
                    cur.clear();
                } else {
                    cur += c;
                }
            }
        }
        std::string rem = trim(cur);
        if (!rem.empty()) statements.push_back(rem);
    }

    // Check if script explicitly has $RUN or !main
    bool has_main_block = false;
    for (const auto& s : statements) {
        std::string ts = trim(s);
        if (startsWith(ts, "$RUN") || startsWith(ts, "RUN") ||
            startsWith(ts, "!main") || startsWith(ts, "main()")) {
            has_main_block = true;
            break;
        }
    }

    std::vector<std::string> imports;
    std::vector<std::string> main_body;

    bool in_func = false;
    bool in_run = false;

    for (size_t si = 0; si < statements.size(); ++si) {
        std::string stmt = trim(statements[si]);
        if (stmt.empty()) continue;

        // Strip trailing semicolon for analysis
        bool has_semi = endsWith(stmt, ";");
        std::string clean = has_semi ? trim(stmt.substr(0, stmt.size() - 1)) : stmt;

        // 1. Imports
        if (startsWith(clean, "%import") || startsWith(clean, "$import")) {
            std::smatch m_imp;
            if (std::regex_match(clean, m_imp, std::regex(R"(^(?:\%import|\$import)(?:\s+lib)?\s+([^\s]+)(?:\s+as\s+([a-zA-Z0-9_]+))?$)"))) {
                std::string lib_name = m_imp[1].str();
                while (!lib_name.empty() && (lib_name.front() == '"' || lib_name.front() == '\'')) lib_name = lib_name.substr(1);
                while (!lib_name.empty() && (lib_name.back() == '"' || lib_name.back() == '\'')) lib_name.pop_back();
                std::string alias = (m_imp.size() > 2 && m_imp[2].matched) ? m_imp[2].str() : lib_name;

                std::string canon = lib_name;
                if (lib_name == "system") canon = "sys";
                else if (lib_name == "iostream") canon = "iostream";

                imports.push_back("$import lib \"" + canon + "\" as " + alias + ";");
                continue;
            }
            imports.push_back(clean + ";");
            continue;
        }

        // 2. $RUN block header: RUN:, RUN->, RUN-, $RUN:, $RUN->, $RUN-
        bool is_run_header = false;
        size_t run_del_len = 0;
        for (const std::string& rprefix : {"$RUN->", "RUN->", "$RUN:", "RUN:", "$RUN-", "RUN-"}) {
            if (startsWith(clean, rprefix)) {
                is_run_header = true;
                run_del_len = rprefix.size();
                break;
            }
        }
        if (!is_run_header && (clean == "$RUN" || clean == "RUN" || startsWith(clean, "!main") || clean == "main()")) {
            is_run_header = true;
            run_del_len = clean.size();
        }

        if (is_run_header) {
            in_run = true;
            main_body.push_back("!main {");
            if (run_del_len > 0 && run_del_len < clean.size()) {
                std::string rest = trim(clean.substr(run_del_len));
                if (!rest.empty()) {
                    statements.insert(statements.begin() + si + 1, rest + ";");
                }
            }
            continue;
        }

        // 3. Block close (END or })
        std::string clean_no_semi = clean;
        while (!clean_no_semi.empty() && (clean_no_semi.back() == ';' || clean_no_semi.back() == ' ')) clean_no_semi.pop_back();
        if (clean_no_semi == "END" || clean == "}") {
            if (in_run) {
                main_body.push_back("}");
                in_run = false;
            } else if (in_func) {
                funcs.push_back("}");
                in_func = false;
            } else {
                main_body.push_back("}");
            }
            continue;
        }

        // 4. Function definitions: $func(args):, func(args):, a(i):, a(i)->, !func name(...)
        std::smatch m_fhead;
        if (std::regex_match(clean, m_fhead, std::regex(R"(^(?:\$|!func\s+|func\s+|def\s+)?([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*(?::|->|-|\{)?$)"))) {
            std::string fname = m_fhead[1].str();
            if (fname != "if" && fname != "while" && fname != "for" && fname != "elif" && fname != "switch" && fname != "return" && fname != "println" && fname != "print") {
                std::string params = m_fhead[2].str();
                funcs.push_back("!func " + fname + "(" + params + ") {");
                in_func = true;
                continue;
            }
        }

        // 5. Arrow conditionals: ?if (...) -> action; ?else -> ...
        // Replace ?else -> ?if with ?elif
        clean = replaceAll(clean, "?else -> ?if", "?elif");
        clean = replaceAll(clean, "?else ->?if", "?elif");

        if (clean.find("->") != std::string::npos &&
            (clean.find("?if") != std::string::npos || clean.find("?elif") != std::string::npos || clean.find("?else") != std::string::npos)) {
            // Parse arrow conditional chain
            std::string transformed = "";
            size_t pos = 0;
            while (pos < clean.size()) {
                if (clean.compare(pos, 3, "?if") == 0 || clean.compare(pos, 5, "?elif") == 0) {
                    bool is_elif = (clean.compare(pos, 5, "?elif") == 0);
                    std::string kw = is_elif ? "?elif" : "?if";
                    pos += kw.size();
                    size_t arrow_p = clean.find("->", pos);
                    if (arrow_p != std::string::npos) {
                        std::string cond = trim(clean.substr(pos, arrow_p - pos));
                        pos = arrow_p + 2;
                        // Find end of action
                        size_t next_branch = clean.find("?else", pos);
                        if (next_branch == std::string::npos) next_branch = clean.find("?elif", pos);
                        std::string action = "";
                        if (next_branch != std::string::npos) {
                            action = trim(clean.substr(pos, next_branch - pos));
                            pos = next_branch;
                        } else {
                            action = trim(clean.substr(pos));
                            pos = clean.size();
                        }
                        if (!action.empty() && !endsWith(action, ";")) action += ";";
                        if (!transformed.empty()) transformed += " ";
                        transformed += kw + " " + cond + " { " + action + " }";
                    } else {
                        transformed += clean.substr(pos);
                        break;
                    }
                } else if (clean.compare(pos, 5, "?else") == 0) {
                    pos += 5;
                    size_t arrow_p = clean.find("->", pos);
                    if (arrow_p != std::string::npos) {
                        pos = arrow_p + 2;
                        std::string action = trim(clean.substr(pos));
                        pos = clean.size();
                        if (action == "break;" || action == "break") action = ";";
                        if (!action.empty() && !endsWith(action, ";")) action += ";";
                        if (!transformed.empty()) transformed += " ";
                        transformed += "?else { " + action + " }";
                    } else {
                        transformed += "?else { }";
                        break;
                    }
                } else {
                    transformed += clean[pos++];
                }
            }
            clean = transformed;
        }

        // 6. Pipeline unrolling: @var expr |> f() or expr |> f()
        if (clean.find("|>") != std::string::npos && !startsWith(clean, "println(") && !startsWith(clean, "print(") && !startsWith(clean, "io.println(") && !startsWith(clean, "iostream.println(")) {
            // Check assignment: e.g. @boba = expr |> ... or @boba k() |> ...
            if (clean.front() == '@' || clean.front() == '&') {
                size_t eq_pos = clean.find('=');
                if (eq_pos != std::string::npos && (eq_pos == 0 || clean[eq_pos - 1] != '!') && (eq_pos + 1 >= clean.size() || clean[eq_pos + 1] != '=')) {
                    std::string vname = trim(clean.substr(0, eq_pos));
                    std::string rest = trim(clean.substr(eq_pos + 1));
                    clean = vname + " = " + unrollPipeline(rest);
                } else {
                    size_t sp = clean.find_first_of(" \t");
                    if (sp != std::string::npos) {
                        std::string vname = clean.substr(0, sp);
                        std::string rest = trim(clean.substr(sp + 1));
                        clean = vname + " = " + unrollPipeline(rest);
                    } else {
                        clean = unrollPipeline(clean);
                    }
                }
            } else {
                clean = unrollPipeline(clean);
            }
        } else {
            // Check assignment without '=': e.g. @var 123
            if (clean.front() == '@' || clean.front() == '&') {
                size_t sp = clean.find_first_of(" \t");
                if (sp != std::string::npos) {
                    std::string vname = clean.substr(0, sp);
                    std::string rest = trim(clean.substr(sp + 1));
                    if (!rest.empty() && rest.front() != '=' &&
                        !startsWith(rest, "+=") && !startsWith(rest, "-=") &&
                        !startsWith(rest, "*=") && !startsWith(rest, "/=") &&
                        !startsWith(rest, "create |") && !startsWith(rest, "write ")) {
                        clean = vname + " = " + rest;
                    }
                }
            }
        }

        // 7. Normalize return
        if (startsWith(clean, "return(") || startsWith(clean, "return ")) {
            clean = "!" + clean;
        }

        if (!clean.empty() && !endsWith(clean, ";") && !endsWith(clean, "}")) {
            clean += ";";
        }

        if (in_func) {
            funcs.push_back(clean);
        } else {
            main_body.push_back(clean);
        }
    }

    // Collect defined function names and signatures for case-insensitive normalization and arity overloads
    struct FnSig {
        std::string name;
        std::vector<std::string> params;
    };
    std::vector<FnSig> defined_sigs;
    std::map<std::string, std::string> fn_cases;

    for (const auto& f : funcs) {
        if (startsWith(f, "!func ")) {
            size_t p_o = f.find('(', 6);
            size_t p_c = f.find(')', p_o != std::string::npos ? p_o : 6);
            if (p_o != std::string::npos && p_c != std::string::npos && p_c > p_o) {
                std::string fname = trim(f.substr(6, p_o - 6));
                std::string param_str = trim(f.substr(p_o + 1, p_c - p_o - 1));
                std::vector<std::string> params;
                if (!param_str.empty()) {
                    auto tokens = splitTopLevel(param_str, ",");
                    for (auto& tok : tokens) {
                        std::string p = trim(tok);
                        if (!p.empty()) params.push_back(p);
                    }
                }
                defined_sigs.push_back({fname, params});
                std::string lower_fn = fname;
                for (char& c : lower_fn) c = (char)std::tolower((unsigned char)c);
                fn_cases[lower_fn] = fname;
            }
        }
    }

    auto isInsideQuotes = [](const std::string& line, size_t target_pos) -> bool {
        bool in_s = false;
        char qc = 0;
        for (size_t i = 0; i < target_pos && i < line.size(); ++i) {
            char c = line[i];
            if (in_s) {
                if (c == qc && (i == 0 || line[i - 1] != '\\')) in_s = false;
            } else {
                if (c == '"' || c == '\'') { in_s = true; qc = c; }
            }
        }
        return in_s;
    };

    // Normalize function call cases in funcs and main_body
    auto normalizeFnCases = [&](std::string& line) {
        for (const auto& kv : fn_cases) {
            std::string pat = kv.first + "(";
            std::string rep = kv.second + "(";
            if (kv.first != kv.second) {
                size_t pos = 0;
                while ((pos = line.find(pat, pos)) != std::string::npos) {
                    if (isInsideQuotes(line, pos)) {
                        pos += pat.size();
                        continue;
                    }
                    bool l_ok = (pos == 0 || (!std::isalnum((unsigned char)line[pos - 1]) && line[pos - 1] != '_' && line[pos - 1] != '@'));
                    if (l_ok) {
                        line.replace(pos, pat.size(), rep);
                        pos += rep.size();
                    } else {
                        pos += pat.size();
                    }
                }
            }
        }
    };
    for (auto& f : funcs) normalizeFnCases(f);
    for (auto& m : main_body) normalizeFnCases(m);

    // Pad function calls where caller provided fewer arguments than declared parameters
    auto padCallArgs = [&](std::string& line) {
        for (const auto& sig : defined_sigs) {
            size_t n = sig.params.size();
            if (n == 0) continue;
            std::string pat = sig.name + "(";
            size_t pos = 0;
            while ((pos = line.find(pat, pos)) != std::string::npos) {
                if (isInsideQuotes(line, pos)) {
                    pos += pat.size();
                    continue;
                }
                bool l_ok = (pos == 0 || (!std::isalnum((unsigned char)line[pos - 1]) && line[pos - 1] != '_' && line[pos - 1] != '@'));
                if (!l_ok) {
                    pos += pat.size();
                    continue;
                }
                size_t p_start = pos + pat.size();
                int p_depth = 1;
                size_t p_end = p_start;
                bool in_s = false;
                char qc = 0;
                while (p_end < line.size() && p_depth > 0) {
                    char c = line[p_end];
                    if (in_s) {
                        if (c == qc && line[p_end - 1] != '\\') in_s = false;
                    } else {
                        if (c == '"' || c == '\'') { in_s = true; qc = c; }
                        else if (c == '(') p_depth++;
                        else if (c == ')') p_depth--;
                    }
                    if (p_depth > 0) p_end++;
                }
                if (p_depth == 0 && p_end < line.size()) {
                    std::string inner = trim(line.substr(p_start, p_end - p_start));
                    size_t arg_count = 0;
                    if (!inner.empty()) {
                        auto args = splitTopLevel(inner, ",");
                        arg_count = args.size();
                    }
                    if (arg_count < n) {
                        std::string pad = inner;
                        for (size_t k = arg_count; k < n; ++k) {
                            if (!pad.empty()) pad += ", ";
                            pad += "0";
                        }
                        line.replace(p_start, p_end - p_start, pad);
                        pos = p_start + pad.size() + 1;
                        continue;
                    }
                }
                pos += pat.size();
            }
        }
    };
    for (auto& f : funcs) padCallArgs(f);
    for (auto& m : main_body) padCallArgs(m);

    // Assemble final Viss source code: funcs BEFORE main_body
    std::string out = "";
    for (const auto& imp : imports) out += imp + "\n";
    if (!imports.empty()) out += "\n";

    for (const auto& f : funcs) out += f + "\n";
    if (!funcs.empty()) out += "\n";

    if (is_repl && !has_main_block) {
        for (const auto& m : main_body) out += m + "\n";
    } else if (!has_main_block) {
        out += "!main {\n";
        for (const auto& m : main_body) out += "    " + m + "\n";
        out += "}\n";
    } else {
        for (const auto& m : main_body) out += m + "\n";
    }

    return out;
}

// Direct Execution via VissVM Engine
inline std::string transpileSSItoViss(const std::string& source) {
    return transpileVSCtoViss(source);
}

inline int execute(const std::string& vsc_code, const std::vector<std::string>& args = {}) {
    std::string viss_code = transpileVSCtoViss(vsc_code);
    return viss::vm::VissVM::executeSource(viss_code, args);
}

inline int executeFile(const std::string& filepath, const std::vector<std::string>& args = {}) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        std::cerr << "[VSC Error] Cannot open file: " << filepath << "\n";
        return 1;
    }
    std::stringstream ss; ss << in.rdbuf();
    return execute(ss.str(), args);
}

// Expression evaluation
inline int evaluateExpression(const std::string& expr_str) {
    viss::vm::VMContext ctx;
    std::string viss_code = transpileVSCtoViss(expr_str, /*is_repl=*/true);
    bool is_expr = false;
    std::string err_m;
    auto res = viss::vm::VissVM::executeReplSnippet(viss_code, ctx, is_expr, err_m);
    if (!err_m.empty()) {
        std::cerr << "[VSC Error] " << err_m << "\n";
        return 1;
    }
    if (res.type != viss::vm::VMType::Null && is_expr) {
        std::cout << res.to_string() << "\n";
    }
    return 0;
}

// Interactive VSC REPL
inline void runRepl() {
    std::cout << "\033[1;36m=================================================================\033[0m\n";
    std::cout << "\033[1;36m  VSC v" << VSC_VERSION << " \"" << VSC_CODENAME << "\"\033[0m\n";
    std::cout << "  Fast lightweight scripting shell & runner (0ms compile overhead)\n";
    std::cout << "  Type code or expressions. Type \033[33mexit\033[0m to quit, \033[33mhelp\033[0m for commands.\n";
    std::cout << "\033[1;36m=================================================================\033[0m\n\n";

    viss::vm::VMContext repl_ctx;
    std::string acc = "";

    while (true) {
        if (acc.empty()) {
            std::cout << "\033[1;35mvsc> \033[0m" << std::flush;
        } else {
            std::cout << "\033[33m... \033[0m" << std::flush;
        }

        std::string line;
        if (!std::getline(std::cin, line)) break;
        std::string trimmed = trim(line);

        if (acc.empty()) {
            if (trimmed == "exit" || trimmed == "quit" || trimmed == ":q") {
                std::cout << "Goodbye :3\n";
                break;
            }
            if (trimmed == "help" || trimmed == "?") {
                std::cout << "\n--- VSC Interactive Commands ---\n"
                          << "  exit, quit     Exit VSC shell\n"
                          << "  clear          Clear session state\n"
                          << "  vars           Show active variables\n"
                          << "  funcs          Show defined functions\n"
                          << "\n--- Examples ---\n"
                          << "  @os = sys.os(); println(\"OS: \" + @os)\n"
                          << "  5 |> math.sqrt => 2.236...\n"
                          << "  vsc -c \"println('hello')\"\n\n";
                continue;
            }
            if (trimmed == "clear") {
                repl_ctx.scopes.clear();
                repl_ctx.scopes.emplace_back();
                repl_ctx.functions.clear();
                std::cout << "[VSC] Session cleared.\n";
                continue;
            }
        } else if (trimmed.empty() || trimmed == "break" || trimmed == "breal" || trimmed == "cancel") {
            // Cancel multiline continuation: clear buffer
            acc.clear();
            continue;
        }

        acc += (acc.empty() ? "" : "\n") + line;
        std::string check = trim(acc);
        if (check.empty()) { acc.clear(); continue; }

        // Check if block continuation needed
        bool needs_continuation = false;
        if (check.back() == ':' || endsWith(check, "->") || endsWith(check, "-") || check.back() == '{') {
            needs_continuation = true;
        } else if (startsWith(check, "$RUN") || startsWith(check, "RUN:") || startsWith(check, "RUN") || startsWith(check, "$") || startsWith(check, "!func") || startsWith(check, "def ")) {
            if (check.find("END") == std::string::npos && check.find('}') == std::string::npos) {
                needs_continuation = true;
            }
        }
        std::string check_trimmed = trimmed;
        while (!check_trimmed.empty() && (check_trimmed.back() == ';' || check_trimmed.back() == ' ')) check_trimmed.pop_back();
        if (needs_continuation && check_trimmed != "END" && trimmed != "}") {
            continue;
        }

        std::string code = acc;
        acc.clear();

        // Translate and execute
        std::string viss_code = transpileVSCtoViss(code, /*is_repl=*/true);
        bool is_expr = false;
        std::string err_m;
        auto res = viss::vm::VissVM::executeReplSnippet(viss_code, repl_ctx, is_expr, err_m);

        if (!err_m.empty()) {
            std::cerr << "\033[31m[VSC Error]\033[0m " << err_m << "\n";
        } else if (is_expr && res.type != viss::vm::VMType::Null) {
            std::cout << "\033[1;36m=> " << res.to_repr() << "\033[0m\n";
        }
    }
}

} // namespace vsc

namespace ssi = vsc;

