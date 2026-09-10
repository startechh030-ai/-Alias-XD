#pragma once
// The smallest test harness that is worth its bytes: CHECK + a scratch directory + a report.
// No gtest, because step 1 promises a dependency-free core and ctest drives this fine.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <type_traits>
#include <string>

namespace lume::test {

inline int& fail_count() {
  static int n = 0;
  return n;
}
inline int& check_count() {
  static int n = 0;
  return n;
}

template <typename T>
std::string to_str(const T& v) {
  if constexpr (std::is_same_v<T, bool>) {
    return v ? "true" : "false";
  } else if constexpr (std::is_enum_v<T>) {
    return std::to_string(static_cast<long long>(v));
  } else if constexpr (std::is_arithmetic_v<T>) {
    return std::to_string(v);
  } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
    return v.string();
  } else if constexpr (std::is_convertible_v<T, std::string>) {
    return std::string(v);
  } else {
    return "(unprinted)";
  }
}

inline void report(const char* file, int line, const std::string& what, bool ok) {
  ++check_count();
  if (ok) return;
  ++fail_count();
  const char* base = file;
  for (const char* p = file; *p; ++p) {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  std::printf("  FAIL %s:%d  %s\n", base, line, what.c_str());
}

// One scratch hub root per test binary, removed on exit. Tests never touch a real profile.
class Scratch {
 public:
  explicit Scratch(const std::string& tag) {
    std::error_code ec;
    const auto tick = static_cast<unsigned long>(
        std::chrono::steady_clock::now().time_since_epoch().count() & 0xffffffUL);
    root_ = std::filesystem::temp_directory_path(ec) /
            ("lume-test-" + tag + "-" + std::to_string(tick));
    std::filesystem::remove_all(root_, ec);
    std::filesystem::create_directories(root_, ec);
  }
  ~Scratch() {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }
  Scratch(const Scratch&) = delete;
  Scratch& operator=(const Scratch&) = delete;
  const std::filesystem::path& root() const { return root_; }

 private:
  std::filesystem::path root_;
};

inline int finish(const char* name) {
  if (fail_count() == 0) {
    std::printf("%s: %d checks, all passed\n", name, check_count());
    return 0;
  }
  std::printf("%s: %d of %d checks FAILED\n", name, fail_count(), check_count());
  return 1;
}

}  // namespace lume::test

#define CHECK(cond) ::lume::test::report(__FILE__, __LINE__, #cond, (cond))

#define CHECK_MSG(cond, msg) ::lume::test::report(__FILE__, __LINE__, (msg), static_cast<bool>(cond))

#define CHECK_EQ(a, b)                                                            \
  do {                                                                            \
    const auto&& lume_va = (a);                                                   \
    const auto&& lume_vb = (b);                                                   \
    ::lume::test::report(__FILE__, __LINE__,                                      \
                         std::string(#a) + " == " + #b + "  got [" +             \
                             ::lume::test::to_str(lume_va) + "] vs [" +           \
                             ::lume::test::to_str(lume_vb) + "]",                 \
                         lume_va == lume_vb);                                     \
  } while (0)

#define CHECK_STREQ(a, b)                                                         \
  do {                                                                            \
    const std::string lume_sa = ::lume::test::to_str(a);                          \
    const std::string lume_sb = ::lume::test::to_str(b);                          \
    ::lume::test::report(__FILE__, __LINE__,                                       \
                         std::string(#a) + " == " + #b + "  got [" + lume_sa +    \
                             "] vs [" + lume_sb + "]",                            \
                         lume_sa == lume_sb);                                       \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                                     \
  do {                                                                            \
    const double lume_va = static_cast<double>(a);                                \
    const double lume_vb = static_cast<double>(b);                                \
    ::lume::test::report(__FILE__, __LINE__,                                      \
                         std::string(#a) + " ~= " + #b,                          \
                         (lume_va - lume_vb) < (eps) && (lume_vb - lume_va) < (eps)); \
  } while (0)
