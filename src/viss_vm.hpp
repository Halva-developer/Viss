#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <sstream>
#include <cmath>
#include <chrono>
#include <thread>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <cctype>
#include <fstream>
#include "../libs/std/audio.hpp"
#include "../libs/std/media.hpp"
#include "../libs/std/gui.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace viss {
namespace vm {

enum class VMType {
    Null,
    Int,
    Dec,
    Str,
    Bool,
    List,
    Bytes,
    Map
};

struct VMVal {
    VMType type = VMType::Null;
    int64_t i_val = 0;
    double d_val = 0.0;
    std::string s_val = "";
    bool b_val = false;
    std::vector<VMVal> list_val;
    std::vector<uint8_t> bytes_val;
    std::map<std::string, VMVal> map_val;

    VMVal() : type(VMType::Null) {}
    VMVal(int64_t v) : type(VMType::Int), i_val(v), d_val((double)v) {}
    VMVal(int v) : type(VMType::Int), i_val(v), d_val((double)v) {}
    VMVal(double v) : type(VMType::Dec), d_val(v), i_val((int64_t)v) {}
    VMVal(const std::string& v) : type(VMType::Str), s_val(v) {}
    VMVal(const char* v) : type(VMType::Str), s_val(v ? v : "") {}
    VMVal(bool v) : type(VMType::Bool), b_val(v), i_val(v ? 1 : 0) {}
    VMVal(const std::vector<VMVal>& v) : type(VMType::List), list_val(v) {}
    VMVal(const std::vector<uint8_t>& v) : type(VMType::Bytes), bytes_val(v) {}

    static VMVal make_null() { return VMVal(); }

    std::string to_string() const {
        switch (type) {
            case VMType::Null: return "null";
            case VMType::Int: return std::to_string(i_val);
            case VMType::Dec: {
                std::string s = std::to_string(d_val);
                while (s.size() > 1 && s.back() == '0' && s[s.size() - 2] != '.') s.pop_back();
                return s;
            }
            case VMType::Str: return s_val;
            case VMType::Bool: return b_val ? "true" : "false";
            case VMType::List: {
                std::string out = "[";
                for (size_t i = 0; i < list_val.size(); ++i) {
                    out += list_val[i].to_string();
                    if (i + 1 < list_val.size()) out += ", ";
                }
                out += "]";
                return out;
            }
            case VMType::Bytes: {
                return "[Bytes: " + std::to_string(bytes_val.size()) + " B]";
            }
            case VMType::Map: {
                std::string out = "{";
                size_t idx = 0;
                for (const auto& kv : map_val) {
                    out += kv.first + ": " + kv.second.to_string();
                    if (++idx < map_val.size()) out += ", ";
                }
                out += "}";
                return out;
            }
            default: return "";
        }
    }

    int64_t to_int() const {
        if (type == VMType::Int) return i_val;
        if (type == VMType::Dec) return (int64_t)d_val;
        if (type == VMType::Bool) return b_val ? 1 : 0;
        if (type == VMType::Str) {
            try { return std::stoll(s_val); } catch (...) { return 0; }
        }
        return 0;
    }

    double to_dec() const {
        if (type == VMType::Dec) return d_val;
        if (type == VMType::Int) return (double)i_val;
        if (type == VMType::Bool) return b_val ? 1.0 : 0.0;
        if (type == VMType::Str) {
            try { return std::stod(s_val); } catch (...) { return 0.0; }
        }
        return 0.0;
    }

    bool to_bool() const {
        if (type == VMType::Bool) return b_val;
        if (type == VMType::Int) return i_val != 0;
        if (type == VMType::Dec) return d_val != 0.0;
        if (type == VMType::Str) return !s_val.empty();
        if (type == VMType::List) return !list_val.empty();
        if (type == VMType::Bytes) return !bytes_val.empty();
        return false;
    }

    int64_t len() const {
        if (type == VMType::Str) return (int64_t)s_val.size();
        if (type == VMType::List) return (int64_t)list_val.size();
        if (type == VMType::Bytes) return (int64_t)bytes_val.size();
        if (type == VMType::Map) return (int64_t)map_val.size();
        return 0;
    }
};

inline VMVal op_add(const VMVal& a, const VMVal& b) {
    if (a.type == VMType::Str || b.type == VMType::Str) {
        return VMVal(a.to_string() + b.to_string());
    }
    if (a.type == VMType::Dec || b.type == VMType::Dec) {
        return VMVal(a.to_dec() + b.to_dec());
    }
    if (a.type == VMType::List && b.type == VMType::List) {
        auto res = a.list_val;
        res.insert(res.end(), b.list_val.begin(), b.list_val.end());
        return VMVal(res);
    }
    return VMVal(a.to_int() + b.to_int());
}

inline VMVal op_sub(const VMVal& a, const VMVal& b) {
    if (a.type == VMType::Dec || b.type == VMType::Dec) return VMVal(a.to_dec() - b.to_dec());
    return VMVal(a.to_int() - b.to_int());
}

inline VMVal op_mul(const VMVal& a, const VMVal& b) {
    if (a.type == VMType::Str && b.type == VMType::Int) {
        std::string res;
        for (int64_t i = 0; i < b.i_val; ++i) res += a.s_val;
        return VMVal(res);
    }
    if (a.type == VMType::Dec || b.type == VMType::Dec) return VMVal(a.to_dec() * b.to_dec());
    return VMVal(a.to_int() * b.to_int());
}

inline VMVal op_div(const VMVal& a, const VMVal& b) {
    double denom = b.to_dec();
    if (denom == 0.0) return VMVal(0);
    if (a.type == VMType::Dec || b.type == VMType::Dec) return VMVal(a.to_dec() / denom);
    return VMVal(a.to_int() / b.to_int());
}

inline VMVal op_mod(const VMVal& a, const VMVal& b) {
    int64_t denom = b.to_int();
    if (denom == 0) return VMVal(0);
    return VMVal(a.to_int() % denom);
}

inline VMVal op_eq(const VMVal& a, const VMVal& b) {
    if (a.type == VMType::Str || b.type == VMType::Str) return VMVal(a.to_string() == b.to_string());
    if (a.type == VMType::Dec || b.type == VMType::Dec) return VMVal(std::abs(a.to_dec() - b.to_dec()) < 1e-9);
    return VMVal(a.to_int() == b.to_int());
}

inline VMVal op_neq(const VMVal& a, const VMVal& b) {
    return VMVal(!op_eq(a, b).b_val);
}

inline VMVal op_lt(const VMVal& a, const VMVal& b) {
    if (a.type == VMType::Dec || b.type == VMType::Dec) return VMVal(a.to_dec() < b.to_dec());
    if (a.type == VMType::Str && b.type == VMType::Str) return VMVal(a.s_val < b.s_val);
    return VMVal(a.to_int() < b.to_int());
}

inline VMVal op_lte(const VMVal& a, const VMVal& b) {
    if (a.type == VMType::Dec || b.type == VMType::Dec) return VMVal(a.to_dec() <= b.to_dec());
    if (a.type == VMType::Str && b.type == VMType::Str) return VMVal(a.s_val <= b.s_val);
    return VMVal(a.to_int() <= b.to_int());
}

inline VMVal op_gt(const VMVal& a, const VMVal& b) {
    return VMVal(!op_lte(a, b).b_val);
}

inline VMVal op_gte(const VMVal& a, const VMVal& b) {
    return VMVal(!op_lt(a, b).b_val);
}

// =============================================================================
// VM AST NODES
// =============================================================================
struct ASTNode;
using NodePtr = std::shared_ptr<ASTNode>;

enum class FlowSignal {
    None,
    Break,
    Continue,
    Return
};

struct VMContext {
    std::vector<std::unordered_map<std::string, VMVal>> scopes;
    std::unordered_map<std::string, std::shared_ptr<struct UserFunction>> functions;
    FlowSignal signal = FlowSignal::None;
    VMVal return_val;

    VMContext() {
        scopes.emplace_back(); // Global scope
    }

    void push_scope() { scopes.emplace_back(); }
    void pop_scope() { if (scopes.size() > 1) scopes.pop_back(); }

    void set_var(const std::string& name, const VMVal& val) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto fit = it->find(name);
            if (fit != it->end()) {
                fit->second = val;
                return;
            }
        }
        scopes.back()[name] = val;
    }

    VMVal get_var(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto fit = it->find(name);
            if (fit != it->end()) return fit->second;
        }
        return VMVal();
    }

    bool has_var(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->find(name) != it->end()) return true;
        }
        return false;
    }
};

struct UserFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<NodePtr> body;
};

struct ASTNode {
    int line = 0;
    virtual ~ASTNode() = default;
    virtual VMVal execute(VMContext& ctx) = 0;
};

// =============================================================================
// EXPRESSION PARSER & EVALUATOR
// =============================================================================
class ExprEvaluator {
private:
    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static std::vector<std::string> splitArgs(const std::string& s) {
        std::vector<std::string> res;
        std::string cur;
        int paren = 0, brace = 0, bracket = 0;
        bool in_str = false;
        char quote = 0;

        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (in_str) {
                cur += c;
                if (c == quote && (i == 0 || s[i-1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') { in_str = true; quote = c; cur += c; }
                else if (c == '(') { paren++; cur += c; }
                else if (c == ')') { paren--; cur += c; }
                else if (c == '{') { brace++; cur += c; }
                else if (c == '}') { brace--; cur += c; }
                else if (c == '[') { bracket++; cur += c; }
                else if (c == ']') { bracket--; cur += c; }
                else if (c == ',' && paren == 0 && brace == 0 && bracket == 0) {
                    res.push_back(trim(cur));
                    cur.clear();
                } else {
                    cur += c;
                }
            }
        }
        if (!cur.empty()) res.push_back(trim(cur));
        return res;
    }

public:
    static VMVal evaluate(const std::string& raw_expr, VMContext& ctx) {
        std::string expr = trim(raw_expr);
        if (expr.empty()) return VMVal();

        // Strip outer parens if fully enclosed
        if (expr.front() == '(' && expr.back() == ')') {
            int depth = 0;
            bool ok = true;
            for (size_t i = 0; i < expr.size() - 1; ++i) {
                if (expr[i] == '(') depth++;
                else if (expr[i] == ')') { depth--; if (depth == 0) { ok = false; break; } }
            }
            if (ok && depth == 1) return evaluate(expr.substr(1, expr.size() - 2), ctx);
        }

        // Logical OR: ||, or
        int paren = 0, brace = 0, bracket = 0;
        bool in_str = false;
        char quote = 0;
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (in_str) {
                if (c == quote && (i == 0 || expr[i-1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') { in_str = true; quote = c; }
                else if (c == ')') paren++;
                else if (c == '(') paren--;
                else if (c == '}') brace++;
                else if (c == '{') brace--;
                else if (c == ']') bracket++;
                else if (c == '[') bracket--;
                else if (paren == 0 && brace == 0 && bracket == 0) {
                    if (i > 0 && expr[i-1] == '|' && expr[i] == '|') {
                        auto left = evaluate(expr.substr(0, i - 1), ctx);
                        if (left.to_bool()) return VMVal(true);
                        return evaluate(expr.substr(i + 1), ctx);
                    }
                }
            }
        }

        // Logical AND: &&, and
        paren = brace = bracket = 0; in_str = false;
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (in_str) {
                if (c == quote && (i == 0 || expr[i-1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') { in_str = true; quote = c; }
                else if (c == ')') paren++;
                else if (c == '(') paren--;
                else if (c == '}') brace++;
                else if (c == '{') brace--;
                else if (c == ']') bracket++;
                else if (c == '[') bracket--;
                else if (paren == 0 && brace == 0 && bracket == 0) {
                    if (i > 0 && expr[i-1] == '&' && expr[i] == '&') {
                        auto left = evaluate(expr.substr(0, i - 1), ctx);
                        if (!left.to_bool()) return VMVal(false);
                        return evaluate(expr.substr(i + 1), ctx);
                    }
                }
            }
        }

        // Comparisons: ==, !=, <=, >=, <, >
        paren = brace = bracket = 0; in_str = false;
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (in_str) {
                if (c == quote && (i == 0 || expr[i-1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') { in_str = true; quote = c; }
                else if (c == ')') paren++;
                else if (c == '(') paren--;
                else if (c == '}') brace++;
                else if (c == '{') brace--;
                else if (c == ']') bracket++;
                else if (c == '[') bracket--;
                else if (paren == 0 && brace == 0 && bracket == 0) {
                    if (i > 0 && expr[i-1] == '=' && expr[i] == '=') return op_eq(evaluate(expr.substr(0, i-1), ctx), evaluate(expr.substr(i+1), ctx));
                    if (i > 0 && expr[i-1] == '!' && expr[i] == '=') return op_neq(evaluate(expr.substr(0, i-1), ctx), evaluate(expr.substr(i+1), ctx));
                    if (i > 0 && expr[i-1] == '<' && expr[i] == '=') return op_lte(evaluate(expr.substr(0, i-1), ctx), evaluate(expr.substr(i+1), ctx));
                    if (i > 0 && expr[i-1] == '>' && expr[i] == '=') return op_gte(evaluate(expr.substr(0, i-1), ctx), evaluate(expr.substr(i+1), ctx));
                    if (expr[i] == '<') return op_lt(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                    if (expr[i] == '>') return op_gt(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                }
            }
        }

        // Addition and Subtraction: +, -
        paren = brace = bracket = 0; in_str = false;
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (in_str) {
                if (c == quote && (i == 0 || expr[i-1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') { in_str = true; quote = c; }
                else if (c == ')') paren++;
                else if (c == '(') paren--;
                else if (c == '}') brace++;
                else if (c == '{') brace--;
                else if (c == ']') bracket++;
                else if (c == '[') bracket--;
                else if (paren == 0 && brace == 0 && bracket == 0) {
                    if (c == '+' && i > 0) return op_add(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                    if (c == '-' && i > 0 && expr[i-1] != '*' && expr[i-1] != '/' && expr[i-1] != '+' && expr[i-1] != '-') {
                        return op_sub(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                    }
                }
            }
        }

        // Multiplication, Division, Modulo: *, /, %
        paren = brace = bracket = 0; in_str = false;
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (in_str) {
                if (c == quote && (i == 0 || expr[i-1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') { in_str = true; quote = c; }
                else if (c == ')') paren++;
                else if (c == '(') paren--;
                else if (c == '}') brace++;
                else if (c == '{') brace--;
                else if (c == ']') bracket++;
                else if (c == '[') bracket--;
                else if (paren == 0 && brace == 0 && bracket == 0) {
                    if (c == '*') return op_mul(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                    if (c == '/') return op_div(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                    if (c == '%') return op_mod(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                }
            }
        }

        // Unary NOT: !expr
        if (expr.front() == '!' && expr.size() > 1 && expr[1] != '!') {
            return VMVal(!evaluate(expr.substr(1), ctx).to_bool());
        }
        // Unary Negation: -expr
        if (expr.front() == '-' && expr.size() > 1) {
            auto val = evaluate(expr.substr(1), ctx);
            if (val.type == VMType::Dec) return VMVal(-val.d_val);
            return VMVal(-val.i_val);
        }

        // String Literals
        if ((expr.front() == '"' && expr.back() == '"') || (expr.front() == '\'' && expr.back() == '\'')) {
            std::string content = expr.substr(1, expr.size() - 2);
            std::string clean;
            for (size_t i = 0; i < content.size(); ++i) {
                if (content[i] == '\\' && i + 1 < content.size()) {
                    char esc = content[++i];
                    if (esc == 'n') clean += '\n';
                    else if (esc == 't') clean += '\t';
                    else if (esc == 'r') clean += '\r';
                    else if (esc == '\\') clean += '\\';
                    else if (esc == '"') clean += '"';
                    else clean += esc;
                } else {
                    clean += content[i];
                }
            }
            return VMVal(clean);
        }

        // List Literals: [1, 2, 3]
        if (expr.front() == '[' && expr.back() == ']') {
            std::string inner = trim(expr.substr(1, expr.size() - 2));
            if (inner.empty()) return VMVal(std::vector<VMVal>{});
            auto items = splitArgs(inner);
            std::vector<VMVal> list;
            for (const auto& itm : items) list.push_back(evaluate(itm, ctx));
            return VMVal(list);
        }

        // Booleans & Null
        if (expr == "true") return VMVal(true);
        if (expr == "false") return VMVal(false);
        if (expr == "null") return VMVal();

        // Numbers
        bool is_num = true, has_dot = false;
        size_t start_idx = (expr[0] == '-') ? 1 : 0;
        if (start_idx >= expr.size()) is_num = false;
        for (size_t i = start_idx; i < expr.size(); ++i) {
            if (expr[i] == '.') {
                if (has_dot) { is_num = false; break; }
                has_dot = true;
            } else if (!std::isdigit(expr[i])) {
                is_num = false;
                break;
            }
        }
        if (is_num) {
            if (has_dot) return VMVal(std::stod(expr));
            return VMVal(std::stoll(expr));
        }

        // Method calls on expressions or variables: obj.method(args) or obj.property
        size_t dot_pos = std::string::npos;
        paren = brace = bracket = 0; in_str = false;
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (in_str) {
                if (c == quote && (i == 0 || expr[i-1] != '\\')) in_str = false;
            } else {
                if (c == '"' || c == '\'') { in_str = true; quote = c; }
                else if (c == ')') paren++;
                else if (c == '(') paren--;
                else if (c == '}') brace++;
                else if (c == '{') brace--;
                else if (c == ']') bracket++;
                else if (c == '[') bracket--;
                else if (paren == 0 && brace == 0 && bracket == 0 && c == '.') {
                    dot_pos = i;
                    break;
                }
            }
        }

        if (dot_pos != std::string::npos) {
            std::string obj_expr = trim(expr.substr(0, dot_pos));
            std::string member_expr = trim(expr.substr(dot_pos + 1));

            // Module Calls: io.println, math.sqrt, audio.sfx, sys.sleep, media, gui
            if (obj_expr == "io" || obj_expr == "rt" || obj_expr == "sys" || obj_expr == "math" || obj_expr == "audio" || obj_expr == "snd" || obj_expr == "sound" || obj_expr == "str" || obj_expr == "time" || obj_expr == "media" || obj_expr == "tag" || obj_expr == "gui" || obj_expr == "ui" || obj_expr == "window" || obj_expr == "win") {
                return callModule(obj_expr, member_expr, ctx);
            }

            // Object Member / Method Evaluation
            auto target = evaluate(obj_expr, ctx);
            std::string mname = member_expr;
            std::string margs = "";
            size_t m_paren = member_expr.find('(');
            if (m_paren != std::string::npos && member_expr.back() == ')') {
                mname = trim(member_expr.substr(0, m_paren));
                margs = member_expr.substr(m_paren + 1, member_expr.size() - m_paren - 2);
            }

            // Properties
            if (mname == "len" || mname == "length" || mname == "get_len" || mname == "size") {
                return VMVal(target.len());
            }
            if (mname == "first" || mname == "get_first") {
                if (target.type == VMType::List && !target.list_val.empty()) return target.list_val.front();
                if (target.type == VMType::Str && !target.s_val.empty()) return VMVal(std::string(1, target.s_val.front()));
                return VMVal();
            }
            if (mname == "last" || mname == "get_last") {
                if (target.type == VMType::List && !target.list_val.empty()) return target.list_val.back();
                if (target.type == VMType::Str && !target.s_val.empty()) return VMVal(std::string(1, target.s_val.back()));
                return VMVal();
            }

            // Methods
            if (mname == "upper" && target.type == VMType::Str) {
                std::string s = target.s_val;
                for (char& c : s) c = (char)std::toupper(c);
                return VMVal(s);
            }
            if (mname == "lower" && target.type == VMType::Str) {
                std::string s = target.s_val;
                for (char& c : s) c = (char)std::tolower(c);
                return VMVal(s);
            }

            // Higher-Order Lambda Methods: .map(|x| ...), .filter(|x| ...), .reduce(init, |acc, x| ...)
            if (target.type == VMType::List) {
                if (mname == "map") {
                    // Extract lambda: |param| body
                    std::string arg = trim(margs);
                    if (arg.front() == '|' && arg.find('|', 1) != std::string::npos) {
                        size_t p2 = arg.find('|', 1);
                        std::string p_name = trim(arg.substr(1, p2 - 1));
                        if (p_name.front() == '@') p_name = p_name.substr(1);
                        std::string l_body = trim(arg.substr(p2 + 1));
                        std::vector<VMVal> mapped;
                        for (const auto& item : target.list_val) {
                            ctx.push_scope();
                            ctx.set_var(p_name, item);
                            mapped.push_back(evaluate(l_body, ctx));
                            ctx.pop_scope();
                        }
                        return VMVal(mapped);
                    }
                } else if (mname == "filter") {
                    std::string arg = trim(margs);
                    if (arg.front() == '|' && arg.find('|', 1) != std::string::npos) {
                        size_t p2 = arg.find('|', 1);
                        std::string p_name = trim(arg.substr(1, p2 - 1));
                        if (p_name.front() == '@') p_name = p_name.substr(1);
                        std::string l_body = trim(arg.substr(p2 + 1));
                        std::vector<VMVal> filtered;
                        for (const auto& item : target.list_val) {
                            ctx.push_scope();
                            ctx.set_var(p_name, item);
                            if (evaluate(l_body, ctx).to_bool()) filtered.push_back(item);
                            ctx.pop_scope();
                        }
                        return VMVal(filtered);
                    }
                } else if (mname == "reduce") {
                    auto args = splitArgs(margs);
                    if (args.size() == 2) {
                        VMVal acc = evaluate(args[0], ctx);
                        std::string l_expr = args[1];
                        if (l_expr.front() == '|' && l_expr.find('|', 1) != std::string::npos) {
                            size_t p2 = l_expr.find('|', 1);
                            auto p_list = splitArgs(l_expr.substr(1, p2 - 1));
                            std::string acc_name = trim(p_list[0]);
                            std::string item_name = trim(p_list[1]);
                            if (acc_name.front() == '@') acc_name = acc_name.substr(1);
                            if (item_name.front() == '@') item_name = item_name.substr(1);
                            std::string l_body = trim(l_expr.substr(p2 + 1));
                            for (const auto& item : target.list_val) {
                                ctx.push_scope();
                                ctx.set_var(acc_name, acc);
                                ctx.set_var(item_name, item);
                                acc = evaluate(l_body, ctx);
                                ctx.pop_scope();
                            }
                            return acc;
                        }
                    }
                }
            }

            // Buffer Methods: read_u8, read_int, read_str, size
            if (target.type == VMType::Bytes) {
                if (mname == "read_u8") {
                    int64_t offset = margs.empty() ? 0 : evaluate(margs, ctx).to_int();
                    if (offset >= 0 && offset < (int64_t)target.bytes_val.size()) {
                        return VMVal((int64_t)target.bytes_val[offset]);
                    }
                    return VMVal(0);
                }
                if (mname == "read_int") {
                    int64_t offset = margs.empty() ? 0 : evaluate(margs, ctx).to_int();
                    if (offset >= 0 && offset + 8 <= (int64_t)target.bytes_val.size()) {
                        int64_t v = 0;
                        std::memcpy(&v, &target.bytes_val[offset], 8);
                        return VMVal(v);
                    }
                    return VMVal(0);
                }
            }
        }

        // Subscript / Indexing: expr[idx]
        if (expr.back() == ']') {
            size_t b_start = expr.find_last_of('[');
            if (b_start != std::string::npos) {
                std::string base = trim(expr.substr(0, b_start));
                std::string idx_s = expr.substr(b_start + 1, expr.size() - b_start - 2);
                auto val = evaluate(base, ctx);
                auto idx_val = evaluate(idx_s, ctx);
                int64_t idx = idx_val.to_int();
                if (val.type == VMType::List) {
                    if (idx >= 0 && idx < (int64_t)val.list_val.size()) return val.list_val[idx];
                } else if (val.type == VMType::Str) {
                    if (idx >= 0 && idx < (int64_t)val.s_val.size()) return VMVal(std::string(1, val.s_val[idx]));
                } else if (val.type == VMType::Bytes) {
                    if (idx >= 0 && idx < (int64_t)val.bytes_val.size()) return VMVal((int64_t)val.bytes_val[idx]);
                } else if (val.type == VMType::Map) {
                    auto fit = val.map_val.find(idx_val.to_string());
                    if (fit != val.map_val.end()) return fit->second;
                }
                return VMVal();
            }
        }

        // Function Calls: name(args)
        size_t p_open = expr.find('(');
        if (p_open != std::string::npos && expr.back() == ')') {
            std::string fname = trim(expr.substr(0, p_open));
            std::string args_str = expr.substr(p_open + 1, expr.size() - p_open - 2);
            auto args_raw = splitArgs(args_str);
            std::vector<VMVal> args;
            for (const auto& a : args_raw) args.push_back(evaluate(a, ctx));

            auto fit = ctx.functions.find(fname);
            if (fit != ctx.functions.end()) {
                auto fn = fit->second;
                ctx.push_scope();
                for (size_t pi = 0; pi < fn->params.size(); ++pi) {
                    VMVal arg_v = (pi < args.size()) ? args[pi] : VMVal();
                    ctx.set_var(fn->params[pi], arg_v);
                }
                VMVal ret_v;
                for (const auto& node : fn->body) {
                    ret_v = node->execute(ctx);
                    if (ctx.signal == FlowSignal::Return) {
                        ret_v = ctx.return_val;
                        ctx.signal = FlowSignal::None;
                        break;
                    }
                }
                ctx.pop_scope();
                return ret_v;
            }
        }

        // Variable Lookup: @var or var or &buf
        std::string vname = expr;
        if (vname.front() == '@' || vname.front() == '&') vname = vname.substr(1);
        if (ctx.has_var(vname)) {
            return ctx.get_var(vname);
        }

        return VMVal();
    }

    static VMVal callModule(const std::string& mod, const std::string& call, VMContext& ctx) {
        std::string fname = call;
        std::string args_str = "";
        size_t p_pos = call.find('(');
        if (p_pos != std::string::npos && call.back() == ')') {
            fname = trim(call.substr(0, p_pos));
            args_str = call.substr(p_pos + 1, call.size() - p_pos - 2);
        }
        auto raw_args = splitArgs(args_str);
        std::vector<VMVal> args;
        for (const auto& a : raw_args) args.push_back(evaluate(a, ctx));

        // IO Module
        if (mod == "io") {
            if (fname == "println") {
                if (args.empty()) std::cout << "\n";
                else {
                    for (size_t i = 0; i < args.size(); ++i) {
                        std::cout << args[i].to_string();
                        if (i + 1 < args.size()) std::cout << " ";
                    }
                    std::cout << "\n";
                }
                return VMVal();
            }
            if (fname == "print") {
                for (size_t i = 0; i < args.size(); ++i) {
                    std::cout << args[i].to_string();
                    if (i + 1 < args.size()) std::cout << " ";
                }
                std::cout << std::flush;
                return VMVal();
            }
            if (fname == "readln" || fname == "input") {
                std::string line;
                std::getline(std::cin, line);
                return VMVal(line);
            }
            if (fname == "clear") {
                #ifdef _WIN32
                std::system("cls");
                #else
                std::system("clear");
                #endif
                return VMVal();
            }
        }

        // MATH Module
        if (mod == "math") {
            if (fname == "sqrt" && !args.empty()) return VMVal(std::sqrt(args[0].to_dec()));
            if (fname == "sin" && !args.empty()) return VMVal(std::sin(args[0].to_dec()));
            if (fname == "cos" && !args.empty()) return VMVal(std::cos(args[0].to_dec()));
            if (fname == "abs" && !args.empty()) return VMVal(std::abs(args[0].to_dec()));
            if (fname == "min" && args.size() >= 2) return VMVal(std::min(args[0].to_dec(), args[1].to_dec()));
            if (fname == "max" && args.size() >= 2) return VMVal(std::max(args[0].to_dec(), args[1].to_dec()));
            if (fname == "round" && !args.empty()) return VMVal((int64_t)std::round(args[0].to_dec()));
            if (fname == "random_int" && args.size() >= 2) {
                int64_t low = args[0].to_int();
                int64_t high = args[1].to_int();
                if (high <= low) return VMVal(low);
                return VMVal(low + (rand() % (high - low + 1)));
            }
            if (fname == "random_dec" || fname == "random") {
                return VMVal((double)rand() / (double)RAND_MAX);
            }
        }

        // AUDIO Module
        if (mod == "audio" || mod == "snd" || mod == "sound") {
            if (fname == "sfx" && !args.empty()) {
                viss::audio::sfx(args[0].to_string());
                return VMVal();
            }
            if (fname == "play_note" && !args.empty()) {
                std::string note = args[0].to_string();
                int dur = args.size() > 1 ? (int)args[1].to_int() : 100;
                std::string wave = args.size() > 2 ? args[2].to_string() : "sine";
                double vol = args.size() > 3 ? args[3].to_dec() : 0.5;
                viss::audio::play_note(note, dur, wave, vol);
                return VMVal();
            }
            if (fname == "beep" && !args.empty()) {
                double freq = args[0].to_dec();
                int dur = args.size() > 1 ? (int)args[1].to_int() : 100;
                viss::audio::play_tone(freq, dur);
                return VMVal();
            }
            if (fname == "set_volume" && !args.empty()) {
                viss::audio::set_volume(args[0].to_dec());
                return VMVal();
            }
            if (fname == "synth" && args.size() >= 7) {
                viss::audio::synth(
                    args[0].to_dec(), (int)args[1].to_int(), args[2].to_string(),
                    args[3].to_dec(), args[4].to_dec(), args[5].to_dec(), args[6].to_dec(),
                    args.size() > 7 ? args[7].to_dec() : 0.5
                );
                return VMVal();
            }
        }

        // SYS Module
        if (mod == "sys") {
            if (fname == "sleep" && !args.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(args[0].to_int()));
                return VMVal();
            }
            if (fname == "platform") {
                #ifdef _WIN32
                return VMVal("windows");
                #else
                return VMVal("posix");
                #endif
            }
            if (fname == "exit") {
                int code = args.empty() ? 0 : (int)args[0].to_int();
                std::exit(code);
            }
        }

        // TIME Module
        if (mod == "time") {
            if (fname == "ms" || fname == "now") {
                auto now = std::chrono::steady_clock::now();
                return VMVal((int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
            }
        }

        // STR Module
        if (mod == "str") {
            if (fname == "len" && !args.empty()) return VMVal(args[0].len());
            if (fname == "upper" && !args.empty()) {
                std::string s = args[0].to_string();
                for (char& c : s) c = (char)std::toupper(c);
                return VMVal(s);
            }
            if (fname == "lower" && !args.empty()) {
                std::string s = args[0].to_string();
                for (char& c : s) c = (char)std::tolower(c);
                return VMVal(s);
            }
        }

        // MEDIA & TAG Module
        if (mod == "media" || mod == "tag") {
            if (fname == "is_file_ready" && !args.empty()) {
                return VMVal(viss::media::is_file_ready(args[0].to_string()));
            }
            if (fname == "get_duration" && !args.empty()) {
                return VMVal(viss::media::get_duration(args[0].to_string()));
            }
            if (fname == "read" && !args.empty()) {
                auto tag = viss::media::read(args[0].to_string());
                std::map<std::string, VMVal> m;
                m["title"] = VMVal(tag.title);
                m["artist"] = VMVal(tag.artist);
                m["album"] = VMVal(tag.album);
                m["year"] = VMVal(tag.year);
                m["genre"] = VMVal(tag.genre);
                m["format"] = VMVal(tag.format);
                m["duration"] = VMVal(tag.duration);
                m["sample_rate"] = VMVal((int64_t)tag.sample_rate);
                m["channels"] = VMVal((int64_t)tag.channels);
                m["has_cover"] = VMVal(tag.has_cover);
                VMVal res;
                res.type = VMType::Map;
                res.map_val = m;
                return res;
            }
            if (fname == "embed_cover" && args.size() >= 2) {
                std::string a_path = args[0].to_string();
                std::string img_path = args[1].to_string();
                std::string out_p = (args.size() > 2) ? args[2].to_string() : "";
                return VMVal(viss::media::embed_cover(a_path, img_path, out_p));
            }
            if (fname == "extract_cover" && args.size() >= 2) {
                return VMVal(viss::media::extract_cover(args[0].to_string(), args[1].to_string()));
            }
        }

        // GUI & WINDOW Module
        if (mod == "gui" || mod == "ui" || mod == "window" || mod == "win") {
            if (fname == "create" || fname == "window") {
                std::string title = !args.empty() ? args[0].to_string() : "Viss Window";
                int w = args.size() > 1 ? (int)args[1].to_int() : 800;
                int h = args.size() > 2 ? (int)args[2].to_int() : 600;
                return VMVal(viss::gui::create(title, w, h));
            }
            if (fname == "is_open") return VMVal(viss::gui::is_open());
            if (fname == "poll") return VMVal(viss::gui::poll());
            if (fname == "clear") {
                viss::gui::clear();
                return VMVal();
            }
            if (fname == "update") {
                viss::gui::update();
                return VMVal();
            }
            if (fname == "close") {
                viss::gui::close();
                return VMVal();
            }
            if (fname == "width") return VMVal((int64_t)viss::gui::width());
            if (fname == "height") return VMVal((int64_t)viss::gui::height());
            if (fname == "mouse_x") return VMVal((int64_t)viss::gui::mouse_x());
            if (fname == "mouse_y") return VMVal((int64_t)viss::gui::mouse_y());
            if (fname == "mouse_clicked") return VMVal(viss::gui::mouse_clicked());
            if (fname == "mouse_down") return VMVal(viss::gui::mouse_down());
            if (fname == "button" && args.size() >= 5) {
                return VMVal(viss::gui::button((int)args[0].to_int(), (int)args[1].to_int(), (int)args[2].to_int(), (int)args[3].to_int(), args[4].to_string()));
            }
            if (fname == "label" && args.size() >= 3) {
                viss::gui::label((int)args[0].to_int(), (int)args[1].to_int(), args[2].to_string());
                return VMVal();
            }
            if (fname == "draw_rect" && args.size() >= 4) {
                viss::gui::draw_rect((int)args[0].to_int(), (int)args[1].to_int(), (int)args[2].to_int(), (int)args[3].to_int(), viss::gui::Colors::PanelBg);
                return VMVal();
            }
            if (fname == "draw_text" && args.size() >= 3) {
                viss::gui::draw_text((int)args[0].to_int(), (int)args[1].to_int(), args[2].to_string());
                return VMVal();
            }
        }

        return VMVal();
    }
};

// =============================================================================
// CONCRETE AST STATEMENT IMPLEMENTATIONS
// =============================================================================

// Variable Assignment: @x = expr or @x += expr
struct StmtAssign : public ASTNode {
    std::string var_name;
    std::string op;
    std::string expr;

    StmtAssign(const std::string& vn, const std::string& o, const std::string& ex)
        : var_name(vn), op(o), expr(ex) {}

    VMVal execute(VMContext& ctx) override {
        VMVal val = ExprEvaluator::evaluate(expr, ctx);
        if (op == "=") {
            ctx.set_var(var_name, val);
        } else if (op == "+=") {
            ctx.set_var(var_name, op_add(ctx.get_var(var_name), val));
        } else if (op == "-=") {
            ctx.set_var(var_name, op_sub(ctx.get_var(var_name), val));
        } else if (op == "*=") {
            ctx.set_var(var_name, op_mul(ctx.get_var(var_name), val));
        } else if (op == "/=") {
            ctx.set_var(var_name, op_div(ctx.get_var(var_name), val));
        }
        return val;
    }
};

// Subscript Assignment: @arr[idx] = expr
struct StmtSubscriptAssign : public ASTNode {
    std::string var_name;
    std::string idx_expr;
    std::string val_expr;

    StmtSubscriptAssign(const std::string& vn, const std::string& ie, const std::string& ve)
        : var_name(vn), idx_expr(ie), val_expr(ve) {}

    VMVal execute(VMContext& ctx) override {
        auto cur = ctx.get_var(var_name);
        int64_t idx = ExprEvaluator::evaluate(idx_expr, ctx).to_int();
        auto val = ExprEvaluator::evaluate(val_expr, ctx);

        if (cur.type == VMType::List) {
            if (idx >= 0 && idx < (int64_t)cur.list_val.size()) {
                cur.list_val[idx] = val;
                ctx.set_var(var_name, cur);
            }
        } else if (cur.type == VMType::Bytes) {
            if (idx >= 0 && idx < (int64_t)cur.bytes_val.size()) {
                cur.bytes_val[idx] = (uint8_t)val.to_int();
                ctx.set_var(var_name, cur);
            }
        }
        return val;
    }
};

// Expression Statement (e.g. io.println(...))
struct StmtExpr : public ASTNode {
    std::string expr;
    StmtExpr(const std::string& ex) : expr(ex) {}

    VMVal execute(VMContext& ctx) override {
        return ExprEvaluator::evaluate(expr, ctx);
    }
};

// If-Elif-Else Statement
struct StmtIf : public ASTNode {
    std::string cond;
    std::vector<NodePtr> then_branch;
    std::vector<std::pair<std::string, std::vector<NodePtr>>> elif_branches;
    std::vector<NodePtr> else_branch;

    VMVal execute(VMContext& ctx) override {
        if (ExprEvaluator::evaluate(cond, ctx).to_bool()) {
            ctx.push_scope();
            for (const auto& node : then_branch) {
                node->execute(ctx);
                if (ctx.signal != FlowSignal::None) break;
            }
            ctx.pop_scope();
            return VMVal();
        }

        for (const auto& elif : elif_branches) {
            if (ExprEvaluator::evaluate(elif.first, ctx).to_bool()) {
                ctx.push_scope();
                for (const auto& node : elif.second) {
                    node->execute(ctx);
                    if (ctx.signal != FlowSignal::None) break;
                }
                ctx.pop_scope();
                return VMVal();
            }
        }

        if (!else_branch.empty()) {
            ctx.push_scope();
            for (const auto& node : else_branch) {
                node->execute(ctx);
                if (ctx.signal != FlowSignal::None) break;
            }
            ctx.pop_scope();
        }
        return VMVal();
    }
};

// While Loop
struct StmtWhile : public ASTNode {
    std::string cond;
    std::vector<NodePtr> body;

    StmtWhile(const std::string& c, const std::vector<NodePtr>& b) : cond(c), body(b) {}

    VMVal execute(VMContext& ctx) override {
        while (ExprEvaluator::evaluate(cond, ctx).to_bool()) {
            ctx.push_scope();
            for (const auto& node : body) {
                node->execute(ctx);
                if (ctx.signal != FlowSignal::None) break;
            }
            ctx.pop_scope();
            if (ctx.signal == FlowSignal::Break) {
                ctx.signal = FlowSignal::None;
                break;
            }
            if (ctx.signal == FlowSignal::Continue) {
                ctx.signal = FlowSignal::None;
            }
            if (ctx.signal == FlowSignal::Return) break;
        }
        return VMVal();
    }
};

// For Range Loop: !for @i in 0..10 { ... }
struct StmtForRange : public ASTNode {
    std::string var_name;
    std::string start_expr;
    std::string end_expr;
    std::vector<NodePtr> body;

    StmtForRange(const std::string& vn, const std::string& se, const std::string& ee, const std::vector<NodePtr>& b)
        : var_name(vn), start_expr(se), end_expr(ee), body(b) {}

    VMVal execute(VMContext& ctx) override {
        int64_t start_v = ExprEvaluator::evaluate(start_expr, ctx).to_int();
        int64_t end_v = ExprEvaluator::evaluate(end_expr, ctx).to_int();

        ctx.push_scope();
        for (int64_t i = start_v; i < end_v; ++i) {
            ctx.set_var(var_name, VMVal(i));
            for (const auto& node : body) {
                node->execute(ctx);
                if (ctx.signal != FlowSignal::None) break;
            }
            if (ctx.signal == FlowSignal::Break) {
                ctx.signal = FlowSignal::None;
                break;
            }
            if (ctx.signal == FlowSignal::Continue) {
                ctx.signal = FlowSignal::None;
            }
            if (ctx.signal == FlowSignal::Return) break;
        }
        ctx.pop_scope();
        return VMVal();
    }
};

// For-in Collection Loop: !for @x in @list { ... }
struct StmtForIn : public ASTNode {
    std::string var_name;
    std::string coll_expr;
    std::vector<NodePtr> body;

    StmtForIn(const std::string& vn, const std::string& ce, const std::vector<NodePtr>& b)
        : var_name(vn), coll_expr(ce), body(b) {}

    VMVal execute(VMContext& ctx) override {
        auto coll = ExprEvaluator::evaluate(coll_expr, ctx);
        ctx.push_scope();
        if (coll.type == VMType::List) {
            for (const auto& item : coll.list_val) {
                ctx.set_var(var_name, item);
                for (const auto& node : body) {
                    node->execute(ctx);
                    if (ctx.signal != FlowSignal::None) break;
                }
                if (ctx.signal == FlowSignal::Break) { ctx.signal = FlowSignal::None; break; }
                if (ctx.signal == FlowSignal::Continue) ctx.signal = FlowSignal::None;
                if (ctx.signal == FlowSignal::Return) break;
            }
        }
        ctx.pop_scope();
        return VMVal();
    }
};

// Loop Repeat: !loop (count) { ... }
struct StmtLoopRepeat : public ASTNode {
    std::string count_expr;
    std::vector<NodePtr> body;

    StmtLoopRepeat(const std::string& ce, const std::vector<NodePtr>& b)
        : count_expr(ce), body(b) {}

    VMVal execute(VMContext& ctx) override {
        int64_t count = ExprEvaluator::evaluate(count_expr, ctx).to_int();
        ctx.push_scope();
        for (int64_t i = 0; i < count; ++i) {
            for (const auto& node : body) {
                node->execute(ctx);
                if (ctx.signal != FlowSignal::None) break;
            }
            if (ctx.signal == FlowSignal::Break) { ctx.signal = FlowSignal::None; break; }
            if (ctx.signal == FlowSignal::Continue) ctx.signal = FlowSignal::None;
            if (ctx.signal == FlowSignal::Return) break;
        }
        ctx.pop_scope();
        return VMVal();
    }
};

// Buffer Creation: &buf create | bytes, 16
struct StmtBufferCreate : public ASTNode {
    std::string buf_name;
    std::string btype;
    std::string size_expr;

    StmtBufferCreate(const std::string& bn, const std::string& bt, const std::string& se)
        : buf_name(bn), btype(bt), size_expr(se) {}

    VMVal execute(VMContext& ctx) override {
        int64_t sz = size_expr.empty() ? 1024 : ExprEvaluator::evaluate(size_expr, ctx).to_int();
        std::vector<uint8_t> bytes((size_t)sz, 0);
        VMVal val(bytes);
        ctx.set_var(buf_name, val);
        return val;
    }
};

// Buffer Write: &buf write u8 255 at 0; or &buf write "hello"
struct StmtBufferWrite : public ASTNode {
    std::string buf_name;
    std::string wtype;
    std::string val_expr;
    std::string offset_expr;

    StmtBufferWrite(const std::string& bn, const std::string& wt, const std::string& ve, const std::string& oe)
        : buf_name(bn), wtype(wt), val_expr(ve), offset_expr(oe) {}

    VMVal execute(VMContext& ctx) override {
        auto cur = ctx.get_var(buf_name);
        if (cur.type != VMType::Bytes) return VMVal();

        int64_t offset = offset_expr.empty() ? 0 : ExprEvaluator::evaluate(offset_expr, ctx).to_int();
        auto val = ExprEvaluator::evaluate(val_expr, ctx);

        if (wtype == "str" || wtype == "string") {
            std::string s = val.to_string();
            if (offset + (int64_t)s.size() > (int64_t)cur.bytes_val.size()) cur.bytes_val.resize(offset + s.size());
            std::memcpy(&cur.bytes_val[offset], s.data(), s.size());
        } else if (wtype == "int" || wtype == "i64" || wtype == "u64") {
            int64_t v = val.to_int();
            if (offset + 8 > (int64_t)cur.bytes_val.size()) cur.bytes_val.resize(offset + 8);
            std::memcpy(&cur.bytes_val[offset], &v, 8);
        } else {
            // Default u8 / byte
            uint8_t b = (uint8_t)val.to_int();
            if (offset + 1 > (int64_t)cur.bytes_val.size()) cur.bytes_val.resize(offset + 1);
            cur.bytes_val[offset] = b;
        }
        ctx.set_var(buf_name, cur);
        return val;
    }
};

// Flow Control Nodes
struct StmtReturn : public ASTNode {
    std::string expr;
    StmtReturn(const std::string& ex) : expr(ex) {}
    VMVal execute(VMContext& ctx) override {
        ctx.signal = FlowSignal::Return;
        ctx.return_val = expr.empty() ? VMVal() : ExprEvaluator::evaluate(expr, ctx);
        return ctx.return_val;
    }
};

struct StmtBreak : public ASTNode {
    VMVal execute(VMContext& ctx) override {
        ctx.signal = FlowSignal::Break;
        return VMVal();
    }
};

struct StmtContinue : public ASTNode {
    VMVal execute(VMContext& ctx) override {
        ctx.signal = FlowSignal::Continue;
        return VMVal();
    }
};

// =============================================================================
// COMPLETE DIRECT PARSER & VM EXECUTION ENGINE
// =============================================================================
class VissVM {
private:
    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static bool startsWith(const std::string& s, const std::string& p) {
        return s.rfind(p, 0) == 0;
    }

    static bool endsWith(const std::string& s, const std::string& suf) {
        if (s.size() < suf.size()) return false;
        return s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    }

    static std::vector<std::string> tokenizeLines(const std::string& source) {
        std::vector<std::string> lines;
        std::stringstream ss(source);
        std::string line;
        while (std::getline(ss, line)) {
            // Strip single-line comments // or #
            size_t c_pos = line.find("//");
            if (c_pos != std::string::npos) line = line.substr(0, c_pos);
            size_t h_pos = line.find('#');
            if (h_pos != std::string::npos && (h_pos == 0 || line[h_pos-1] == ' ')) line = line.substr(0, h_pos);
            line = trim(line);
            if (!line.empty()) lines.push_back(line);
        }
        return lines;
    }

public:
    static int executeSource(const std::string& source, const std::vector<std::string>& args = {}) {
        VMContext ctx;
        auto lines = tokenizeLines(source);

        std::vector<NodePtr> root_nodes;
        size_t i = 0;
        parseBlock(lines, i, root_nodes, ctx);

        // Execute top-level / functions
        for (const auto& node : root_nodes) {
            node->execute(ctx);
            if (ctx.signal == FlowSignal::Return) break;
        }

        // Check if main() exists
        auto fit = ctx.functions.find("main");
        if (fit != ctx.functions.end()) {
            ctx.push_scope();
            for (const auto& node : fit->second->body) {
                node->execute(ctx);
                if (ctx.signal == FlowSignal::Return) break;
            }
            ctx.pop_scope();
        }

        return 0;
    }

    static int executeFile(const std::string& filepath, const std::vector<std::string>& args = {}) {
        std::ifstream in(filepath);
        if (!in.is_open()) {
            std::cerr << "[Viss VM Error] Cannot open file: " << filepath << "\n";
            return 1;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return executeSource(ss.str(), args);
    }

private:
    static void parseBlock(
        const std::vector<std::string>& lines,
        size_t& idx,
        std::vector<NodePtr>& out_nodes,
        VMContext& ctx
    ) {
        while (idx < lines.size()) {
            std::string line = lines[idx++];
            if (line == "}") return; // End of block

            // Strip trailing semicolons
            if (!line.empty() && line.back() == ';') line.pop_back();
            line = trim(line);
            if (line.empty()) continue;

            // Directives: $import ... -> skip in VM (built-ins always available)
            if (startsWith(line, "$import") || startsWith(line, "$impoer") || startsWith(line, "@use")) {
                continue;
            }

            // Function Definition: !main { or !func name(params) {
            if (line == "!main {" || startsWith(line, "!func main")) {
                auto fn = std::make_shared<UserFunction>();
                fn->name = "main";
                parseBlock(lines, idx, fn->body, ctx);
                ctx.functions["main"] = fn;
                continue;
            }

            if (startsWith(line, "!func ") && endsWith(line, "{")) {
                std::string decl = trim(line.substr(6, line.size() - 7));
                size_t p_open = decl.find('(');
                size_t p_close = decl.find(')');
                if (p_open != std::string::npos && p_close != std::string::npos) {
                    std::string fname = trim(decl.substr(0, p_open));
                    std::string p_str = decl.substr(p_open + 1, p_close - p_open - 1);
                    auto fn = std::make_shared<UserFunction>();
                    fn->name = fname;
                    std::stringstream p_ss(p_str);
                    std::string p_item;
                    while (std::getline(p_ss, p_item, ',')) {
                        p_item = trim(p_item);
                        size_t as_pos = p_item.find(" as ");
                        if (as_pos != std::string::npos) p_item = trim(p_item.substr(0, as_pos));
                        if (p_item.front() == '@') p_item = p_item.substr(1);
                        if (!p_item.empty()) fn->params.push_back(p_item);
                    }
                    parseBlock(lines, idx, fn->body, ctx);
                    ctx.functions[fname] = fn;
                    continue;
                }
            }

            // If Statement: !if (cond) { or ?if (cond) {
            if ((startsWith(line, "!if") || startsWith(line, "?if") || startsWith(line, "if")) && endsWith(line, "{")) {
                auto stmt = std::make_shared<StmtIf>();
                size_t p1 = line.find('(');
                size_t p2 = line.rfind(')');
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    stmt->cond = line.substr(p1 + 1, p2 - p1 - 1);
                } else {
                    stmt->cond = "true";
                }
                parseBlock(lines, idx, stmt->then_branch, ctx);

                // Check for elif / else
                while (idx < lines.size()) {
                    std::string next = trim(lines[idx]);
                    if (startsWith(next, "!elif") || startsWith(next, "?elif") || startsWith(next, "else if")) {
                        idx++;
                        size_t ep1 = next.find('(');
                        size_t ep2 = next.rfind(')');
                        std::string econd = "true";
                        if (ep1 != std::string::npos && ep2 != std::string::npos && ep2 > ep1) {
                            econd = next.substr(ep1 + 1, ep2 - ep1 - 1);
                        }
                        std::vector<NodePtr> e_branch;
                        parseBlock(lines, idx, e_branch, ctx);
                        stmt->elif_branches.push_back({econd, e_branch});
                    } else if (startsWith(next, "!else") || startsWith(next, "?else") || next == "else {") {
                        idx++;
                        parseBlock(lines, idx, stmt->else_branch, ctx);
                        break;
                    } else {
                        break;
                    }
                }
                out_nodes.push_back(stmt);
                continue;
            }

            // While Loop: !while (cond) {
            if ((startsWith(line, "!while") || startsWith(line, "while")) && endsWith(line, "{")) {
                size_t p1 = line.find('(');
                size_t p2 = line.rfind(')');
                std::string cond = "true";
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    cond = line.substr(p1 + 1, p2 - p1 - 1);
                }
                std::vector<NodePtr> b;
                parseBlock(lines, idx, b, ctx);
                out_nodes.push_back(std::make_shared<StmtWhile>(cond, b));
                continue;
            }

            // For Range Loop: !for @i in 0..10 {
            if ((startsWith(line, "!for") || startsWith(line, "for")) && line.find(" in ") != std::string::npos && endsWith(line, "{")) {
                std::string decl = trim(line.substr(line.find(' ') + 1, line.size() - line.find(' ') - 2));
                size_t in_pos = decl.find(" in ");
                std::string vname = trim(decl.substr(0, in_pos));
                if (vname.front() == '@') vname = vname.substr(1);
                std::string rest = trim(decl.substr(in_pos + 4));

                size_t dotdot = rest.find("..");
                if (dotdot != std::string::npos) {
                    std::string s_expr = trim(rest.substr(0, dotdot));
                    std::string e_expr = trim(rest.substr(dotdot + 2));
                    std::vector<NodePtr> b;
                    parseBlock(lines, idx, b, ctx);
                    out_nodes.push_back(std::make_shared<StmtForRange>(vname, s_expr, e_expr, b));
                } else {
                    std::vector<NodePtr> b;
                    parseBlock(lines, idx, b, ctx);
                    out_nodes.push_back(std::make_shared<StmtForIn>(vname, rest, b));
                }
                continue;
            }

            // Loop Repeat: !loop (count) {
            if (startsWith(line, "!loop") && endsWith(line, "{")) {
                size_t p1 = line.find('(');
                size_t p2 = line.rfind(')');
                std::string cnt = "1";
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    cnt = line.substr(p1 + 1, p2 - p1 - 1);
                }
                std::vector<NodePtr> b;
                parseBlock(lines, idx, b, ctx);
                out_nodes.push_back(std::make_shared<StmtLoopRepeat>(cnt, b));
                continue;
            }

            // Buffer Create: &buf create | bytes, 16
            if (startsWith(line, "&") && line.find(" create |") != std::string::npos) {
                size_t sp = line.find(' ');
                std::string bname = line.substr(1, sp - 1);
                size_t pipe = line.find('|');
                std::string spec = trim(line.substr(pipe + 1));
                size_t comma = spec.find(',');
                std::string btype = "bytes";
                std::string sz = "1024";
                if (comma != std::string::npos) {
                    btype = trim(spec.substr(0, comma));
                    sz = trim(spec.substr(comma + 1));
                } else {
                    btype = trim(spec);
                }
                out_nodes.push_back(std::make_shared<StmtBufferCreate>(bname, btype, sz));
                continue;
            }

            // Buffer Write: &buf write u8 255 at 0; or &buf write "hello"
            if (startsWith(line, "&") && line.find(" write ") != std::string::npos) {
                size_t sp = line.find(' ');
                std::string bname = line.substr(1, sp - 1);
                std::string rest = trim(line.substr(line.find(" write ") + 7));
                std::string wtype = "u8";
                std::string val = "";
                std::string off = "";
                size_t at_pos = rest.find(" at ");
                if (at_pos != std::string::npos) {
                    off = trim(rest.substr(at_pos + 4));
                    rest = trim(rest.substr(0, at_pos));
                }
                size_t fsp = rest.find(' ');
                if (fsp != std::string::npos) {
                    std::string possible_type = rest.substr(0, fsp);
                    if (possible_type == "u8" || possible_type == "i8" || possible_type == "int" || possible_type == "str" || possible_type == "string") {
                        wtype = possible_type;
                        val = trim(rest.substr(fsp + 1));
                    } else {
                        val = rest;
                    }
                } else {
                    val = rest;
                }
                out_nodes.push_back(std::make_shared<StmtBufferWrite>(bname, wtype, val, off));
                continue;
            }

            // Subscript Assignment: @arr[idx] = val
            if (startsWith(line, "@") && line.find('[') != std::string::npos && line.find("]=") != std::string::npos) {
                size_t b_open = line.find('[');
                std::string vname = line.substr(1, b_open - 1);
                size_t b_close = line.find(']');
                std::string ie = line.substr(b_open + 1, b_close - b_open - 1);
                size_t eq = line.find('=', b_close);
                std::string ve = trim(line.substr(eq + 1));
                out_nodes.push_back(std::make_shared<StmtSubscriptAssign>(vname, ie, ve));
                continue;
            }

            // Compound Variable Assignment: @x += 1, @x -= 1, etc.
            size_t op_pos = std::string::npos;
            std::string found_op = "";
            for (const std::string& op : {"+=", "-=", "*=", "/="}) {
                size_t p = line.find(op);
                if (p != std::string::npos) {
                    op_pos = p;
                    found_op = op;
                    break;
                }
            }
            if (startsWith(line, "@") && op_pos != std::string::npos) {
                std::string vname = trim(line.substr(1, op_pos - 1));
                std::string ex = trim(line.substr(op_pos + found_op.size()));
                out_nodes.push_back(std::make_shared<StmtAssign>(vname, found_op, ex));
                continue;
            }

            // Generic Variable Assignment: @x = expr [| type]
            if (startsWith(line, "@") && line.find('=') != std::string::npos) {
                size_t eq_pos = line.find('=');
                std::string vname = trim(line.substr(1, eq_pos - 1));
                std::string expr = trim(line.substr(eq_pos + 1));
                size_t pipe_pos = expr.find('|');
                if (pipe_pos != std::string::npos && expr.find('|', pipe_pos + 1) == std::string::npos) {
                    expr = trim(expr.substr(0, pipe_pos));
                }
                out_nodes.push_back(std::make_shared<StmtAssign>(vname, "=", expr));
                continue;
            }

            // Flow control: !return, !break, !continue
            if (startsWith(line, "!return") || startsWith(line, "return")) {
                std::string ret_expr = "";
                size_t sp = line.find(' ');
                if (sp != std::string::npos) ret_expr = trim(line.substr(sp + 1));
                out_nodes.push_back(std::make_shared<StmtReturn>(ret_expr));
                continue;
            }
            if (line == "!break" || line == "break") {
                out_nodes.push_back(std::make_shared<StmtBreak>());
                continue;
            }
            if (line == "!continue" || line == "continue") {
                out_nodes.push_back(std::make_shared<StmtContinue>());
                continue;
            }

            // Default Expression Statement (e.g. io.println(...))
            out_nodes.push_back(std::make_shared<StmtExpr>(line));
        }
    }
};

} // namespace vm
} // namespace viss
