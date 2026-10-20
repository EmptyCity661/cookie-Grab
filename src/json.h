// 极简 JSON：生成器 + 递归下降解析器（header-only，无第三方依赖）
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mjson {

inline std::string Escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += (char)c;
            }
        }
    }
    return out;
}

struct Value;
using ValuePtr = std::shared_ptr<Value>;

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<ValuePtr> arr;
    std::map<std::string, ValuePtr> obj;

    bool Has(const char* k) const { return type == Object && obj.count(k); }
    const std::string& GetStr(const char* k, const std::string& def = "") const {
        auto it = obj.find(k);
        return (it != obj.end() && it->second->type == String) ? it->second->str : def;
    }
    bool GetBool(const char* k, bool def) const {
        auto it = obj.find(k);
        return (it != obj.end() && it->second->type == Bool) ? it->second->b : def;
    }
};

// ---------- 生成器 ----------
struct Writer {
    std::string out;
    bool needComma = false;
    bool afterKey = false; // 刚写完 Key，下一个值不换行
    int indent = 0;
    bool pretty = false;

    void Comma() { if (needComma) out += ','; needComma = false; }
    void Nl() { if (pretty) { out += '\n'; out.append((size_t)indent * 2, ' '); } }
    void ValNl() { if (afterKey) { afterKey = false; return; } Nl(); }

    void BeginObject() { Comma(); ValNl(); out += '{'; needComma = false; indent++; }
    void EndObject() { indent--; Nl(); out += '}'; needComma = true; }
    void BeginArray() { Comma(); ValNl(); out += '['; needComma = false; indent++; }
    void EndArray() { indent--; Nl(); out += ']'; needComma = true; }
    void Key(const std::string& k) { Comma(); Nl(); out += '"'; out += Escape(k); out += "\":"; if (pretty) out += ' '; needComma = false; afterKey = true; }
    void Str(const std::string& v) { Comma(); ValNl(); out += '"'; out += Escape(v); out += '"'; needComma = true; }
    void Raw(const std::string& v) { Comma(); ValNl(); out += v; needComma = true; }
    void Bool(bool v) { Raw(v ? "true" : "false"); }
    void Int(long long v) { char b[32]; snprintf(b, sizeof(b), "%lld", v); Raw(b); }
    void Dbl(double v) {
        if (v == (long long)v) { Int((long long)v); return; }
        char b[40]; snprintf(b, sizeof(b), "%.10g", v); Raw(b);
    }
};

// ---------- 解析器 ----------
namespace detail {
struct Parser {
    const char* p;
    const char* end;
    void Ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool Lit(const char* s) {
        size_t n = strlen(s);
        if ((size_t)(end - p) >= n && strncmp(p, s, n) == 0) { p += n; return true; }
        return false;
    }
    void AppendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool ParseString(std::string& out) {
        if (p >= end || *p != '"') return false;
        p++;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                p++;
                if (p >= end) return false;
                char e = *p++;
                switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (end - p < 4) return false;
                    auto hex = [](char c) -> int {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        return -1;
                    };
                    int h[4] = {hex(p[0]), hex(p[1]), hex(p[2]), hex(p[3])};
                    if (h[0] < 0 || h[1] < 0 || h[2] < 0 || h[3] < 0) return false;
                    unsigned cp = (h[0] << 12) | (h[1] << 8) | (h[2] << 4) | h[3];
                    p += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        int g[4] = {hex(p[2]), hex(p[3]), hex(p[4]), hex(p[5])};
                        if (g[0] >= 0 && g[1] >= 0 && g[2] >= 0 && g[3] >= 0) {
                            unsigned lo = (g[0] << 12) | (g[1] << 8) | (g[2] << 4) | g[3];
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                p += 6;
                            }
                        }
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return false;
                }
            } else {
                out += *p++;
            }
        }
        if (p >= end) return false;
        p++; // closing quote
        return true;
    }
    ValuePtr ParseValue() {
        Ws();
        auto v = std::make_shared<Value>();
        if (p >= end) return nullptr;
        if (*p == '"') { v->type = Value::String; if (!ParseString(v->str)) return nullptr; return v; }
        if (*p == '{') {
            v->type = Value::Object; p++;
            Ws();
            if (p < end && *p == '}') { p++; return v; }
            while (p < end) {
                Ws();
                std::string k;
                if (!ParseString(k)) return nullptr;
                Ws();
                if (p >= end || *p != ':') return nullptr;
                p++;
                auto val = ParseValue();
                if (!val) return nullptr;
                v->obj[k] = val;
                Ws();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == '}') { p++; return v; }
                return nullptr;
            }
            return nullptr;
        }
        if (*p == '[') {
            v->type = Value::Array; p++;
            Ws();
            if (p < end && *p == ']') { p++; return v; }
            while (p < end) {
                auto val = ParseValue();
                if (!val) return nullptr;
                v->arr.push_back(val);
                Ws();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == ']') { p++; return v; }
                return nullptr;
            }
            return nullptr;
        }
        if (Lit("true")) { v->type = Value::Bool; v->b = true; return v; }
        if (Lit("false")) { v->type = Value::Bool; v->b = false; return v; }
        if (Lit("null")) { v->type = Value::Null; return v; }
        // number
        const char* s = p;
        if (p < end && (*p == '-' || *p == '+')) p++;
        bool digits = false;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '-' || *p == '+')) {
            if (*p >= '0' && *p <= '9') digits = true;
            p++;
        }
        if (!digits || p == s) return nullptr;
        v->type = Value::Number;
        v->num = atof(std::string(s, p).c_str());
        return v;
    }
};
} // namespace detail

inline ValuePtr Parse(const std::string& text) {
    detail::Parser ps{text.data(), text.data() + text.size()};
    return ps.ParseValue();
}

} // namespace mjson
