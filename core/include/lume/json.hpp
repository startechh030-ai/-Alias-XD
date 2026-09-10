#pragma once
// A deliberately small JSON reader/writer.
//
// Why hand-rolled: `lume-core` has to compile on a bare runner (and on the NDK) with zero
// dependencies, and the project system needs only objects, arrays, strings, numbers, bools.
// The moment step 3 pulls in Assimp/glTF we gain a full-featured parser and this file can
// shrink, but the *format* stays ours.
//
// Guarantees: object key order is preserved (matters for readable project.json diffs),
// unknown keys round-trip through Value untouched, and parsing never throws.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lume::json {

enum class Type { Null, Bool, Number, String, Array, Object };

class Value;
using Array = std::vector<Value>;
using Member = std::pair<std::string, Value>;
using Object = std::vector<Member>;

class Value {
 public:
  Value() = default;
  Value(bool v) : type_(Type::Bool), bool_(v) {}
  Value(std::int64_t v) : type_(Type::Number), num_(static_cast<double>(v)) {}
  Value(int v) : type_(Type::Number), num_(static_cast<double>(v)) {}
  Value(double v) : type_(Type::Number), num_(v) {}
  Value(float v) : type_(Type::Number), num_(static_cast<double>(v)) {}
  Value(const char* v) : type_(Type::String), str_(v ? v : "") {}
  Value(std::string v) : type_(Type::String), str_(std::move(v)) {}
  Value(Array v) : type_(Type::Array), arr_(std::move(v)) {}
  Value(Object v) : type_(Type::Object), obj_(std::move(v)) {}

  static Value make_object() { return Value(Object{}); }
  static Value make_array() { return Value(Array{}); }

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_bool() const { return type_ == Type::Bool; }
  bool is_number() const { return type_ == Type::Number; }
  bool is_string() const { return type_ == Type::String; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_object() const { return type_ == Type::Object; }

  // Readers never throw and never fabricate: a missing key yields the caller's default.
  bool as_bool(bool fallback = false) const;
  double as_number(double fallback = 0.0) const;
  std::int64_t as_int(std::int64_t fallback = 0) const;
  float as_float(float fallback = 0.0f) const;
  const std::string& as_string() const;

  // Object access. `at` returns a shared Null value; `operator[]` converts *this to an
  // object if it is still Null and inserts the key.
  bool has(const std::string& key) const;
  const Value& at(const std::string& key) const;
  Value& operator[](const std::string& key);
  void set(const std::string& key, Value value);
  void erase(const std::string& key);
  const Object& members() const { return obj_; }

  // Array access.
  std::size_t size() const;
  void push_back(Value value);
  const Array& elements() const { return arr_; }
  Array& elements() { return arr_; }

  std::string dump(int indent = 2) const;
  std::string dump_compact() const;

  // Returns false and fills `err` (never throws) on malformed input.
  static bool parse(const std::string& text, Value* out, std::string* err);

 private:
  Type type_ = Type::Null;
  bool bool_ = false;
  double num_ = 0.0;
  std::string str_;
  Array arr_;
  Object obj_;
};

// Convenience lookups used throughout the project system.
std::string get_string(const Value& obj, const std::string& key, const std::string& fallback = "");
std::int64_t get_int(const Value& obj, const std::string& key, std::int64_t fallback = 0);
double get_number(const Value& obj, const std::string& key, double fallback = 0.0);
bool get_bool(const Value& obj, const std::string& key, bool fallback = false);
std::vector<std::string> get_string_array(const Value& obj, const std::string& key);
void set_string_array(Value& obj, const std::string& key, const std::vector<std::string>& values);

// Escapes a string as a JSON literal (with quotes). Used by the hub's text export.
std::string quote(const std::string& text);

}  // namespace lume::json
