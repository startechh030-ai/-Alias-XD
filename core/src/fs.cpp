#include "lume/fs.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "lume/log.hpp"

namespace lume::fs {
namespace {

namespace stdfs = std::filesystem;

std::string env_str(const char* name) {
  const char* v = std::getenv(name);
  return v ? std::string(v) : std::string();
}

bool copy_recursive(const stdfs::path& from, const stdfs::path& to, std::string* err) {
  std::error_code ec;
  if (!stdfs::exists(from, ec)) {
    if (err) *err = "missing source: " + from.string();
    return false;
  }
  if (stdfs::is_directory(from, ec)) {
    stdfs::create_directories(to, ec);
    if (ec) {
      if (err) *err = "cannot create " + to.string() + ": " + ec.message();
      return false;
    }
    for (const auto& entry : stdfs::directory_iterator(from, ec)) {
      if (!copy_recursive(entry.path(), to / entry.path().filename(), err)) return false;
    }
    return true;
  }
  stdfs::create_directories(to.parent_path(), ec);
  stdfs::copy_file(from, to, stdfs::copy_options::overwrite_existing, ec);
  if (ec) {
    if (err) *err = "cannot copy " + from.string() + ": " + ec.message();
    return false;
  }
  return true;
}

}  // namespace

bool exists(const Path& p) {
  std::error_code ec;
  return stdfs::exists(p, ec);
}

bool is_directory(const Path& p) {
  std::error_code ec;
  return stdfs::is_directory(p, ec);
}

bool read_text(const Path& p, std::string* out, std::string* err) {
  if (!out) return false;
  std::error_code ec;
  if (!stdfs::is_regular_file(p, ec)) {
    if (err) *err = "not a file: " + p.string();
    return false;
  }
  // One read of the whole file: project.json is a few KB, a scene is not read here.
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) {
    if (err) *err = "cannot open " + p.string();
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::string buf;
  if (size > 0) {
    buf.resize(static_cast<std::size_t>(size));
    const std::size_t got = std::fread(&buf[0], 1, static_cast<std::size_t>(size), f);
    buf.resize(got);
  }
  std::fclose(f);
  *out = std::move(buf);
  return true;
}

bool write_text(const Path& p, const std::string& text, std::string* err) {
  std::error_code ec;
  if (!p.parent_path().empty()) {
    stdfs::create_directories(p.parent_path(), ec);
    if (ec) {
      if (err) *err = "cannot create " + p.parent_path().string() + ": " + ec.message();
      return false;
    }
  }
  // Write-then-rename keeps a crash mid-save from truncating a manifest to nothing.
  const Path tmp = p.string() + ".tmp";
  std::FILE* f = std::fopen(tmp.string().c_str(), "wb");
  if (!f) {
    if (err) *err = "cannot write " + tmp.string();
    return false;
  }
  const bool wrote = text.empty() || std::fwrite(text.data(), 1, text.size(), f) == text.size();
  std::fclose(f);
  if (!wrote) {
    std::remove(tmp.string().c_str());
    if (err) *err = "short write to " + tmp.string();
    return false;
  }
  stdfs::rename(tmp, p, ec);
  if (ec) {
    // Rename fails across volumes / against a locked file: fall back to copy.
    std::error_code copy_ec;
    stdfs::copy_file(tmp, p, stdfs::copy_options::overwrite_existing, copy_ec);
    std::error_code rm_ec;
    stdfs::remove(tmp, rm_ec);  // best-effort cleanup of the temp file
    if (copy_ec) {
      if (err) *err = "cannot save " + p.string() + ": " + copy_ec.message();
      return false;
    }
  }
  return true;
}

bool create_directories(const Path& p, std::string* err) {
  std::error_code ec;
  stdfs::create_directories(p, ec);
  if (ec) {
    if (err) *err = "cannot create " + p.string() + ": " + ec.message();
    return false;
  }
  return true;
}

std::vector<Path> list_directories(const Path& dir) {
  std::vector<Path> out;
  std::error_code ec;
  if (!stdfs::is_directory(dir, ec)) return out;
  for (const auto& entry : stdfs::directory_iterator(dir, ec)) {
    if (entry.is_directory(ec)) out.push_back(entry.path());
  }
  std::sort(out.begin(), out.end(), [](const Path& a, const Path& b) {
    return a.filename().string() < b.filename().string();
  });
  return out;
}

std::vector<Path> list_files(const Path& dir, const char* extension) {
  std::vector<Path> out;
  std::error_code ec;
  if (!stdfs::is_directory(dir, ec)) return out;
  std::string want;
  if (extension) {
    want = extension;
    std::transform(want.begin(), want.end(), want.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  }
  for (const auto& entry : stdfs::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file(ec)) continue;
    if (!want.empty()) {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (ext != want) continue;
    }
    out.push_back(entry.path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool remove_file(const Path& p, std::string* err) {
  std::error_code ec;
  stdfs::remove(p, ec);
  if (ec) {
    if (err) *err = "cannot delete " + p.string() + ": " + ec.message();
    return false;
  }
  return true;
}

bool copy_directory(const Path& from, const Path& to, std::string* err) {
  return copy_recursive(from, to, err);
}

bool remove_directory(const Path& p, std::string* err) {
  std::error_code ec;
  stdfs::remove_all(p, ec);
  if (ec) {
    if (err) *err = "cannot delete " + p.string() + ": " + ec.message();
    return false;
  }
  return true;
}

std::string stem(const Path& p) { return p.stem().string(); }

std::string extension(const Path& p) {
  std::string e = p.extension().string();
  std::transform(e.begin(), e.end(), e.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return e;
}

std::string to_generic_string(const Path& p) {
  std::string out;
  for (const auto& part : p) {
    if (!out.empty() && out.back() != '/') out.push_back('/');
    out += part.string();
  }
  return out;
}

std::string slugify(const std::string& name) {
  std::string out;
  bool pending_dash = false;
  for (unsigned char c : name) {
    if (std::isalnum(c)) {
      if (pending_dash && !out.empty()) out.push_back('-');
      pending_dash = false;
      out.push_back(static_cast<char>(std::tolower(c)));
    } else if (c == '_' || c == '-' || c == ' ' || c == '.') {
      pending_dash = !out.empty();
    }
  }
  while (pending_dash && !out.empty() && out.back() == '-') {
    out.pop_back();
    pending_dash = false;
  }
  return out;
}

Path unique_path(const Path& dir, const std::string& stem_text, const std::string& ext) {
  Path candidate = dir / (stem_text + ext);
  for (int n = 2; exists(candidate); ++n) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "-%d", n);
    candidate = dir / (stem_text + buf + ext);
    if (n > 9999) break;
  }
  return candidate;
}

std::int64_t now_unix() {
  const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());
  return static_cast<std::int64_t>(secs.count());
}

std::string format_utc(std::int64_t unix_seconds) {
  if (unix_seconds <= 0) return "1970-01-01T00:00:00Z";
  const std::time_t t = static_cast<std::time_t>(unix_seconds);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

std::uint64_t hash64(const std::string& text) {
  std::uint64_t h = 1469598103934665603ull;
  for (unsigned char c : text) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h ? h : 1u;
}

std::string hex(std::uint64_t value, unsigned digits) {
  static const char* kDigits = "0123456789abcdef";
  std::string out(digits, '0');
  for (unsigned i = 0; i < digits; ++i) {
    out[digits - 1 - i] = kDigits[(value >> (4u * i)) & 0xFu];
  }
  return out;
}

Path default_data_dir(const char* org, const char* app) {
#if defined(__ANDROID__)
  const std::string data_dir = env_str("LUME_DATA_DIR");
  if (!data_dir.empty()) return Path(data_dir);
  return Path(".");  // JNI layer sets LUME_DATA_DIR to the app-private dir
#elif defined(_WIN32)
  std::string base = env_str("LOCALAPPDATA");
  if (base.empty()) base = env_str("USERPROFILE") + "\\AppData\\Local";
  if (base.empty()) base = ".";
  return Path(base) / org / app;
#else
  std::string base = env_str("XDG_DATA_HOME");
  if (base.empty()) {
    const std::string home = env_str("HOME");
    base = home.empty() ? std::string(".") : home + "/.local/share";
  }
  return Path(base) / org / app;
#endif
}

Path documents_dir() {
#if defined(__ANDROID__)
  const std::string ext = env_str("LUME_EXTERNAL_DIR");
  return ext.empty() ? Path(".") : Path(ext);
#elif defined(_WIN32)
  const std::string up = env_str("USERPROFILE");
  return up.empty() ? Path(".") : Path(up) / "Documents";
#else
  const std::string home = env_str("HOME");
  if (home.empty()) return Path(".");
  const Path docs = Path(home) / "Documents";
  return exists(docs) ? docs : Path(home);
#endif
}

}  // namespace lume::fs
