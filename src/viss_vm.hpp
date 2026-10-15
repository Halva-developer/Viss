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
#include <filesystem>
#include "../libs/std/audio.hpp"
#include "../libs/std/media.hpp"
#include "../libs/std/gui.hpp"
#include "../libs/std/net.hpp"
#include "../libs/std/db.hpp"
#include "../libs/std/json.hpp"

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

inline int64_t vm_utf8_len(const std::string& s) {
    int64_t count = 0;
    for (char c : s) {
        if (((unsigned char)c & 0xC0) != 0x80) count++;
    }
    return count;
}

inline std::string vm_utf8_substr(const std::string& s, int64_t start, int64_t length = -1) {
    if (start < 0) return "";
    size_t b_start = s.size(), b_end = s.size();
    int64_t cp = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {
            if (cp == start) b_start = i;
            if (length >= 0 && cp == start + length) { b_end = i; break; }
            cp++;
        }
    }
    if (b_start >= s.size()) {
        if (cp == start && length == 0) return "";
        return "";
    }
    if (length < 0) return s.substr(b_start);
    return s.substr(b_start, b_end - b_start);
}

inline uint32_t vm_decode_utf8(const std::string& s, size_t& i) {
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

inline void vm_encode_utf8(uint32_t cp, std::string& out) {
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

inline uint32_t vm_cp_upper(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - ('a' - 'A');
    if (cp >= 0x0430 && cp <= 0x044F) return cp - 0x20;
    if (cp == 0x0451) return 0x0401; // ё -> Ё
    if (cp == 0x0454) return 0x0404; // є -> Є
    if (cp == 0x0456) return 0x0406; // і -> І
    if (cp == 0x0457) return 0x0407; // ї -> Ї
    if (cp == 0x045E) return 0x040E; // ў -> Ў
    return cp;
}

inline uint32_t vm_cp_lower(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + ('a' - 'A');
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;
    if (cp == 0x0401) return 0x0451; // Ё -> ё
    if (cp == 0x0404) return 0x0454; // Є -> є
    if (cp == 0x0406) return 0x0456; // І -> і
    if (cp == 0x0407) return 0x0457; // Ї -> ї
    if (cp == 0x040E) return 0x045E; // Ў -> ў
    return cp;
}

inline std::string vm_utf8_upper(const std::string& s) {
    std::string res;
    res.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = vm_decode_utf8(s, i);
        vm_encode_utf8(vm_cp_upper(cp), res);
    }
    return res;
}

inline std::string vm_utf8_lower(const std::string& s) {
    std::string res;
    res.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = vm_decode_utf8(s, i);
        vm_encode_utf8(vm_cp_lower(cp), res);
    }
    return res;
}

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

    std::string type_name() const {
        switch (type) {
            case VMType::Null: return "Null";
            case VMType::Int: return "Int";
            case VMType::Dec: return "Dec";
            case VMType::Str: return "Str";
            case VMType::Bool: return "Bool";
            case VMType::List: return "List";
            case VMType::Bytes: return "Bytes";
            case VMType::Map: return "Map";
            default: return "Unknown";
        }
    }

    std::string to_repr() const {
        if (type == VMType::Str) return "\"" + s_val + "\"";
        if (type == VMType::List) {
            std::string out = "[";
            for (size_t i = 0; i < list_val.size(); ++i) {
                out += list_val[i].to_repr();
                if (i + 1 < list_val.size()) out += ", ";
            }
            out += "]";
            return out;
        }
        return to_string();
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
        if (type == VMType::Str) return vm_utf8_len(s_val);
        if (type == VMType::List) return (int64_t)list_val.size();
        if (type == VMType::Bytes) return (int64_t)bytes_val.size();
        if (type == VMType::Map) return (int64_t)map_val.size();
        return 0;
    }
};

inline VMVal jsonToVMVal(const viss::json::JsonValue& jv) {
    switch (jv.type) {
        case viss::json::JsonType::Null: return VMVal();
        case viss::json::JsonType::Bool: return VMVal((bool)jv.bool_val);
        case viss::json::JsonType::Int: return VMVal((int64_t)jv.int_val);
        case viss::json::JsonType::Number: return VMVal((double)jv.num_val);
        case viss::json::JsonType::String: return VMVal(std::string(jv.str_val));
        case viss::json::JsonType::Array: {
            std::vector<VMVal> list;
            for (const auto& item : jv.arr_val) list.push_back(jsonToVMVal(item));
            return VMVal(list);
        }
        case viss::json::JsonType::Object: {
            VMVal m;
            m.type = VMType::Map;
            for (const auto& kv : jv.obj_val) {
                m.map_val[std::string(kv.first)] = jsonToVMVal(kv.second);
            }
            return m;
        }
    }
    return VMVal();
}

inline viss::json::JsonValue vmValToJson(const VMVal& v) {
    switch (v.type) {
        case VMType::Null: return viss::json::JsonValue();
        case VMType::Bool: return viss::json::JsonValue((Bool)v.b_val);
        case VMType::Int: return viss::json::JsonValue((Int)v.i_val);
        case VMType::Dec: return viss::json::JsonValue((Dec)v.d_val);
        case VMType::Str: return viss::json::JsonValue(v.s_val);
        case VMType::List: {
            auto arr = viss::json::JsonValue::array();
            for (const auto& item : v.list_val) arr.push(vmValToJson(item));
            return arr;
        }
        case VMType::Map: {
            auto obj = viss::json::JsonValue::object();
            for (const auto& kv : v.map_val) obj[kv.first] = vmValToJson(kv.second);
            return obj;
        }
        default: return viss::json::JsonValue(v.to_string());
    }
}

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

inline VMVal op_contains(const VMVal& a, const VMVal& b) {
    if (a.type == VMType::List) {
        for (const auto& item : a.list_val) {
            if (op_eq(item, b).to_bool()) return VMVal(true);
        }
        return VMVal(false);
    }
    if (a.type == VMType::Map) {
        return VMVal(a.map_val.find(b.to_string()) != a.map_val.end());
    }
    if (a.type == VMType::Str || b.type == VMType::Str) {
        std::string s_a = a.to_string();
        std::string s_b = b.to_string();
        std::string lower_a = s_a;
        std::string lower_b = s_b;
        for (char& c : lower_a) c = (char)std::tolower((unsigned char)c);
        for (char& c : lower_b) c = (char)std::tolower((unsigned char)c);
        return VMVal(lower_a.find(lower_b) != std::string::npos);
    }
    return VMVal(false);
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
    std::unordered_map<std::string, std::string> module_aliases;
    std::vector<std::string> cli_args;
    FlowSignal signal = FlowSignal::None;
    VMVal return_val;
    bool has_error = false;
    std::string error_msg = "";
    int exit_code = 0;

    VMContext() {
        scopes.emplace_back(); // Global scope
        init_aliases();
    }

    void init_aliases() {
        module_aliases = {
            {"io", "io"}, {"iostream", "io"},
            {"math", "math"},
            {"rt", "rt"}, {"retrotech", "rt"},
            {"sys", "sys"}, {"system", "sys"},
            {"str", "str"}, {"string", "str"},
            {"time", "time"},
            {"audio", "audio"}, {"snd", "audio"}, {"sound", "audio"},
            {"media", "media"}, {"tag", "media"},
            {"gui", "gui"}, {"ui", "gui"}, {"window", "gui"}, {"win", "gui"},
            {"net", "net"},
            {"db", "db"},
            {"chan", "chan"},
            {"fs", "fs"},
            {"mask", "mask"}, {"bytemask", "mask"}, {"colormask", "mask"},
            {"crypto", "crypto"},
            {"json", "json"},
            {"async", "async"}, {"asyncIO", "async"},
            {"env", "env"},
            {"collections", "collections"}
        };
    }

    void push_scope() { scopes.emplace_back(); }
    void pop_scope() { if (scopes.size() > 1) scopes.pop_back(); }

    void set_var(const std::string& name, const VMVal& val) {
        if (scopes.empty()) scopes.emplace_back();
        if (scopes.size() > 1) {
            for (size_t s = scopes.size(); s > 1; --s) {
                auto fit = scopes[s - 1].find(name);
                if (fit != scopes[s - 1].end()) {
                    fit->second = val;
                    return;
                }
            }
            auto git = scopes.front().find(name);
            if (git != scopes.front().end()) {
                git->second = val;
                return;
            }
            scopes.back()[name] = val;
            return;
        }
        scopes.front()[name] = val;
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

    static std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
        if (from.empty()) return str;
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return str;
    }

    static bool startsWith(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
    }

    static bool endsWith(const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
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
        if (ctx.has_error) return VMVal();
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

        // Pipeline Operator: |>
        paren = brace = bracket = 0; in_str = false;
        for (int i = (int)expr.size() - 1; i >= 1; --i) {
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
                    if (expr[i - 1] == '|' && expr[i] == '>') {
                        std::string left_s = trim(expr.substr(0, i - 1));
                        std::string right_s = trim(expr.substr(i + 1));
                        VMVal left_val = evaluate(left_s, ctx);
                        return evaluatePipelineCall(left_val, right_s, ctx);
                    }
                }
            }
        }

        // Comparisons: ==, !=, <=, >=, <, >, contains, conatins
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

                    auto matchWordOp = [&](int idx, const std::string& word) -> bool {
                        size_t wlen = word.size();
                        if (idx + 1 < (int)wlen) return false;
                        int start = idx - (int)wlen + 1;
                        if (expr.substr(start, wlen) != word) return false;
                        if (start > 0 && (std::isalnum((unsigned char)expr[start - 1]) || expr[start - 1] == '_' || expr[start - 1] == '@' || expr[start - 1] == '.')) return false;
                        if (idx + 1 < (int)expr.size() && (std::isalnum((unsigned char)expr[idx + 1]) || expr[idx + 1] == '_' || expr[idx + 1] == '@' || expr[idx + 1] == '(')) return false;
                        return true;
                    };
                    if (matchWordOp(i, "contains")) {
                        int start = i - 8 + 1;
                        return op_contains(evaluate(expr.substr(0, start), ctx), evaluate(expr.substr(i + 1), ctx));
                    }
                    if (matchWordOp(i, "conatins")) {
                        int start = i - 8 + 1;
                        return op_contains(evaluate(expr.substr(0, start), ctx), evaluate(expr.substr(i + 1), ctx));
                    }
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
                    if (c == '-' && i > 0) {
                        int prev_idx = i - 1;
                        while (prev_idx >= 0 && (expr[prev_idx] == ' ' || expr[prev_idx] == '\t')) prev_idx--;
                        if (prev_idx >= 0) {
                            char prev_c = expr[prev_idx];
                            if (prev_c != '+' && prev_c != '-' && prev_c != '*' && prev_c != '/' &&
                                prev_c != '%' && prev_c != '(' && prev_c != '[' && prev_c != '{' &&
                                prev_c != '=' && prev_c != '<' && prev_c != '>' && prev_c != '!' &&
                                prev_c != ',' && prev_c != ':') {
                                return op_sub(evaluate(expr.substr(0, i), ctx), evaluate(expr.substr(i+1), ctx));
                            }
                        }
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
                    if (c == '/') {
                        auto right = evaluate(expr.substr(i+1), ctx);
                        if (right.to_dec() == 0.0) {
                            std::cerr << "RuntimeError: Division by zero\n";
                            ctx.has_error = true;
                            ctx.exit_code = 1;
                            return VMVal(0);
                        }
                        return op_div(evaluate(expr.substr(0, i), ctx), right);
                    }
                    if (c == '%') {
                        auto right = evaluate(expr.substr(i+1), ctx);
                        if (right.to_int() == 0) {
                            std::cerr << "RuntimeError: Modulo by zero\n";
                            ctx.has_error = true;
                            ctx.exit_code = 1;
                            return VMVal(0);
                        }
                        return op_mod(evaluate(expr.substr(0, i), ctx), right);
                    }
                }
            }
        }

        // Unary NOT or Action Call: !expr
        if (expr.front() == '!' && expr.size() > 1 && expr[1] != '!') {
            std::string sub = trim(expr.substr(1));
            size_t p_o = sub.find('(');
            std::string sub_fn = (p_o != std::string::npos && sub.back() == ')') ? trim(sub.substr(0, p_o)) : sub;
            while (!sub_fn.empty() && (sub_fn.front() == '!' || sub_fn.front() == '@')) sub_fn = sub_fn.substr(1);
            if (!sub_fn.empty() && ctx.functions.count(sub_fn)) {
                return evaluate(sub, ctx);
            }
            return VMVal(!evaluate(sub, ctx).to_bool());
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

        // Map Literals: {"a": 1, "b": 2} or {a: 1, b: 2}
        if (expr.front() == '{' && expr.back() == '}') {
            std::string inner = trim(expr.substr(1, expr.size() - 2));
            VMVal map_res;
            map_res.type = VMType::Map;
            if (inner.empty()) return map_res;
            auto items = splitArgs(inner);
            for (const auto& itm : items) {
                size_t col_pos = std::string::npos;
                int p = 0, br = 0, bc = 0; bool ins = false; char q = 0;
                for (size_t ci = 0; ci < itm.size(); ++ci) {
                    char ch = itm[ci];
                    if (ins) {
                        if (ch == q && (ci == 0 || itm[ci-1] != '\\')) ins = false;
                    } else {
                        if (ch == '"' || ch == '\'') { ins = true; q = ch; }
                        else if (ch == '(') p++; else if (ch == ')') p--;
                        else if (ch == '[') br++; else if (ch == ']') br--;
                        else if (ch == '{') bc++; else if (ch == '}') bc--;
                        else if (p == 0 && br == 0 && bc == 0 && ch == ':') {
                            col_pos = ci;
                            break;
                        }
                    }
                }
                if (col_pos != std::string::npos) {
                    std::string k_str = trim(itm.substr(0, col_pos));
                    std::string v_str = trim(itm.substr(col_pos + 1));
                    std::string k_val;
                    if (!k_str.empty() && ((k_str.front() == '"' && k_str.back() == '"') || (k_str.front() == '\'' && k_str.back() == '\''))) {
                        k_val = k_str.substr(1, k_str.size() - 2);
                    } else {
                        k_val = k_str;
                    }
                    map_res.map_val[k_val] = evaluate(v_str, ctx);
                }
            }
            return map_res;
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
            return VMVal((int64_t)std::stoll(expr));
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

            // Module Calls: lookup via registered module aliases
            auto mod_it = ctx.module_aliases.find(obj_expr);
            if (mod_it != ctx.module_aliases.end()) {
                return callModule(mod_it->second, member_expr, ctx);
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

            // Methods on Strings
            if (target.type == VMType::Str) {
                if (mname == "upper") {
                    return VMVal(vm_utf8_upper(target.s_val));
                }
                if (mname == "lower") {
                    return VMVal(vm_utf8_lower(target.s_val));
                }
                if (mname == "trim") {
                    return VMVal(trim(target.s_val));
                }
                if (mname == "split") {
                    std::string delim = margs.empty() ? " " : evaluate(margs, ctx).to_string();
                    std::string s = target.s_val;
                    std::vector<VMVal> tokens;
                    if (delim.empty()) {
                        for (char c : s) tokens.push_back(VMVal(std::string(1, c)));
                    } else {
                        size_t prev = 0, pos = 0;
                        do {
                            pos = s.find(delim, prev);
                            if (pos == std::string::npos) pos = s.length();
                            tokens.push_back(VMVal(s.substr(prev, pos - prev)));
                            prev = pos + delim.length();
                        } while (pos < s.length() && prev < s.length());
                    }
                    return VMVal(tokens);
                }
                if (mname == "replace") {
                    auto r_args = splitArgs(margs);
                    if (r_args.size() >= 2) {
                        std::string from = evaluate(r_args[0], ctx).to_string();
                        std::string to = evaluate(r_args[1], ctx).to_string();
                        return VMVal(replaceAll(target.s_val, from, to));
                    }
                    return target;
                }
                if (mname == "starts_with") {
                    std::string p = evaluate(margs, ctx).to_string();
                    return VMVal(target.s_val.size() >= p.size() && target.s_val.compare(0, p.size(), p) == 0);
                }
                if (mname == "ends_with") {
                    std::string suff = evaluate(margs, ctx).to_string();
                    return VMVal(target.s_val.size() >= suff.size() && target.s_val.compare(target.s_val.size() - suff.size(), suff.size(), suff) == 0);
                }
                if (mname == "contains") {
                    std::string sub = evaluate(margs, ctx).to_string();
                    return VMVal(target.s_val.find(sub) != std::string::npos);
                }
                if (mname == "find" || mname == "index_of") {
                    std::string sub = evaluate(margs, ctx).to_string();
                    auto pos = target.s_val.find(sub);
                    return VMVal(pos == std::string::npos ? (int64_t)-1 : (int64_t)pos);
                }
                if (mname == "substr" || mname == "slice") {
                    auto s_args = splitArgs(margs);
                    if (!s_args.empty()) {
                        int64_t start = evaluate(s_args[0], ctx).to_int();
                        if (start < 0) return VMVal("");
                        if (s_args.size() > 1) {
                            int64_t len = evaluate(s_args[1], ctx).to_int();
                            return VMVal(vm_utf8_substr(target.s_val, start, len));
                        }
                        return VMVal(vm_utf8_substr(target.s_val, start));
                    }
                }
            }

            // Methods on Maps
            if (target.type == VMType::Map) {
                if (mname == "keys") {
                    std::vector<VMVal> klist;
                    for (const auto& kv : target.map_val) klist.push_back(VMVal(kv.first));
                    return VMVal(klist);
                }
                if (mname == "values") {
                    std::vector<VMVal> vlist;
                    for (const auto& kv : target.map_val) vlist.push_back(kv.second);
                    return VMVal(vlist);
                }
                if (mname == "has" || mname == "contains") {
                    std::string k = evaluate(margs, ctx).to_string();
                    return VMVal(target.map_val.find(k) != target.map_val.end());
                }
                if (mname == "get") {
                    auto g_args = splitArgs(margs);
                    if (!g_args.empty()) {
                        std::string k = evaluate(g_args[0], ctx).to_string();
                        auto it = target.map_val.find(k);
                        if (it != target.map_val.end()) return it->second;
                        if (g_args.size() > 1) return evaluate(g_args[1], ctx);
                    }
                    return VMVal();
                }
            }

            // Methods on Lists
            if (target.type == VMType::List) {
                if (mname == "join") {
                    std::string delim = margs.empty() ? "" : evaluate(margs, ctx).to_string();
                    std::string res;
                    for (size_t i = 0; i < target.list_val.size(); ++i) {
                        res += target.list_val[i].to_string();
                        if (i + 1 < target.list_val.size()) res += delim;
                    }
                    return VMVal(res);
                }
                if (mname == "contains") {
                    VMVal to_find = evaluate(margs, ctx);
                    for (const auto& item : target.list_val) {
                        if (op_eq(item, to_find).to_bool()) return VMVal(true);
                    }
                    return VMVal(false);
                }
                if (mname == "reverse") {
                    auto rev = target.list_val;
                    std::reverse(rev.begin(), rev.end());
                    return VMVal(rev);
                }
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
            while (!fname.empty() && (fname.front() == '!' || fname.front() == '@' || fname.front() == '$')) fname = fname.substr(1);
            std::string args_str = expr.substr(p_open + 1, expr.size() - p_open - 2);
            auto args_raw = splitArgs(args_str);
            std::vector<VMVal> args;
            for (const auto& a : args_raw) args.push_back(evaluate(a, ctx));

            // Builtin prints & conversions
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
                return VMVal();
            }
            if (fname == "len" && !args.empty()) return VMVal(args[0].len());
            if (fname == "int" && !args.empty()) return VMVal(args[0].to_int());
            if (fname == "dec" && !args.empty()) return VMVal(args[0].to_dec());
            if (fname == "str" && !args.empty()) return VMVal(args[0].to_string());
            if (fname == "bool" && !args.empty()) return VMVal(args[0].to_bool());

            auto fit = ctx.functions.find(fname);
            if (fit == ctx.functions.end()) {
                for (auto it = ctx.functions.begin(); it != ctx.functions.end(); ++it) {
                    if (it->first.size() == fname.size()) {
                        bool eq = true;
                        for (size_t ci = 0; ci < fname.size(); ++ci) {
                            if (std::tolower((unsigned char)it->first[ci]) != std::tolower((unsigned char)fname[ci])) {
                                eq = false; break;
                            }
                        }
                        if (eq) { fit = it; break; }
                    }
                }
            }

            if (fit != ctx.functions.end()) {
                auto fn = fit->second;
                auto saved_scopes = std::move(ctx.scopes);
                ctx.scopes.clear();
                ctx.scopes.push_back(saved_scopes.front()); // global scope
                ctx.scopes.emplace_back(); // isolated local scope

                for (size_t pi = 0; pi < fn->params.size(); ++pi) {
                    VMVal arg_v = (pi < args.size()) ? args[pi] : VMVal(0);
                    ctx.scopes.back()[fn->params[pi]] = arg_v;
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
                auto updated_global = ctx.scopes.front();
                ctx.scopes = std::move(saved_scopes);
                ctx.scopes.front() = updated_global;
                return ret_v;
            }

            // Function not found
            std::cerr << "RuntimeError: Undefined function '" << fname << "'\n";
            ctx.has_error = true;
            ctx.exit_code = 1;
            return VMVal();
        }

        // Variable Lookup: @var or var or &buf or 0-arg function call
        std::string vname = expr;
        while (!vname.empty() && (vname.front() == '@' || vname.front() == '&' || vname.front() == '!' || vname.front() == '$')) vname = vname.substr(1);
        if (ctx.has_var(vname)) {
            return ctx.get_var(vname);
        }
        auto fn_it = ctx.functions.find(vname);
        if (fn_it == ctx.functions.end()) {
            for (auto it = ctx.functions.begin(); it != ctx.functions.end(); ++it) {
                if (it->first.size() == vname.size()) {
                    bool eq = true;
                    for (size_t ci = 0; ci < vname.size(); ++ci) {
                        if (std::tolower((unsigned char)it->first[ci]) != std::tolower((unsigned char)vname[ci])) {
                            eq = false; break;
                        }
                    }
                    if (eq) { fn_it = it; break; }
                }
            }
        }
        if (fn_it != ctx.functions.end() && fn_it->second->params.empty()) {
            auto fn = fn_it->second;
            auto saved_scopes = std::move(ctx.scopes);
            ctx.scopes.clear();
            ctx.scopes.push_back(saved_scopes.front());
            ctx.scopes.emplace_back();
            VMVal ret_v;
            for (const auto& node : fn->body) {
                ret_v = node->execute(ctx);
                if (ctx.signal == FlowSignal::Return) {
                    ret_v = ctx.return_val;
                    ctx.signal = FlowSignal::None;
                    break;
                }
            }
            auto updated_global = ctx.scopes.front();
            ctx.scopes = std::move(saved_scopes);
            ctx.scopes.front() = updated_global;
            return ret_v;
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
        if (mod == "io" || mod == "iostream") {
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
        if (mod == "sys" || mod == "system") {
            if (fname == "args") {
                std::vector<VMVal> res;
                for (const auto& a : ctx.cli_args) res.push_back(VMVal(a));
                return VMVal(res);
            }
            if (fname == "arg" && !args.empty()) {
                int64_t idx = args[0].to_int();
                if (idx >= 0 && idx < (int64_t)ctx.cli_args.size()) {
                    return VMVal(ctx.cli_args[idx]);
                }
                return VMVal();
            }
            if (fname == "argc") {
                return VMVal((int64_t)ctx.cli_args.size());
            }
            if (fname == "exec" && !args.empty()) {
                return VMVal(std::string(viss::sys::exec(args[0].to_string())));
            }
            if ((fname == "system" || fname == "command" || fname == "run") && !args.empty()) {
                return VMVal((int64_t)viss::sys::system(args[0].to_string()));
            }
            if (fname == "env" && !args.empty()) {
                return VMVal(std::string(viss::sys::env(args[0].to_string())));
            }
            if (fname == "set_env" && args.size() >= 2) {
                #ifdef _WIN32
                _putenv_s(args[0].to_string().c_str(), args[1].to_string().c_str());
                #else
                setenv(args[0].to_string().c_str(), args[1].to_string().c_str(), 1);
                #endif
                return VMVal(true);
            }
            if (fname == "cwd" || fname == "pwd") {
                return VMVal(std::filesystem::current_path().string());
            }
            if (fname == "last_exit_code") {
                return VMVal((int64_t)viss::sys::last_exit_code());
            }
            if (fname == "cpu_count") {
                return VMVal((int64_t)viss::sys::cpu_count());
            }
            if (fname == "sleep" && !args.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(args[0].to_int()));
                return VMVal();
            }
            if (fname == "platform") {
                #ifdef _WIN32
                return VMVal("windows");
                #elif defined(__APPLE__)
                return VMVal("darwin");
                #else
                return VMVal("linux");
                #endif
            }
            if (fname == "os") {
                #ifdef _WIN32
                return VMVal("Windows");
                #elif defined(__APPLE__)
                return VMVal("macOS");
                #else
                return VMVal("Linux");
                #endif
            }
            if (fname == "distro") {
                #ifdef _WIN32
                return VMVal("Windows");
                #elif defined(__APPLE__)
                return VMVal("macOS");
                #else
                std::ifstream f("/etc/os-release");
                if (f.is_open()) {
                    std::string line;
                    while (std::getline(f, line)) {
                        if (line.rfind("PRETTY_NAME=", 0) == 0) {
                            std::string val = line.substr(12);
                            if (!val.empty() && val.front() == '"') val = val.substr(1);
                            if (!val.empty() && val.back() == '"') val.pop_back();
                            return VMVal(val);
                        }
                    }
                }
                return VMVal("Linux");
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
        if (mod == "str" || mod == "string") {
            if (fname == "len" && !args.empty()) return VMVal(args[0].len());
            if (fname == "size" && !args.empty()) return VMVal(args[0].len());
            if (fname == "upper" && !args.empty()) {
                std::string s = args[0].to_string();
                for (char& c : s) c = (char)std::toupper((unsigned char)c);
                return VMVal(s);
            }
            if (fname == "lower" && !args.empty()) {
                std::string s = args[0].to_string();
                for (char& c : s) c = (char)std::tolower((unsigned char)c);
                return VMVal(s);
            }
            if (fname == "trim" && !args.empty()) {
                return VMVal(trim(args[0].to_string()));
            }
            if (fname == "split" && args.size() >= 2) {
                std::string s = args[0].to_string();
                std::string delim = args[1].to_string();
                std::vector<VMVal> tokens;
                if (delim.empty()) {
                    for (char c : s) tokens.push_back(VMVal(std::string(1, c)));
                } else {
                    size_t prev = 0, pos = 0;
                    do {
                        pos = s.find(delim, prev);
                        if (pos == std::string::npos) pos = s.length();
                        tokens.push_back(VMVal(s.substr(prev, pos - prev)));
                        prev = pos + delim.length();
                    } while (pos < s.length() && prev < s.length());
                }
                return VMVal(tokens);
            }
            if (fname == "join" && args.size() >= 2) {
                std::string delim = args[1].to_string();
                std::string res;
                if (args[0].type == VMType::List) {
                    for (size_t i = 0; i < args[0].list_val.size(); ++i) {
                        res += args[0].list_val[i].to_string();
                        if (i + 1 < args[0].list_val.size()) res += delim;
                    }
                } else {
                    res = args[0].to_string();
                }
                return VMVal(res);
            }
            if (fname == "replace" && args.size() >= 3) {
                return VMVal(replaceAll(args[0].to_string(), args[1].to_string(), args[2].to_string()));
            }
            if (fname == "contains" && args.size() >= 2) {
                return VMVal(args[0].to_string().find(args[1].to_string()) != std::string::npos);
            }
            if (fname == "starts_with" && args.size() >= 2) {
                return VMVal(startsWith(args[0].to_string(), args[1].to_string()));
            }
            if (fname == "ends_with" && args.size() >= 2) {
                return VMVal(endsWith(args[0].to_string(), args[1].to_string()));
            }
            if (fname == "find" && args.size() >= 2) {
                auto pos = args[0].to_string().find(args[1].to_string());
                return VMVal(pos == std::string::npos ? (int64_t)-1 : (int64_t)pos);
            }
            if (fname == "sub" || fname == "substr" || fname == "slice") {
                if (args.size() >= 2) {
                    std::string s = args[0].to_string();
                    int64_t start = args[1].to_int();
                    if (start < 0 || (size_t)start >= s.size()) return VMVal("");
                    if (args.size() >= 3) {
                        int64_t len = args[2].to_int();
                        return VMVal(s.substr(start, len));
                    }
                    return VMVal(s.substr(start));
                }
            }
            if (fname == "repeat" && args.size() >= 2) {
                std::string s = args[0].to_string();
                int64_t cnt = args[1].to_int();
                std::string res;
                for (int64_t i = 0; i < cnt; ++i) res += s;
                return VMVal(res);
            }
            if (fname == "lines" && !args.empty()) {
                std::vector<VMVal> lines;
                std::stringstream ss(args[0].to_string());
                std::string line;
                while (std::getline(ss, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    lines.push_back(VMVal(line));
                }
                return VMVal(lines);
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

        // FS Module
        if (mod == "fs") {
            namespace sfs = std::filesystem;
            if (fname == "exists" && !args.empty()) {
                std::error_code ec;
                return VMVal(sfs::exists(args[0].to_string(), ec));
            }
            if (fname == "is_file" && !args.empty()) {
                std::error_code ec;
                return VMVal(sfs::is_regular_file(args[0].to_string(), ec));
            }
            if (fname == "is_dir" && !args.empty()) {
                std::error_code ec;
                return VMVal(sfs::is_directory(args[0].to_string(), ec));
            }
            if (fname == "read" && !args.empty()) {
                std::string path = args[0].to_string();
                std::ifstream in(path);
                if (!in.is_open()) {
                    std::cerr << "IOError: Cannot open file '" << path << "'\n";
                    ctx.has_error = true;
                    ctx.exit_code = 1;
                    return VMVal("");
                }
                std::stringstream ss; ss << in.rdbuf();
                return VMVal(ss.str());
            }
            if ((fname == "read_lines" || fname == "lines") && !args.empty()) {
                std::ifstream in(args[0].to_string());
                std::vector<VMVal> lines;
                if (in.is_open()) {
                    std::string line;
                    while (std::getline(in, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        lines.push_back(VMVal(line));
                    }
                }
                return VMVal(lines);
            }
            if (fname == "write" && args.size() >= 2) {
                std::ofstream out(args[0].to_string());
                if (!out.is_open()) return VMVal(false);
                out << args[1].to_string();
                return VMVal(true);
            }
            if (fname == "write_lines" && args.size() >= 2) {
                std::ofstream out(args[0].to_string());
                if (!out.is_open()) return VMVal(false);
                if (args[1].type == VMType::List) {
                    for (const auto& item : args[1].list_val) out << item.to_string() << "\n";
                } else {
                    out << args[1].to_string() << "\n";
                }
                return VMVal(true);
            }
            if (fname == "append" && args.size() >= 2) {
                std::ofstream out(args[0].to_string(), std::ios::app);
                if (!out.is_open()) return VMVal(false);
                out << args[1].to_string();
                return VMVal(true);
            }
            if ((fname == "list" || fname == "list_dir" || fname == "list_files") && !args.empty()) {
                std::vector<VMVal> items;
                std::string target_dir = args[0].to_string();
                try {
                    for (const auto& entry : sfs::directory_iterator(target_dir)) {
                        items.push_back(VMVal(entry.path().filename().string()));
                    }
                } catch (...) {}
                return VMVal(items);
            }
            if (fname == "list_paths" && !args.empty()) {
                std::vector<VMVal> items;
                std::string target_dir = args[0].to_string();
                try {
                    for (const auto& entry : sfs::directory_iterator(target_dir)) {
                        items.push_back(VMVal(entry.path().string()));
                    }
                } catch (...) {}
                return VMVal(items);
            }
            if ((fname == "mkdir" || fname == "mkdir_p") && !args.empty()) {
                std::error_code ec;
                sfs::create_directories(args[0].to_string(), ec);
                return VMVal(!ec);
            }
            if ((fname == "remove" || fname == "delete") && !args.empty()) {
                std::error_code ec;
                return VMVal(sfs::remove(args[0].to_string(), ec));
            }
            if (fname == "remove_all" && !args.empty()) {
                std::error_code ec;
                return VMVal((int64_t)sfs::remove_all(args[0].to_string(), ec));
            }
            if (fname == "copy" && args.size() >= 2) {
                std::error_code ec;
                return VMVal(sfs::copy_file(args[0].to_string(), args[1].to_string(), sfs::copy_options::overwrite_existing, ec));
            }
            if ((fname == "move" || fname == "rename") && args.size() >= 2) {
                std::error_code ec;
                sfs::rename(args[0].to_string(), args[1].to_string(), ec);
                return VMVal(!ec);
            }
            if (fname == "size" && !args.empty()) {
                std::error_code ec;
                auto sz = sfs::file_size(args[0].to_string(), ec);
                return VMVal(ec ? 0 : (int64_t)sz);
            }
            if (fname == "ext" || fname == "extension") {
                if (args.empty()) return VMVal("");
                return VMVal(sfs::path(args[0].to_string()).extension().string());
            }
            if (fname == "stem") {
                if (args.empty()) return VMVal("");
                return VMVal(sfs::path(args[0].to_string()).stem().string());
            }
            if (fname == "filename" || fname == "basename") {
                if (args.empty()) return VMVal("");
                return VMVal(sfs::path(args[0].to_string()).filename().string());
            }
            if (fname == "parent" || fname == "dirname") {
                if (args.empty()) return VMVal("");
                return VMVal(sfs::path(args[0].to_string()).parent_path().string());
            }
            if (fname == "abs_path" || fname == "absolute") {
                if (args.empty()) return VMVal("");
                std::error_code ec;
                auto p = sfs::absolute(args[0].to_string(), ec);
                return VMVal(p.string());
            }
        }

        // JSON Module
        if (mod == "json") {
            if (fname == "parse" && !args.empty()) {
                std::string err;
                auto jv = viss::json::parse(args[0].to_string(), err);
                if (!err.empty()) {
                    std::cerr << "JSONError: " << err << "\n";
                    ctx.has_error = true;
                    ctx.exit_code = 1;
                }
                return jsonToVMVal(jv);
            }
            if (fname == "stringify" && !args.empty()) {
                bool pretty = args.size() > 1 ? args[1].to_bool() : false;
                auto jv = vmValToJson(args[0]);
                return VMVal(std::string(viss::json::stringify(jv, pretty)));
            }
            if (fname == "get" && args.size() >= 2) {
                if (args[0].type == VMType::Map) {
                    auto it = args[0].map_val.find(args[1].to_string());
                    if (it != args[0].map_val.end()) return it->second;
                    return VMVal();
                } else {
                    auto jv = viss::json::parse(args[0].to_string());
                    return jsonToVMVal(jv.get(args[1].to_string()));
                }
            }
            if (fname == "has" && args.size() >= 2) {
                if (args[0].type == VMType::Map) {
                    return VMVal(args[0].map_val.find(args[1].to_string()) != args[0].map_val.end());
                } else {
                    auto jv = viss::json::parse(args[0].to_string());
                    return VMVal((bool)jv.has(args[1].to_string()));
                }
            }
        }

        // NET Module
        if (mod == "net") {
            if (fname == "http_get" && !args.empty()) return VMVal(viss::net::http_get(args[0].to_string()));
            if (fname == "http_post" && args.size() >= 2) return VMVal(viss::net::http_post(args[0].to_string(), args[1].to_string()));
            if (fname == "download" && args.size() >= 2) return VMVal(viss::net::download_file(args[0].to_string(), args[1].to_string()));
            if (fname == "url_encode" && !args.empty()) return VMVal(viss::net::url_encode(args[0].to_string()));
            if (fname == "url_decode" && !args.empty()) return VMVal(viss::net::url_decode(args[0].to_string()));
        }

        // DB Module
        if (mod == "db") {
            if (fname == "open") {
                std::string path = !args.empty() ? args[0].to_string() : ":memory:";
                return VMVal(path);
            }
            if (fname == "exec" && args.size() >= 2) {
                auto db_h = viss::db::open(args[0].to_string());
                int res = viss::db::exec(db_h, args[1].to_string());
                return VMVal((int64_t)res);
            }
            if (fname == "query_json" && args.size() >= 2) {
                auto db_h = viss::db::open(args[0].to_string());
                std::string j = viss::db::query_json(db_h, args[1].to_string());
                return VMVal(j);
            }
            if (fname == "count" && args.size() >= 2) {
                auto db_h = viss::db::open(args[0].to_string());
                return VMVal((int64_t)viss::db::count(db_h, args[1].to_string()));
            }
            if (fname == "close") {
                return VMVal();
            }
        }

        // CHAN Module
        if (mod == "chan") {
            size_t t_pos = fname.find('<');
            std::string base_fn = (t_pos != std::string::npos) ? fname.substr(0, t_pos) : fname;
            if (base_fn == "create") {
                return VMVal("chan_handle");
            }
            if (base_fn == "send") {
                return VMVal();
            }
            if (base_fn == "recv") {
                return VMVal("chan_msg");
            }
            if (base_fn == "len") {
                return VMVal((int64_t)0);
            }
            if (base_fn == "close") {
                return VMVal();
            }
            if (base_fn == "is_closed") {
                return VMVal(true);
            }
        }

        return VMVal();
    }

    static VMVal evaluatePipelineCall(const VMVal& val, const std::string& step_raw, VMContext& ctx) {
        std::string step = trim(step_raw);
        if (step.empty()) return val;

        size_t p_open = step.find('(');
        std::string fname = step;
        std::vector<std::string> extra_args;
        if (p_open != std::string::npos && step.back() == ')') {
            fname = trim(step.substr(0, p_open));
            std::string args_str = step.substr(p_open + 1, step.size() - p_open - 2);
            extra_args = splitArgs(args_str);
        }

        while (!fname.empty() && (fname.front() == '!' || fname.front() == '@' || fname.front() == '$')) {
            fname = fname.substr(1);
        }

        // Module calls: mod.func
        size_t dot_p = fname.find('.');
        if (dot_p != std::string::npos) {
            std::string mod_name = trim(fname.substr(0, dot_p));
            std::string mfn_name = trim(fname.substr(dot_p + 1));
            auto mod_it = ctx.module_aliases.find(mod_name);
            std::string target_mod = (mod_it != ctx.module_aliases.end()) ? mod_it->second : mod_name;

            std::string call_expr = mfn_name + "(" + val.to_repr();
            for (const auto& a : extra_args) call_expr += ", " + a;
            call_expr += ")";
            return callModule(target_mod, call_expr, ctx);
        }

        // Builtin io prints
        if (fname == "println" || fname == "print") {
            std::string call_expr = fname + "(" + val.to_repr();
            for (const auto& a : extra_args) call_expr += ", " + a;
            call_expr += ")";
            return callModule("io", call_expr, ctx);
        }

        // User functions (case-insensitive fallback)
        auto fit = ctx.functions.find(fname);
        if (fit == ctx.functions.end()) {
            for (auto it = ctx.functions.begin(); it != ctx.functions.end(); ++it) {
                if (it->first.size() == fname.size()) {
                    bool eq = true;
                    for (size_t ci = 0; ci < fname.size(); ++ci) {
                        if (std::tolower((unsigned char)it->first[ci]) != std::tolower((unsigned char)fname[ci])) {
                            eq = false; break;
                        }
                    }
                    if (eq) { fit = it; break; }
                }
            }
        }

        if (fit != ctx.functions.end()) {
            auto fn = fit->second;
            std::vector<VMVal> call_args;
            call_args.push_back(val);
            for (const auto& a : extra_args) call_args.push_back(evaluate(a, ctx));

            auto saved_scopes = std::move(ctx.scopes);
            ctx.scopes.clear();
            ctx.scopes.push_back(saved_scopes.front());
            ctx.scopes.emplace_back();

            for (size_t pi = 0; pi < fn->params.size(); ++pi) {
                VMVal arg_v = (pi < call_args.size()) ? call_args[pi] : VMVal(0);
                ctx.scopes.back()[fn->params[pi]] = arg_v;
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

            auto updated_global = ctx.scopes.front();
            ctx.scopes = std::move(saved_scopes);
            ctx.scopes.front() = updated_global;
            return ret_v;
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
        auto idx_val = ExprEvaluator::evaluate(idx_expr, ctx);
        int64_t idx = idx_val.to_int();
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
        } else if (cur.type == VMType::Map) {
            cur.map_val[idx_val.to_string()] = val;
            ctx.set_var(var_name, cur);
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

        int64_t step = (start_v <= end_v) ? 1 : -1;
        ctx.push_scope();
        auto should_continue = [&](int64_t curr) {
            return (step > 0) ? (curr < end_v) : (curr >= end_v);
        };
        for (int64_t i = start_v; should_continue(i); i += step) {
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
        VMVal val = expr.empty() ? VMVal() : ExprEvaluator::evaluate(expr, ctx);
        ctx.signal = FlowSignal::Return;
        ctx.return_val = val;
        return val;
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

    static bool isIdentifier(const std::string& s) {
        if (s.empty()) return false;
        size_t start = (s[0] == '@' || s[0] == '&') ? 1 : 0;
        if (start >= s.size()) return false;
        if (!std::isalpha((unsigned char)s[start]) && s[start] != '_') return false;
        for (size_t i = start + 1; i < s.size(); ++i) {
            if (!std::isalnum((unsigned char)s[i]) && s[i] != '_') return false;
        }
        return true;
    }

    static std::vector<std::string> tokenizeLines(const std::string& source) {
        std::vector<std::string> raw_lines;
        std::stringstream ss(source);
        std::string line;
        bool is_first = true;
        while (std::getline(ss, line)) {
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

            // Strip single-line comments // or # respecting strings
            std::string clean = "";
            bool in_s = false;
            char q = 0;
            for (size_t ci = 0; ci < line.size(); ++ci) {
                char c = line[ci];
                if (in_s) {
                    clean += c;
                    if (c == q && (ci == 0 || line[ci - 1] != '\\')) in_s = false;
                } else {
                    if (c == '"' || c == '\'') {
                        in_s = true; q = c; clean += c;
                    } else if (c == '/' && ci + 1 < line.size() && line[ci + 1] == '/') {
                        break;
                    } else if (c == '#' && (ci == 0 || line[ci - 1] == ' ' || line[ci - 1] == '\t' || line[ci - 1] == ';')) {
                        break;
                    } else {
                        clean += c;
                    }
                }
            }
            clean = trim(clean);
            if (!clean.empty()) raw_lines.push_back(clean);
        }

        std::vector<std::string> lines;
        std::string cur = "";
        bool in_str = false;
        char q_char = 0;
        int paren = 0, bracket = 0, expr_brace = 0;

        for (const auto& rline : raw_lines) {
            for (size_t i = 0; i < rline.size(); ++i) {
                char c = rline[i];
                if (in_str) {
                    cur += c;
                    if (c == q_char && (i == 0 || rline[i-1] != '\\')) in_str = false;
                } else {
                    if (c == '"' || c == '\'') {
                        in_str = true;
                        q_char = c;
                        cur += c;
                    } else if (c == '(') {
                        paren++; cur += c;
                    } else if (c == ')') {
                        if (paren > 0) paren--; cur += c;
                    } else if (c == '[') {
                        bracket++; cur += c;
                    } else if (c == ']') {
                        if (bracket > 0) bracket--; cur += c;
                    } else if (c == '{') {
                        bool has_assign = false;
                        size_t eq_p = cur.find('=');
                        if (eq_p != std::string::npos && (eq_p == 0 || cur[eq_p-1] != '!') && (eq_p + 1 >= cur.size() || (cur[eq_p+1] != '=' && cur[eq_p+1] != '>'))) {
                            has_assign = true;
                        }
                        std::string tcur = trim(cur);
                        bool is_return = startsWith(tcur, "return") || startsWith(tcur, "!return");
                        if (has_assign || is_return || paren > 0 || bracket > 0 || expr_brace > 0) {
                            expr_brace++;
                            cur += c;
                        } else {
                            cur += c;
                            std::string t = trim(cur);
                            if (!t.empty()) lines.push_back(t);
                            cur.clear();
                        }
                    } else if (c == '}') {
                        if (expr_brace > 0) {
                            expr_brace--;
                            cur += c;
                        } else {
                            std::string t = trim(cur);
                            if (!t.empty()) lines.push_back(t);
                            cur.clear();
                            lines.push_back("}");
                        }
                    } else if (c == ';' && paren == 0 && bracket == 0 && expr_brace == 0) {
                        std::string t = trim(cur);
                        if (t == "END") lines.push_back("}");
                        else if (!t.empty()) lines.push_back(t);
                        cur.clear();
                    } else if (c == ',' && paren == 0 && bracket == 0 && expr_brace == 0 && trim(cur) == "END") {
                        lines.push_back("}");
                        cur.clear();
                    } else {
                        cur += c;
                    }
                }
            }
            if (paren == 0 && bracket == 0 && expr_brace == 0 && !in_str) {
                std::string rem = trim(cur);
                if (rem == "END") { lines.push_back("}"); cur.clear(); }
                else if (!rem.empty()) { lines.push_back(rem); cur.clear(); }
            } else {
                cur += " ";
            }
        }
        std::string rem = trim(cur);
        if (rem == "END") lines.push_back("}");
        else if (!rem.empty()) lines.push_back(rem);

        return lines;
    }

public:
    static int executeSource(const std::string& source, const std::vector<std::string>& args = {}) {
        VMContext ctx;
        ctx.cli_args = args;
        std::vector<VMVal> vargs;
        for (const auto& a : args) vargs.push_back(VMVal(a));
        ctx.set_var("@args", VMVal(vargs));
        ctx.set_var("args", VMVal(vargs));

        auto lines = tokenizeLines(source);

        std::vector<NodePtr> root_nodes;
        size_t i = 0;
        parseBlock(lines, i, root_nodes, ctx);

        // Execute top-level / functions
        for (const auto& node : root_nodes) {
            node->execute(ctx);
            if (ctx.signal == FlowSignal::Return || ctx.has_error) break;
        }

        // Check if main() exists
        auto fit = ctx.functions.find("main");
        if (fit != ctx.functions.end() && !ctx.has_error) {
            ctx.push_scope();
            for (const auto& node : fit->second->body) {
                node->execute(ctx);
                if (ctx.signal == FlowSignal::Return || ctx.has_error) break;
            }
            ctx.pop_scope();
        }

        if (ctx.has_error) return ctx.exit_code != 0 ? ctx.exit_code : 1;
        return ctx.exit_code;
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

    static VMVal executeReplSnippet(
        const std::string& input,
        VMContext& ctx,
        bool& is_expr,
        std::string& err_msg
    ) {
        is_expr = false;
        err_msg.clear();
        try {
            auto lines = tokenizeLines(input);
            if (lines.empty()) return VMVal();

            if (lines.size() == 1) {
                std::string single = lines[0];
                if (!single.empty() && single.back() == ';') single.pop_back();
                single = trim(single);

                // Ignore print statements from returning expressions
                if (startsWith(single, "io.println") || startsWith(single, "io.print") || startsWith(single, "io.clear") ||
                    startsWith(single, "iostream.println") || startsWith(single, "iostream.print") ||
                    startsWith(single, "println") || startsWith(single, "print")) {
                    ExprEvaluator::evaluate(single, ctx);
                    is_expr = false;
                    return VMVal();
                }

                // Check if it's assignment
                size_t eq_pos = single.find('=');
                bool is_assign = false;
                if (eq_pos != std::string::npos && eq_pos > 0 &&
                    single[eq_pos - 1] != '!' && single[eq_pos - 1] != '=' &&
                    single[eq_pos - 1] != '<' && single[eq_pos - 1] != '>' &&
                    (eq_pos + 1 >= single.size() || (single[eq_pos + 1] != '=' && single[eq_pos + 1] != '>'))) {
                    std::string left = trim(single.substr(0, eq_pos));
                    if (left.front() == '@' || left.front() == '&') left = left.substr(1);
                    if (isIdentifier(left)) {
                        is_assign = true;
                    }
                }
                for (const std::string& op : {"+=", "-=", "*=", "/="}) {
                    size_t op_p = single.find(op);
                    if (op_p != std::string::npos) {
                        std::string left = trim(single.substr(0, op_p));
                        if (left.front() == '@' || left.front() == '&') left = left.substr(1);
                        if (isIdentifier(left)) {
                            is_assign = true;
                            break;
                        }
                    }
                }
                if (single.find('[') != std::string::npos && single.find("]=") != std::string::npos) {
                    is_assign = true;
                }

                if (is_assign) {
                    std::vector<NodePtr> nodes;
                    size_t i = 0;
                    parseBlock(lines, i, nodes, ctx);
                    for (const auto& node : nodes) node->execute(ctx);
                    is_expr = false;
                    return VMVal();
                }

                // If not statement, evaluate as expression
                bool is_stmt = startsWith(single, "!func") || startsWith(single, "func") || startsWith(single, "def ") ||
                               startsWith(single, "!if") || startsWith(single, "?if") ||
                               startsWith(single, "!for") || startsWith(single, "!while") ||
                               startsWith(single, "&") || startsWith(single, "$") ||
                               (single.find('(') != std::string::npos && (endsWith(single, ":") || endsWith(single, "->") || endsWith(single, "{")));
                if (!is_stmt) {
                    VMVal v = ExprEvaluator::evaluate(single, ctx);
                    is_expr = true;
                    return v;
                }
            }

            // Block or multi-statement
            std::vector<NodePtr> nodes;
            size_t i = 0;
            parseBlock(lines, i, nodes, ctx);
            VMVal last_val;
            for (const auto& node : nodes) {
                last_val = node->execute(ctx);
                if (ctx.signal == FlowSignal::Return) {
                    ctx.signal = FlowSignal::None;
                    break;
                }
            }
            if (nodes.empty() && ctx.functions.count("main")) {
                auto main_fn = ctx.functions["main"];
                ctx.functions.erase("main");
                ctx.push_scope();
                for (const auto& n : main_fn->body) {
                    last_val = n->execute(ctx);
                    if (ctx.signal == FlowSignal::Return) break;
                }
                ctx.pop_scope();
                ctx.signal = FlowSignal::None;
            }
            is_expr = false;
            return last_val;
        } catch (const std::exception& e) {
            err_msg = e.what();
            return VMVal();
        } catch (...) {
            err_msg = "Runtime evaluation error";
            return VMVal();
        }
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

            // Directives: $import lib ... or $import file ... or $import "..." or %import ...
            if (startsWith(line, "$import") || startsWith(line, "$impoer") || startsWith(line, "@use") || startsWith(line, "%import")) {
                std::smatch m_lib;
                if (std::regex_match(line, m_lib, std::regex(R"(^(?:\$impo(?:rt|er)|\%import)(?:\s+lib)?\s+([^\s]+)(?:\s+as\s+([a-zA-Z0-9_]+))?;?$)"))) {
                    std::string lib_name = m_lib[1].str();
                    while (!lib_name.empty() && (lib_name.front() == '"' || lib_name.front() == '\'')) lib_name = lib_name.substr(1);
                    while (!lib_name.empty() && (lib_name.back() == '"' || lib_name.back() == '\'')) lib_name.pop_back();
                    std::string alias = (m_lib.size() > 2 && m_lib[2].matched) ? m_lib[2].str() : lib_name;
                    std::string canonical = lib_name;
                    if (lib_name == "iostream" || lib_name == "io") canonical = "io";
                    else if (lib_name == "system" || lib_name == "sys") canonical = "sys";
                    else if (lib_name == "string" || lib_name == "str") canonical = "str";
                    else if (lib_name == "retrotech" || lib_name == "rt") canonical = "rt";
                    else if (lib_name == "bytemask" || lib_name == "colormask" || lib_name == "mask") canonical = "mask";
                    else if (lib_name == "asyncIO" || lib_name == "async") canonical = "async";
                    else if (lib_name == "sound" || lib_name == "snd" || lib_name == "audio") canonical = "audio";
                    else if (lib_name == "media" || lib_name == "tag") canonical = "media";
                    else if (lib_name == "gui" || lib_name == "ui" || lib_name == "window" || lib_name == "win") canonical = "gui";
                    
                    ctx.module_aliases[alias] = canonical;
                    ctx.module_aliases[lib_name] = canonical;
                    continue;
                }
                if (std::regex_match(line, m_lib, std::regex(R"(^\$import\s+lib\s+from\s+([^\s\[]+)(?:\s*\[[^\]]*\])?(?:\s+as\s+([a-zA-Z0-9_]+))?;?$)"))) {
                    std::string mod_src = m_lib[1].str();
                    while (!mod_src.empty() && (mod_src.front() == '"' || mod_src.front() == '\'')) mod_src = mod_src.substr(1);
                    while (!mod_src.empty() && (mod_src.back() == '"' || mod_src.back() == '\'')) mod_src.pop_back();
                    std::string alias = (m_lib.size() > 2 && m_lib[2].matched) ? m_lib[2].str() : mod_src;
                    ctx.module_aliases[alias] = "sys";
                    continue;
                }
                std::smatch m_fimp;
                if (std::regex_match(line, m_fimp, std::regex(R"(^\$import(?:\s+file)?\s+([^\s]+)(?:\s+as\s+([a-zA-Z0-9_]+))?;?$)"))) {
                    std::string ftoken = m_fimp[1].str();
                    while (!ftoken.empty() && (ftoken.front() == '"' || ftoken.front() == '\'')) ftoken = ftoken.substr(1);
                    while (!ftoken.empty() && (ftoken.back() == '"' || ftoken.back() == '\'')) ftoken.pop_back();
                    std::string alias = (m_fimp.size() > 2 && m_fimp[2].matched) ? m_fimp[2].str() : "";
                    if (endsWith(ftoken, ".viss")) {
                        std::filesystem::path target_file = ftoken;
                        if (!std::filesystem::exists(target_file)) {
                            target_file = std::filesystem::current_path() / ftoken;
                        }
                        if (std::filesystem::exists(target_file)) {
                            std::ifstream inf(target_file);
                            std::stringstream ss; ss << inf.rdbuf();
                            auto file_lines = tokenizeLines(ss.str());
                            size_t fi = 0;
                            std::vector<NodePtr> file_nodes;
                            parseBlock(file_lines, fi, file_nodes, ctx);
                            for (const auto& fn_node : file_nodes) fn_node->execute(ctx);
                            if (!alias.empty()) {
                                std::vector<std::string> new_fnames;
                                for (const auto& kv : ctx.functions) {
                                    if (kv.first.find('.') == std::string::npos) {
                                        new_fnames.push_back(kv.first);
                                    }
                                }
                                for (const auto& fn_n : new_fnames) {
                                    ctx.functions[alias + "." + fn_n] = ctx.functions[fn_n];
                                }
                            }
                        }
                    }
                    continue;
                }
                continue;
            }

            // Function Definition: !main { or !func main or !func name(params) { or func name ...
            bool is_main_decl = (line == "!main {" || line == "!main" || startsWith(line, "$RUN") || startsWith(line, "RUN:") || startsWith(line, "!func main") || startsWith(line, "func main"));
            if (is_main_decl) {
                if (endsWith(line, "{") || endsWith(line, ":")) {
                    // ok
                } else if (idx < lines.size() && (trim(lines[idx]) == "{" || trim(lines[idx]) == ":")) {
                    idx++;
                }
                auto fn = std::make_shared<UserFunction>();
                fn->name = "main";
                parseBlock(lines, idx, fn->body, ctx);
                ctx.functions["main"] = fn;
                continue;
            }

            auto is_control_kw = [](const std::string& l) -> bool {
                return startsWith(l, "!if") || startsWith(l, "?if") || startsWith(l, "if ") || startsWith(l, "if(") ||
                       startsWith(l, "!while") || startsWith(l, "?while") || startsWith(l, "while ") || startsWith(l, "while(") ||
                       startsWith(l, "!for") || startsWith(l, "?for") || startsWith(l, "for ") || startsWith(l, "for(") ||
                       startsWith(l, "!loop") || startsWith(l, "loop ") || startsWith(l, "loop(") ||
                       startsWith(l, "!elif") || startsWith(l, "?elif") || startsWith(l, "elif") ||
                       startsWith(l, "!else") || startsWith(l, "?else") || startsWith(l, "else") ||
                       startsWith(l, "!return") || startsWith(l, "return");
            };

            bool is_func_decl = !is_control_kw(line) && (
                                startsWith(line, "!func") || startsWith(line, "func") || startsWith(line, "def ") ||
                                (startsWith(line, "$") && line.find('(') != std::string::npos) ||
                                (line.find('(') != std::string::npos && (endsWith(line, ":") || endsWith(line, "->") || endsWith(line, "-") || endsWith(line, "{"))));
            if (is_func_decl) {
                std::string decl_line = line;
                if (endsWith(decl_line, "{") || endsWith(decl_line, ":")) {
                    decl_line = trim(decl_line.substr(0, decl_line.size() - 1));
                } else if (endsWith(decl_line, "->")) {
                    decl_line = trim(decl_line.substr(0, decl_line.size() - 2));
                } else if (endsWith(decl_line, "-")) {
                    decl_line = trim(decl_line.substr(0, decl_line.size() - 1));
                } else if (idx < lines.size() && (trim(lines[idx]) == "{" || trim(lines[idx]) == ":" || trim(lines[idx]) == "->")) {
                    idx++;
                }
                size_t prefix_len = 0;
                if (startsWith(decl_line, "!func")) prefix_len = 5;
                else if (startsWith(decl_line, "func")) prefix_len = 4;
                else if (startsWith(decl_line, "def")) prefix_len = 3;
                else if (!decl_line.empty() && decl_line.front() == '$') prefix_len = 1;
                std::string decl = trim(decl_line.substr(prefix_len));
                size_t p_open = decl.find('(');
                size_t p_close = decl.rfind(')');
                std::string fname;
                std::string p_str = "";
                if (p_open != std::string::npos && p_close != std::string::npos && p_close > p_open) {
                    fname = trim(decl.substr(0, p_open));
                    p_str = decl.substr(p_open + 1, p_close - p_open - 1);
                } else {
                    fname = trim(decl);
                }
                while (!fname.empty() && (fname.front() == '!' || fname.front() == '@' || fname.front() == '$')) fname = fname.substr(1);
                if (!fname.empty()) {
                    auto fn = std::make_shared<UserFunction>();
                    fn->name = fname;
                    if (!p_str.empty()) {
                        std::stringstream p_ss(p_str);
                        std::string p_item;
                        while (std::getline(p_ss, p_item, ',')) {
                            p_item = trim(p_item);
                            size_t as_pos = p_item.find(" as ");
                            if (as_pos != std::string::npos) p_item = trim(p_item.substr(0, as_pos));
                            size_t colon_pos = p_item.find(':');
                            if (colon_pos != std::string::npos) p_item = trim(p_item.substr(0, colon_pos));
                            size_t eq_pos = p_item.find('=');
                            if (eq_pos != std::string::npos) p_item = trim(p_item.substr(0, eq_pos));
                            if (!p_item.empty() && p_item.front() == '@') p_item = p_item.substr(1);
                            p_item = trim(p_item);
                            if (!p_item.empty()) fn->params.push_back(p_item);
                        }
                    }
                    parseBlock(lines, idx, fn->body, ctx);
                    ctx.functions[fname] = fn;
                    continue;
                }
            }

            // If Statement: !if (cond) { or ?if (cond) { or if (cond)
            bool is_if_decl = startsWith(line, "!if") || startsWith(line, "?if") || startsWith(line, "if ") || startsWith(line, "if(");
            if (is_if_decl) {
                if (endsWith(line, "{")) {
                    line = trim(line.substr(0, line.size() - 1));
                } else if (endsWith(line, "->")) {
                    line = trim(line.substr(0, line.size() - 2));
                } else if (idx < lines.size() && (trim(lines[idx]) == "{" || trim(lines[idx]) == "->")) {
                    idx++;
                }
                auto stmt = std::make_shared<StmtIf>();
                size_t p1 = line.find('(');
                size_t p2 = line.rfind(')');
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    stmt->cond = line.substr(p1 + 1, p2 - p1 - 1);
                } else {
                    size_t sp = line.find_first_of(" \t");
                    stmt->cond = (sp != std::string::npos) ? trim(line.substr(sp + 1)) : "true";
                }
                parseBlock(lines, idx, stmt->then_branch, ctx);

                // Check for elif / else
                while (idx < lines.size()) {
                    std::string next = trim(lines[idx]);
                    if (startsWith(next, "!elif") || startsWith(next, "?elif") || startsWith(next, "else if") || startsWith(next, "elif")) {
                        idx++;
                        if (endsWith(next, "{")) {
                            next = trim(next.substr(0, next.size() - 1));
                        } else if (endsWith(next, "->")) {
                            next = trim(next.substr(0, next.size() - 2));
                        } else if (idx < lines.size() && (trim(lines[idx]) == "{" || trim(lines[idx]) == "->")) {
                            idx++;
                        }
                        size_t ep1 = next.find('(');
                        size_t ep2 = next.rfind(')');
                        std::string econd = "true";
                        if (ep1 != std::string::npos && ep2 != std::string::npos && ep2 > ep1) {
                            econd = next.substr(ep1 + 1, ep2 - ep1 - 1);
                        } else {
                            size_t sp = next.find_first_of(" \t");
                            if (sp != std::string::npos) econd = trim(next.substr(sp + 1));
                        }
                        std::vector<NodePtr> e_branch;
                        parseBlock(lines, idx, e_branch, ctx);
                        stmt->elif_branches.push_back({econd, e_branch});
                    } else if (startsWith(next, "!else") || startsWith(next, "?else") || startsWith(next, "else")) {
                        idx++;
                        if (endsWith(next, "{")) {
                            next = trim(next.substr(0, next.size() - 1));
                        } else if (endsWith(next, "->")) {
                            next = trim(next.substr(0, next.size() - 2));
                        } else if (idx < lines.size() && (trim(lines[idx]) == "{" || trim(lines[idx]) == "->")) {
                            idx++;
                        }
                        parseBlock(lines, idx, stmt->else_branch, ctx);
                        break;
                    } else {
                        break;
                    }
                }
                out_nodes.push_back(stmt);
                continue;
            }

            // While Loop: !while (cond) { or while (cond)
            bool is_while_decl = startsWith(line, "!while") || startsWith(line, "?while") || startsWith(line, "while ") || startsWith(line, "while(");
            if (is_while_decl) {
                if (endsWith(line, "{")) {
                    line = trim(line.substr(0, line.size() - 1));
                } else if (idx < lines.size() && trim(lines[idx]) == "{") {
                    idx++;
                }
                size_t p1 = line.find('(');
                size_t p2 = line.rfind(')');
                std::string cond = "true";
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    cond = line.substr(p1 + 1, p2 - p1 - 1);
                } else {
                    size_t sp = line.find_first_of(" \t");
                    if (sp != std::string::npos) cond = trim(line.substr(sp + 1));
                }
                std::vector<NodePtr> b;
                parseBlock(lines, idx, b, ctx);
                out_nodes.push_back(std::make_shared<StmtWhile>(cond, b));
                continue;
            }

            // For Range Loop: !for @i in 0..10 { or for @i in 0..10
            bool is_for_decl = (startsWith(line, "!for") || startsWith(line, "for ") || startsWith(line, "for(")) && line.find(" in ") != std::string::npos;
            if (is_for_decl) {
                if (endsWith(line, "{")) {
                    line = trim(line.substr(0, line.size() - 1));
                } else if (idx < lines.size() && trim(lines[idx]) == "{") {
                    idx++;
                }
                size_t sp_pos = line.find_first_of(" \t(");
                std::string decl = trim(line.substr(sp_pos + 1));
                if (!decl.empty() && decl.back() == ')') decl = trim(decl.substr(0, decl.size() - 1));
                if (!decl.empty() && decl.front() == '(') decl = trim(decl.substr(1));
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

            // Loop Repeat: !loop (count) { or loop (count)
            bool is_loop_decl = startsWith(line, "!loop") || startsWith(line, "loop ") || startsWith(line, "loop(");
            if (is_loop_decl) {
                if (endsWith(line, "{")) {
                    line = trim(line.substr(0, line.size() - 1));
                } else if (idx < lines.size() && trim(lines[idx]) == "{") {
                    idx++;
                }
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

            // Subscript Assignment: @arr[idx] = val or arr[idx] = val
            if (line.find('[') != std::string::npos && line.find("]=") != std::string::npos) {
                size_t b_open = line.find('[');
                std::string vname = trim(line.substr(0, b_open));
                if (vname.front() == '@' || vname.front() == '&') vname = vname.substr(1);
                if (isIdentifier(vname)) {
                    size_t b_close = line.find(']');
                    std::string ie = line.substr(b_open + 1, b_close - b_open - 1);
                    size_t eq = line.find('=', b_close);
                    std::string ve = trim(line.substr(eq + 1));
                    out_nodes.push_back(std::make_shared<StmtSubscriptAssign>(vname, ie, ve));
                    continue;
                }
            }

            // Compound Variable Assignment: @x += 1, x += 1, etc.
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
            if (op_pos != std::string::npos) {
                std::string vname = trim(line.substr(0, op_pos));
                if (vname.front() == '@' || vname.front() == '&') vname = vname.substr(1);
                if (isIdentifier(vname)) {
                    std::string ex = trim(line.substr(op_pos + found_op.size()));
                    out_nodes.push_back(std::make_shared<StmtAssign>(vname, found_op, ex));
                    continue;
                }
            }

            // Generic Variable Assignment: @x = expr or x = expr [| type]
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos && eq_pos > 0 &&
                line[eq_pos - 1] != '!' && line[eq_pos - 1] != '=' &&
                line[eq_pos - 1] != '<' && line[eq_pos - 1] != '>' &&
                (eq_pos + 1 >= line.size() || (line[eq_pos + 1] != '=' && line[eq_pos + 1] != '>'))) {
                std::string vname = trim(line.substr(0, eq_pos));
                if (vname.front() == '@' || vname.front() == '&') vname = vname.substr(1);
                if (isIdentifier(vname)) {
                    std::string expr = trim(line.substr(eq_pos + 1));
                    size_t pipe_pos = expr.find('|');
                    if (pipe_pos != std::string::npos && expr.find('|', pipe_pos + 1) == std::string::npos) {
                        expr = trim(expr.substr(0, pipe_pos));
                    }
                    out_nodes.push_back(std::make_shared<StmtAssign>(vname, "=", expr));
                    continue;
                }
            }

            // Assignment without '=' (e.g. @var expr):
            if (!line.empty() && (line.front() == '@' || line.front() == '&')) {
                size_t sp = line.find_first_of(" \t");
                if (sp != std::string::npos) {
                    std::string vname = line.substr(1, sp - 1);
                    if (isIdentifier(vname)) {
                        std::string rest = trim(line.substr(sp + 1));
                        if (!rest.empty() && rest.front() != '=' &&
                            !startsWith(rest, "+=") && !startsWith(rest, "-=") &&
                            !startsWith(rest, "*=") && !startsWith(rest, "/=") &&
                            !startsWith(rest, "create |") && !startsWith(rest, "write ")) {
                            out_nodes.push_back(std::make_shared<StmtAssign>(vname, "=", rest));
                            continue;
                        }
                    }
                }
            }

            // Flow control: !return, !break, !continue
            if (startsWith(line, "!return") || startsWith(line, "return")) {
                std::string ret_expr = "";
                std::string sub = startsWith(line, "!return") ? line.substr(7) : line.substr(6);
                ret_expr = trim(sub);
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
