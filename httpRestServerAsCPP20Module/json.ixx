export module json;

import <string>;
import <string_view>;
import <variant>;
import <vector>;
import <map>;
import <stdexcept>;
import <cctype>;
import <type_traits>;

export namespace json {

    struct Json;

    using JsonValue = std::variant<
        std::nullptr_t,
        bool,
        double,
        std::string,
        std::vector<Json>,
        std::map<std::string, Json>
    >;

    struct Json {
        JsonValue value;

        // Constructors
        Json() : value(nullptr) {}
        Json(std::nullptr_t) : value(nullptr) {}
        Json(bool b) : value(b) {}
        Json(double d) : value(d) {}
        Json(const std::string& s) : value(s) {}
        Json(const char* s) : value(std::string(s)) {}
        Json(const std::vector<Json>& arr) : value(arr) {}
        Json(const std::map<std::string, Json>& obj) : value(obj) {}

        // Factory methods
        static Json object() { return std::map<std::string, Json>{}; }
        static Json array() { return std::vector<Json>{}; }

        // Type checks
        bool is_null()   const { return std::holds_alternative<std::nullptr_t>(value); }
        bool is_bool()   const { return std::holds_alternative<bool>(value); }
        bool is_number() const { return std::holds_alternative<double>(value); }
        bool is_string() const { return std::holds_alternative<std::string>(value); }
        bool is_array()  const { return std::holds_alternative<std::vector<Json>>(value); }
        bool is_object() const { return std::holds_alternative<std::map<std::string, Json>>(value); }

        // Accessors
        const auto& as_bool()   const { return std::get<bool>(value); }
        const auto& as_number() const { return std::get<double>(value); }
        const auto& as_string() const { return std::get<std::string>(value); }
        const auto& as_array()  const { return std::get<std::vector<Json>>(value); }
        const auto& as_object() const { return std::get<std::map<std::string, Json>>(value); }

        // Mutable accessors
        auto& as_array() { return std::get<std::vector<Json>>(value); }
        auto& as_object() { return std::get<std::map<std::string, Json>>(value); }

        // Object indexing
        Json& operator[](const std::string& key) {
            if (!is_object()) value = std::map<std::string, Json>{};
            return std::get<std::map<std::string, Json>>(value)[key];
        }

        const Json& operator[](const std::string& key) const {
            return std::get<std::map<std::string, Json>>(value).at(key);
        }

        // Array push_back
        void push_back(const Json& j) {
            if (!is_array()) value = std::vector<Json>{};
            std::get<std::vector<Json>>(value).push_back(j);
        }

        // Safe getter with default
        template<typename T>
        T get(const std::string& key, const T& def) const {
            if (!is_object()) return def;
            auto& obj = std::get<std::map<std::string, Json>>(value);
            auto it = obj.find(key);
            if (it == obj.end()) return def;

            if constexpr (std::is_same_v<T, std::string>) {
                return it->second.is_string() ? it->second.as_string() : def;
            }
            else if constexpr (std::is_same_v<T, int>) {
                return it->second.is_number() ? static_cast<int>(it->second.as_number()) : def;
            }
            else if constexpr (std::is_same_v<T, double>) {
                return it->second.is_number() ? it->second.as_number() : def;
            }
            else if constexpr (std::is_same_v<T, bool>) {
                return it->second.is_bool() ? it->second.as_bool() : def;
            }
            else {
                return def;
            }
        }
    };

    // --- JSON Parser ---
    class Parser {
        std::string_view text;
        size_t pos{ 0 };

        char peek() const {
            return pos < text.size() ? text[pos] : '\0';
        }

        char get() {
            return pos < text.size() ? text[pos++] : '\0';
        }

        void skip_ws() {
            while (std::isspace(static_cast<unsigned char>(peek())))
                get();
        }

        std::string parse_string() {
            if (get() != '"') throw std::runtime_error("Expected string opening quote");
            std::string result;
            while (true) {
                char c = get();
                if (c == '"') break;
                if (c == '\\') {
                    char esc = get();
                    switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: throw std::runtime_error("Invalid escape");
                    }
                }
                else {
                    result += c;
                }
            }
            return result;
        }

        double parse_number() {
            size_t start = pos;
            while (std::isdigit(peek()) || peek() == '-' || peek() == '.' || peek() == 'e' || peek() == 'E' || peek() == '+')
                get();
            return std::stod(std::string(text.substr(start, pos - start)));
        }

        Json parse_value() {
            skip_ws();
            char c = peek();
            if (c == '"') return Json(parse_string());
            if (c == '{') return parse_object();
            if (c == '[') return parse_array();
            if (std::isdigit(c) || c == '-') return Json(parse_number());
            if (text.substr(pos, 4) == "true") { pos += 4; return Json(true); }
            if (text.substr(pos, 5) == "false") { pos += 5; return Json(false); }
            if (text.substr(pos, 4) == "null") { pos += 4; return Json(nullptr); }
            throw std::runtime_error("Invalid JSON value");
        }

        Json parse_array() {
            get(); // [
            skip_ws();
            std::vector<Json> arr;
            if (peek() == ']') { get(); return arr; }
            while (true) {
                arr.push_back(parse_value());
                skip_ws();
                char c = get();
                if (c == ']') break;
                if (c != ',') throw std::runtime_error("Expected , or ] in array");
                skip_ws();
            }
            return arr;
        }

        Json parse_object() {
            get(); // {
            skip_ws();
            std::map<std::string, Json> obj;
            if (peek() == '}') { get(); return obj; }
            while (true) {
                skip_ws();
                std::string key = parse_string();
                skip_ws();
                if (get() != ':') throw std::runtime_error("Expected : after key");
                skip_ws();
                obj[key] = parse_value();
                skip_ws();
                char c = get();
                if (c == '}') break;
                if (c != ',') throw std::runtime_error("Expected , or } in object");
                skip_ws();
            }
            return obj;
        }

    public:
        Parser(std::string_view sv) : text(sv) {}
        Json parse() {
            auto val = parse_value();
            skip_ws();
            if (pos != text.size()) throw std::runtime_error("Extra characters after JSON");
            return val;
        }
    };

    inline Json parse(std::string_view sv) {
        return Parser(sv).parse();
    }

    // --- JSON Serializer ---
    inline std::string stringify_string(const std::string& s) {
        std::string out = "\"";
        for (char c : s) {
            switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
            }
        }
        out += "\"";
        return out;
    }

    inline std::string stringify(const Json& j) {
        if (j.is_null()) return "null";
        if (j.is_bool()) return j.as_bool() ? "true" : "false";
        if (j.is_number()) return std::to_string(j.as_number());
        if (j.is_string()) return stringify_string(j.as_string());
        if (j.is_array()) {
            std::string out = "[";
            const auto& arr = j.as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) out += ",";
                out += stringify(arr[i]);
            }
            out += "]";
            return out;
        }
        if (j.is_object()) {
            std::string out = "{";
            const auto& obj = j.as_object();
            bool first = true;
            for (auto& [k, v] : obj) {
                if (!first) out += ",";
                first = false;
                out += stringify_string(k) + ":" + stringify(v);
            }
            out += "}";
            return out;
        }
        return "null";
    }

} // namespace json
