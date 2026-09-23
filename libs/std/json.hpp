#pragma once
#include "../vissrt.hpp"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace viss {
    namespace json {

        enum class JsonType { Null, Bool, Int, Number, String, Array, Object };

        class JsonValue {
        public:
            JsonType type = JsonType::Null;
            Bool bool_val = false;
            Int int_val = 0;
            Dec num_val = 0.0;
            Str str_val = "";
            std::vector<JsonValue> arr_val;
            std::map<Str, JsonValue> obj_val;

            JsonValue() : type(JsonType::Null) {}
            JsonValue(Bool b) : type(JsonType::Bool), bool_val(b) {}
            JsonValue(Int i) : type(JsonType::Int), int_val(i), num_val((Dec)i) {}
            JsonValue(int i) : type(JsonType::Int), int_val((Int)i), num_val((Dec)i) {}
            JsonValue(Dec d) : type(JsonType::Number), num_val(d), int_val((Int)d) {}
            JsonValue(const Str& s) : type(JsonType::String), str_val(s) {}
            JsonValue(const char* s) : type(JsonType::String), str_val(s) {}

            inline Bool is_null() const { return type == JsonType::Null; }
            inline Bool is_bool() const { return type == JsonType::Bool; }
            inline Bool is_int() const { return type == JsonType::Int; }
            inline Bool is_number() const { return type == JsonType::Number || type == JsonType::Int; }
            inline Bool is_str() const { return type == JsonType::String; }
            inline Bool is_array() const { return type == JsonType::Array; }
            inline Bool is_object() const { return type == JsonType::Object; }

            inline Str as_str() const {
                if (type == JsonType::String) return str_val;
                if (type == JsonType::Int) return std::to_string(int_val);
                if (type == JsonType::Number) return std::to_string(num_val);
                if (type == JsonType::Bool) return bool_val ? "true" : "false";
                return "";
            }

            inline Int as_int() const {
                if (type == JsonType::Int) return int_val;
                if (type == JsonType::Number) return (Int)num_val;
                if (type == JsonType::Bool) return bool_val ? 1 : 0;
                if (type == JsonType::String) {
                    try { return std::stoll(str_val); } catch (...) { return 0; }
                }
                return 0;
            }

            inline Dec as_dec() const {
                if (type == JsonType::Number) return num_val;
                if (type == JsonType::Int) return (Dec)int_val;
                if (type == JsonType::String) {
                    try { return std::stod(str_val); } catch (...) { return 0.0; }
                }
                return 0.0;
            }

            inline Bool as_bool() const {
                if (type == JsonType::Bool) return bool_val;
                if (type == JsonType::Int) return int_val != 0;
                if (type == JsonType::String) return str_val == "true" || str_val == "1";
                return false;
            }

            inline Int size() const {
                if (type == JsonType::Array) return (Int)arr_val.size();
                if (type == JsonType::Object) return (Int)obj_val.size();
                if (type == JsonType::String) return (Int)str_val.size();
                return 0;
            }

            inline Bool has(const Str& key) const {
                if (type != JsonType::Object) return false;
                return obj_val.find(key) != obj_val.end();
            }

            inline List<Str> keys() const {
                List<Str> kList;
                if (type == JsonType::Object) {
                    for (const auto& p : obj_val) kList.add(p.first);
                }
                return kList;
            }

            inline JsonValue get(const Str& key) const {
                return (*this)[key];
            }
            inline JsonValue get(Int index) const {
                return (*this)[index];
            }

            inline JsonValue& operator[](const Str& key) {

                if (type != JsonType::Object) {
                    type = JsonType::Object;
                    arr_val.clear();
                }
                return obj_val[key];
            }

            inline const JsonValue& operator[](const Str& key) const {
                static JsonValue nullVal;
                if (type != JsonType::Object) return nullVal;
                auto it = obj_val.find(key);
                if (it != obj_val.end()) return it->second;
                return nullVal;
            }

            inline JsonValue& operator[](Int index) {
                if (type != JsonType::Array) {
                    type = JsonType::Array;
                    obj_val.clear();
                }
                if (index < 0) index = 0;
                if ((size_t)index >= arr_val.size()) arr_val.resize(index + 1);
                return arr_val[index];
            }

            inline const JsonValue& operator[](Int index) const {
                static JsonValue nullVal;
                if (type != JsonType::Array || index < 0 || (size_t)index >= arr_val.size()) return nullVal;
                return arr_val[index];
            }

            inline void push(const JsonValue& v) {
                if (type != JsonType::Array) {
                    type = JsonType::Array;
                    obj_val.clear();
                }
                arr_val.push_back(v);
            }
        };

        // --- Serializer ---
        inline Str stringify(const JsonValue& v, Bool pretty = false, Int indent = 0) {
            std::stringstream ss;
            switch (v.type) {
                case JsonType::Null: ss << "null"; break;
                case JsonType::Bool: ss << (v.bool_val ? "true" : "false"); break;
                case JsonType::Int: ss << v.int_val; break;
                case JsonType::Number: ss << v.num_val; break;
                case JsonType::String: {
                    ss << "\"";
                    for (char c : v.str_val) {
                        if (c == '"') ss << "\\\"";
                        else if (c == '\\') ss << "\\\\";
                        else if (c == '\n') ss << "\\n";
                        else if (c == '\r') ss << "\\r";
                        else if (c == '\t') ss << "\\t";
                        else ss << c;
                    }
                    ss << "\"";
                    break;
                }
                case JsonType::Array: {
                    ss << "[";
                    for (size_t i = 0; i < v.arr_val.size(); ++i) {
                        if (i > 0) ss << (pretty ? ", " : ",");
                        ss << stringify(v.arr_val[i], pretty, indent + 2);
                    }
                    ss << "]";
                    break;
                }
                case JsonType::Object: {
                    if (pretty) ss << "{\n"; else ss << "{";
                    size_t i = 0;
                    for (const auto& p : v.obj_val) {
                        if (i > 0) {
                            if (pretty) ss << ",\n"; else ss << ",";
                        }
                        if (pretty) ss << Str(indent + 2, ' ');
                        ss << "\"" << p.first << "\":" << (pretty ? " " : "") << stringify(p.second, pretty, indent + 2);
                        i++;
                    }
                    if (pretty) { ss << "\n" << Str(indent, ' ') << "}"; } else { ss << "}"; }
                    break;
                }
            }
            return ss.str();
        }

        inline Str pretty(const JsonValue& v) {
            return stringify(v, true, 0);
        }

        // --- Parser ---
        class JsonParser {
        private:
            Str s;
            size_t pos = 0;

            void skip_ws() {
                while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) {
                    pos++;
                }
            }

            char peek() { skip_ws(); return pos < s.size() ? s[pos] : '\0'; }
            char get() { skip_ws(); return pos < s.size() ? s[pos++] : '\0'; }

            Str parse_string() {
                get(); // skip opening quote
                Str res = "";
                while (pos < s.size()) {
                    char c = s[pos++];
                    if (c == '"') return res;
                    if (c == '\\' && pos < s.size()) {
                        char esc = s[pos++];
                        if (esc == 'n') res += '\n';
                        else if (esc == 'r') res += '\r';
                        else if (esc == 't') res += '\t';
                        else if (esc == '"') res += '"';
                        else if (esc == '\\') res += '\\';
                        else res += esc;
                    } else {
                        res += c;
                    }
                }
                return res;
            }

            JsonValue parse_number() {
                size_t start = pos;
                bool is_dec = false;
                if (s[pos] == '-') pos++;
                while (pos < s.size() && (isdigit((unsigned char)s[pos]) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+')) {
                    if (s[pos] == '.') is_dec = true;
                    pos++;
                }
                Str num_str = s.substr(start, pos - start);
                if (is_dec) {
                    try { return JsonValue(std::stod(num_str)); } catch (...) { return JsonValue(0.0); }
                } else {
                    try { return JsonValue((Int)std::stoll(num_str)); } catch (...) { return JsonValue((Int)0); }
                }
            }

        public:
            JsonParser(const Str& src) : s(src), pos(0) {}

            JsonValue parse_value() {
                skip_ws();
                if (pos >= s.size()) return JsonValue();
                char c = s[pos];

                if (c == '"') {
                    return JsonValue(parse_string());
                } else if (c == '{') {
                    get(); // skip {
                    JsonValue obj;
                    obj.type = JsonType::Object;
                    if (peek() == '}') { get(); return obj; }
                    while (pos < s.size()) {
                        if (peek() != '"') break;
                        Str key = parse_string();
                        if (get() != ':') break;
                        obj.obj_val[key] = parse_value();
                        char next = peek();
                        if (next == ',') { get(); }
                        else if (next == '}') { get(); break; }
                        else break;
                    }
                    return obj;
                } else if (c == '[') {
                    get(); // skip [
                    JsonValue arr;
                    arr.type = JsonType::Array;
                    if (peek() == ']') { get(); return arr; }
                    while (pos < s.size()) {
                        arr.arr_val.push_back(parse_value());
                        char next = peek();
                        if (next == ',') { get(); }
                        else if (next == ']') { get(); break; }
                        else break;
                    }
                    return arr;
                } else if (c == 't' && s.compare(pos, 4, "true") == 0) {
                    pos += 4; return JsonValue(true);
                } else if (c == 'f' && s.compare(pos, 5, "false") == 0) {
                    pos += 5; return JsonValue(false);
                } else if (c == 'n' && s.compare(pos, 4, "null") == 0) {
                    pos += 4; return JsonValue();
                } else if (c == '-' || isdigit((unsigned char)c)) {
                    return parse_number();
                }
                return JsonValue();
            }
        };

        inline JsonValue parse(const Str& src) {
            JsonParser p(src);
            return p.parse_value();
        }
    }
}
