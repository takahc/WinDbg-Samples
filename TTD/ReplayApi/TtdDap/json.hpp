// Simple JSON library for DAP implementation
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <map>
#include <vector>
#include <variant>
#include <sstream>
#include <iomanip>

namespace nlohmann
{
    class json
    {
    public:
        using value_type = std::variant<
            std::nullptr_t,
            bool,
            int,
            double,
            std::string,
            std::vector<json>,
            std::map<std::string, json>
        >;

        value_type value_;

        json() : value_(nullptr) {}
        json(std::nullptr_t) : value_(nullptr) {}
        json(bool b) : value_(b) {}
        json(int i) : value_(i) {}
        json(double d) : value_(d) {}
        json(const char* s) : value_(std::string(s)) {}
        json(const std::string& s) : value_(s) {}
        json(const std::vector<json>& arr) : value_(arr) {}
        json(const std::map<std::string, json>& obj) : value_(obj) {}

        // Array operations
        static json array() {
            return json(std::vector<json>());
        }

        static json object() {
            return json(std::map<std::string, json>());
        }

        void push_back(const json& item) {
            if (auto* arr = std::get_if<std::vector<json>>(&value_)) {
                arr->push_back(item);
            } else {
                throw std::runtime_error("Not an array");
            }
        }

        // Object operations
        json& operator[](const std::string& key) {
            if (auto* obj = std::get_if<std::map<std::string, json>>(&value_)) {
                return (*obj)[key];
            } else {
                // Convert to object if null
                if (std::holds_alternative<std::nullptr_t>(value_)) {
                    value_ = std::map<std::string, json>();
                    return (*std::get_if<std::map<std::string, json>>(&value_))[key];
                }
                throw std::runtime_error("Not an object");
            }
        }

        const json& operator[](const std::string& key) const {
            if (auto* obj = std::get_if<std::map<std::string, json>>(&value_)) {
                auto it = obj->find(key);
                if (it != obj->end()) {
                    return it->second;
                }
                static json null_value;
                return null_value;
            }
            throw std::runtime_error("Not an object");
        }

        // Type checking
        bool is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
        bool is_boolean() const { return std::holds_alternative<bool>(value_); }
        bool is_number() const { return std::holds_alternative<int>(value_) || std::holds_alternative<double>(value_); }
        bool is_string() const { return std::holds_alternative<std::string>(value_); }
        bool is_array() const { return std::holds_alternative<std::vector<json>>(value_); }
        bool is_object() const { return std::holds_alternative<std::map<std::string, json>>(value_); }

        // Value access
        template<typename T>
        T value(const std::string& key, const T& default_val) const {
            if (is_object()) {
                auto* obj = std::get_if<std::map<std::string, json>>(&value_);
                auto it = obj->find(key);
                if (it != obj->end()) {
                    if constexpr (std::is_same_v<T, std::string>) {
                        if (auto* str = std::get_if<std::string>(&it->second.value_)) {
                            return *str;
                        }
                    } else if constexpr (std::is_same_v<T, int>) {
                        if (auto* i = std::get_if<int>(&it->second.value_)) {
                            return *i;
                        }
                    } else if constexpr (std::is_same_v<T, bool>) {
                        if (auto* b = std::get_if<bool>(&it->second.value_)) {
                            return *b;
                        }
                    }
                }
            }
            return default_val;
        }

        // Serialization
        std::string dump(int /*indent*/ = -1) const {
            std::ostringstream oss;
            dump_impl(oss);
            return oss.str();
        }

        // Parsing
        static json parse(const std::string& str) {
            size_t pos = 0;
            return parse_impl(str, pos);
        }

    private:
        void dump_impl(std::ostringstream& oss) const {
            std::visit([&](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    oss << "null";
                } else if constexpr (std::is_same_v<T, bool>) {
                    oss << (val ? "true" : "false");
                } else if constexpr (std::is_same_v<T, int>) {
                    oss << val;
                } else if constexpr (std::is_same_v<T, double>) {
                    oss << val;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    oss << '"' << escape_string(val) << '"';
                } else if constexpr (std::is_same_v<T, std::vector<json>>) {
                    oss << "[";
                    for (size_t i = 0; i < val.size(); ++i) {
                        if (i > 0) oss << ",";
                        val[i].dump_impl(oss);
                    }
                    oss << "]";
                } else if constexpr (std::is_same_v<T, std::map<std::string, json>>) {
                    oss << "{";
                    bool first = true;
                    for (const auto& [key, value] : val) {
                        if (!first) oss << ",";
                        first = false;
                        oss << '"' << escape_string(key) << "\":";
                        value.dump_impl(oss);
                    }
                    oss << "}";
                }
            }, value_);
        }

        static std::string escape_string(const std::string& str) {
            std::string result;
            for (char c : str) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b"; break;
                    case '\f': result += "\\f"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default: result += c; break;
                }
            }
            return result;
        }

        static json parse_impl(const std::string& str, size_t& pos) {
            skip_whitespace(str, pos);
            if (pos >= str.length()) {
                throw std::runtime_error("Unexpected end of input");
            }

            char c = str[pos];
            if (c == 'n') {
                if (str.substr(pos, 4) == "null") {
                    pos += 4;
                    return json();
                }
            } else if (c == 't') {
                if (str.substr(pos, 4) == "true") {
                    pos += 4;
                    return json(true);
                }
            } else if (c == 'f') {
                if (str.substr(pos, 5) == "false") {
                    pos += 5;
                    return json(false);
                }
            } else if (c == '"') {
                return json(parse_string(str, pos));
            } else if (c == '[') {
                return parse_array(str, pos);
            } else if (c == '{') {
                return parse_object(str, pos);
            } else if (c == '-' || (c >= '0' && c <= '9')) {
                return parse_number(str, pos);
            }

            throw std::runtime_error("Invalid JSON");
        }

        static void skip_whitespace(const std::string& str, size_t& pos) {
            while (pos < str.length() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r')) {
                pos++;
            }
        }

        static std::string parse_string(const std::string& str, size_t& pos) {
            if (str[pos] != '"') throw std::runtime_error("Expected '\"'");
            pos++;
            std::string result;
            while (pos < str.length() && str[pos] != '"') {
                if (str[pos] == '\\') {
                    pos++;
                    if (pos >= str.length()) throw std::runtime_error("Unexpected end in string");
                    switch (str[pos]) {
                        case '"': result += '"'; break;
                        case '\\': result += '\\'; break;
                        case '/': result += '/'; break;
                        case 'b': result += '\b'; break;
                        case 'f': result += '\f'; break;
                        case 'n': result += '\n'; break;
                        case 'r': result += '\r'; break;
                        case 't': result += '\t'; break;
                        default: throw std::runtime_error("Invalid escape sequence");
                    }
                } else {
                    result += str[pos];
                }
                pos++;
            }
            if (pos >= str.length()) throw std::runtime_error("Unterminated string");
            pos++; // skip closing quote
            return result;
        }

        static json parse_array(const std::string& str, size_t& pos) {
            if (str[pos] != '[') throw std::runtime_error("Expected '['");
            pos++;
            skip_whitespace(str, pos);
            
            std::vector<json> arr;
            if (pos < str.length() && str[pos] == ']') {
                pos++;
                return json(arr);
            }

            while (pos < str.length()) {
                arr.push_back(parse_impl(str, pos));
                skip_whitespace(str, pos);
                
                if (pos < str.length() && str[pos] == ']') {
                    pos++;
                    return json(arr);
                } else if (pos < str.length() && str[pos] == ',') {
                    pos++;
                    skip_whitespace(str, pos);
                } else {
                    throw std::runtime_error("Expected ',' or ']'");
                }
            }
            throw std::runtime_error("Unterminated array");
        }

        static json parse_object(const std::string& str, size_t& pos) {
            if (str[pos] != '{') throw std::runtime_error("Expected '{'");
            pos++;
            skip_whitespace(str, pos);
            
            std::map<std::string, json> obj;
            if (pos < str.length() && str[pos] == '}') {
                pos++;
                return json(obj);
            }

            while (pos < str.length()) {
                skip_whitespace(str, pos);
                std::string key = parse_string(str, pos);
                skip_whitespace(str, pos);
                
                if (pos >= str.length() || str[pos] != ':') {
                    throw std::runtime_error("Expected ':'");
                }
                pos++;
                skip_whitespace(str, pos);
                
                obj[key] = parse_impl(str, pos);
                skip_whitespace(str, pos);
                
                if (pos < str.length() && str[pos] == '}') {
                    pos++;
                    return json(obj);
                } else if (pos < str.length() && str[pos] == ',') {
                    pos++;
                } else {
                    throw std::runtime_error("Expected ',' or '}'");
                }
            }
            throw std::runtime_error("Unterminated object");
        }

        static json parse_number(const std::string& str, size_t& pos) {
            size_t start = pos;
            if (str[pos] == '-') pos++;
            
            if (pos >= str.length() || str[pos] < '0' || str[pos] > '9') {
                throw std::runtime_error("Invalid number");
            }
            
            while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
                pos++;
            }
            
            bool is_double = false;
            if (pos < str.length() && str[pos] == '.') {
                is_double = true;
                pos++;
                while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
                    pos++;
                }
            }
            
            std::string num_str = str.substr(start, pos - start);
            if (is_double) {
                return json(std::stod(num_str));
            } else {
                return json(std::stoi(num_str));
            }
        }
    };
}