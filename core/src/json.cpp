#include "lume/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace lume::json {
namespace {

const std::string kEmptyString;
const Value kNullValue{};

void escape_into(std::string* out, const std::string& s) {
  out->push_back('"');
  for (unsigned char c : s) {
    switch (c) {
      case '"': out->append("\\\""); break;
      case '\\': out->append("\\\\"); break;
      case '\n': out->append("\\n"); break;
      case '\r': out->append("\\r"); break;
      case '\t': out->append("\\t"); break;
      case '\b': out->append("\\b"); break;
      case '\f': out->append("\\f"); break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out->append(buf);
        } else {
          out->push_back(static_cast<char>(c));
        }
    }
  }
  out->push_back('"');
}

std::string format_number(double v) {
  if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 9.0e15) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
    return buf;
  }
  if (!std::isfinite(v)) return "0";  // never emit NaN/Inf into a project file
  char buf[40];
  std::snprintf(buf, sizeof buf, "%.10g", v);
  return buf;
}

void append_utf8(std::string* out, std::uint32_t cp) {
  if (cp < 0x80) {
    out->push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out->push_back(static_cast<char>(0xC0u | (cp >> 6)));
    out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  } else if (cp < 0x10000) {
    out->push_back(static_cast<char>(0xE0u | (cp >> 12)));
    out->push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
    out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  } else {
    out->push_back(static_cast<char>(0xF0u | (cp >> 18)));
    out->push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
    out->push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
    out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  }
}

void write_value(std::string* out, const Value& v, int indent, int depth) {
  const std::string pad(static_cast<std::size_t>(indent > 0 ? indent * (depth + 1) : 0), ' ');
  const std::string close_pad(static_cast<std::size_t>(indent > 0 ? indent * depth : 0), ' ');
  const char* nl = indent > 0 ? "\n" : "";
  const char* sp = indent > 0 ? " " : "";

  switch (v.type()) {
    case Type::Null: out->append("null"); break;
    case Type::Bool: out->append(v.as_bool() ? "true" : "false"); break;
    case Type::Number: out->append(format_number(v.as_number())); break;
    case Type::String: escape_into(out, v.as_string()); break;
    case Type::Array: {
      if (v.elements().empty()) { out->append("[]"); break; }
      out->append("[");
      out->append(nl);
      for (std::size_t i = 0; i < v.elements().size(); ++i) {
        out->append(pad);
        write_value(out, v.elements()[i], indent, depth + 1);
        if (i + 1 < v.elements().size()) out->append(",");
        out->append(nl);
      }
      out->append(close_pad);
      out->append("]");
      break;
    }
    case Type::Object: {
      if (v.members().empty()) { out->append("{}"); break; }
      out->append("{");
      out->append(nl);
      std::size_t i = 0;
      for (const Member& m : v.members()) {
        out->append(pad);
        escape_into(out, m.first);
        out->append(":");
        out->append(sp);
        write_value(out, m.second, indent, depth + 1);
        if (++i < v.members().size()) out->append(",");
        out->append(nl);
      }
      out->append(close_pad);
      out->append("}");
      break;
    }
  }
}

class Reader {
 public:
  explicit Reader(const std::string& text) : s_(text) {}

  bool fail(const char* what) {
    err_ = std::string(what) + " (offset " + std::to_string(i_) + ")";
    return false;
  }
  const std::string& err() const { return err_; }

  bool parse(Value* out) {
    skip_ws();
    if (!parse_value(out)) return false;
    skip_ws();
    if (i_ != s_.size()) return fail("trailing characters after value");
    return true;
  }

 private:
  void skip_ws() {
    while (i_ < s_.size()) {
      const char c = s_[i_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++i_;
      } else {
        break;
      }
    }
  }

  bool peek(char* c) const {
    if (i_ >= s_.size()) return false;
    *c = s_[i_];
    return true;
  }

  bool literal(const char* word) {
    const std::size_t n = std::strlen(word);
    if (i_ + n > s_.size()) return false;
    if (s_.compare(i_, n, word) != 0) return false;
    i_ += n;
    return true;
  }

  bool parse_value(Value* out) {
    char c = 0;
    if (!peek(&c)) return fail("unexpected end of input");
    switch (c) {
      case '{': return parse_object(out);
      case '[': return parse_array(out);
      case '"': {
        std::string str;
        if (!parse_string(&str)) return false;
        *out = Value(std::move(str));
        return true;
      }
      case 't':
        if (!literal("true")) return fail("bad literal");
        *out = Value(true);
        return true;
      case 'f':
        if (!literal("false")) return fail("bad literal");
        *out = Value(false);
        return true;
      case 'n':
        if (!literal("null")) return fail("bad literal");
        *out = Value();
        return true;
      default:
        return parse_number(out);
    }
  }

  bool parse_number(Value* out) {
    const char* start = s_.c_str() + i_;
    char* end = nullptr;
    const double v = std::strtod(start, &end);
    if (end == start) return fail("expected value");
    i_ += static_cast<std::size_t>(end - start);
    *out = Value(v);
    return true;
  }

  bool hex4(std::uint32_t* out) {
    if (i_ + 4 > s_.size()) return fail("short \\u escape");
    std::uint32_t v = 0;
    for (int k = 0; k < 4; ++k) {
      const char c = s_[i_++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
      else return fail("bad hex digit in \\u escape");
    }
    *out = v;
    return true;
  }

  bool parse_string(std::string* out) {
    if (s_[i_] != '"') return fail("expected \"");
    ++i_;
    out->clear();
    while (true) {
      if (i_ >= s_.size()) return fail("unterminated string");
      const char c = s_[i_++];
      if (c == '"') return true;
      if (c != '\\') {
        out->push_back(c);
        continue;
      }
      if (i_ >= s_.size()) return fail("bad escape");
      const char e = s_[i_++];
      switch (e) {
        case '"': out->push_back('"'); break;
        case '\\': out->push_back('\\'); break;
        case '/': out->push_back('/'); break;
        case 'b': out->push_back('\b'); break;
        case 'f': out->push_back('\f'); break;
        case 'n': out->push_back('\n'); break;
        case 'r': out->push_back('\r'); break;
        case 't': out->push_back('\t'); break;
        case 'u': {
          std::uint32_t cp = 0;
          if (!hex4(&cp)) return false;
          if (cp >= 0xD800u && cp <= 0xDBFFu) {  // high surrogate
            if (i_ + 2 > s_.size() || s_[i_] != '\\' || s_[i_ + 1] != 'u') {
              return fail("lone surrogate");
            }
            i_ += 2;
            std::uint32_t lo = 0;
            if (!hex4(&lo)) return false;
            if (lo < 0xDC00u || lo > 0xDFFFu) return fail("bad low surrogate");
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
          }
          append_utf8(out, cp);
          break;
        }
        default: return fail("unknown escape");
      }
    }
  }

  bool expect(char c) {
    skip_ws();
    if (i_ >= s_.size() || s_[i_] != c) {
      const std::string msg = std::string("expected '") + c + "'";
      return fail(msg.c_str());
    }
    ++i_;
    return true;
  }

  bool parse_array(Value* out) {
    if (!expect('[')) return false;
    Array items;
    skip_ws();
    if (peek() && s_[i_] == ']') { ++i_; *out = Value(std::move(items)); return true; }
    while (true) {
      Value item;
      skip_ws();
      if (!parse_value(&item)) return false;
      items.push_back(std::move(item));
      skip_ws();
      char c = 0;
      if (!peek(&c)) return fail("unterminated array");
      if (c == ',') { ++i_; continue; }
      if (c == ']') { ++i_; break; }
      return fail("expected , or ] in array");
    }
    *out = Value(std::move(items));
    return true;
  }

  bool parse_object(Value* out) {
    if (!expect('{')) return false;
    Object members;
    skip_ws();
    if (peek() && s_[i_] == '}') { ++i_; *out = Value(std::move(members)); return true; }
    while (true) {
      skip_ws();
      if (!peek() || s_[i_] != '"') return fail("expected object key");
      std::string key;
      if (!parse_string(&key)) return false;
      if (!expect(':')) return false;
      Value value;
      skip_ws();
      if (!parse_value(&value)) return false;
      members.emplace_back(std::move(key), std::move(value));
      skip_ws();
      char c = 0;
      if (!peek(&c)) return fail("unterminated object");
      if (c == ',') { ++i_; continue; }
      if (c == '}') { ++i_; break; }
      return fail("expected , or } in object");
    }
    *out = Value(std::move(members));
    return true;
  }

  bool peek() const { return i_ < s_.size(); }

  const std::string& s_;
  std::size_t i_ = 0;
  std::string err_;
};

}  // namespace

bool Value::as_bool(bool fallback) const { return type_ == Type::Bool ? bool_ : fallback; }
double Value::as_number(double fallback) const { return type_ == Type::Number ? num_ : fallback; }
std::int64_t Value::as_int(std::int64_t fallback) const {
  if (type_ == Type::Number) return static_cast<std::int64_t>(num_);
  if (type_ == Type::Bool) return bool_ ? 1 : 0;
  return fallback;
}
float Value::as_float(float fallback) const {
  return type_ == Type::Number ? static_cast<float>(num_) : fallback;
}
const std::string& Value::as_string() const { return type_ == Type::String ? str_ : kEmptyString; }

bool Value::has(const std::string& key) const {
  for (const Member& m : obj_) {
    if (m.first == key) return true;
  }
  return false;
}

const Value& Value::at(const std::string& key) const {
  for (const Member& m : obj_) {
    if (m.first == key) return m.second;
  }
  return kNullValue;
}

Value& Value::operator[](const std::string& key) {
  if (type_ == Type::Null) {
    type_ = Type::Object;
  }
  if (type_ != Type::Object) {
    static Value scratch;  // calling [] on a non-object is a programmer error, not data
    return scratch;
  }
  for (Member& m : obj_) {
    if (m.first == key) return m.second;
  }
  obj_.emplace_back(key, Value{});
  return obj_.back().second;
}

void Value::set(const std::string& key, Value value) {
  if (type_ != Type::Object) {
    type_ = Type::Object;
    arr_.clear();
    str_.clear();
  }
  for (Member& m : obj_) {
    if (m.first == key) { m.second = std::move(value); return; }
  }
  obj_.emplace_back(key, std::move(value));
}

void Value::erase(const std::string& key) {
  for (std::size_t i = 0; i < obj_.size(); ++i) {
    if (obj_[i].first == key) {
      obj_.erase(obj_.begin() + static_cast<std::ptrdiff_t>(i));
      return;
    }
  }
}

std::size_t Value::size() const {
  if (type_ == Type::Array) return arr_.size();
  if (type_ == Type::Object) return obj_.size();
  return 0;
}

void Value::push_back(Value value) {
  if (type_ != Type::Array) {
    type_ = Type::Array;
    obj_.clear();
    str_.clear();
  }
  arr_.push_back(std::move(value));
}

std::string Value::dump(int indent) const {
  std::string out;
  write_value(&out, *this, indent, 0);
  return out;
}

std::string Value::dump_compact() const { return dump(0); }

bool Value::parse(const std::string& text, Value* out, std::string* err) {
  if (!out) return false;
  Reader r(text);
  if (!r.parse(out)) {
    if (err) *err = r.err();
    return false;
  }
  return true;
}

std::string get_string(const Value& obj, const std::string& key, const std::string& fallback) {
  const Value& v = obj.at(key);
  return v.is_string() ? v.as_string() : fallback;
}

std::int64_t get_int(const Value& obj, const std::string& key, std::int64_t fallback) {
  const Value& v = obj.at(key);
  return v.is_number() ? v.as_int(fallback) : fallback;
}

double get_number(const Value& obj, const std::string& key, double fallback) {
  const Value& v = obj.at(key);
  return v.is_number() ? v.as_number(fallback) : fallback;
}

bool get_bool(const Value& obj, const std::string& key, bool fallback) {
  const Value& v = obj.at(key);
  return v.is_bool() ? v.as_bool(fallback) : fallback;
}

std::vector<std::string> get_string_array(const Value& obj, const std::string& key) {
  std::vector<std::string> out;
  const Value& v = obj.at(key);
  if (!v.is_array()) return out;
  for (const Value& e : v.elements()) {
    if (e.is_string()) out.push_back(e.as_string());
  }
  return out;
}

void set_string_array(Value& obj, const std::string& key, const std::vector<std::string>& values) {
  Array arr;
  arr.reserve(values.size());
  for (const std::string& s : values) arr.push_back(Value(s));
  obj.set(key, Value(std::move(arr)));
}

std::string quote(const std::string& text) {
  std::string out;
  escape_into(&out, text);
  return out;
}

}  // namespace lume::json
