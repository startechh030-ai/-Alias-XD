#pragma once
// Filesystem helpers with a uniform, non-throwing error convention:
// every fallible call takes `std::string* err` and returns false, so the hub can show the
// reason verbatim instead of collapsing failures into exceptions.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lume::fs {

using Path = std::filesystem::path;

bool exists(const Path& p);
bool is_directory(const Path& p);

// Reads bytes as text. Returns false if the file is missing or unreadable.
bool read_text(const Path& p, std::string* out, std::string* err = nullptr);
// Writes text, creating parent directories as needed. Returns false on failure.
bool write_text(const Path& p, const std::string& text, std::string* err = nullptr);

bool create_directories(const Path& p, std::string* err = nullptr);
// Sorted, directories first. `out` receives full paths.
std::vector<Path> list_directories(const Path& dir);
std::vector<Path> list_files(const Path& dir, const char* extension = nullptr);

bool remove_file(const Path& p, std::string* err = nullptr);
bool copy_directory(const Path& from, const Path& to, std::string* err = nullptr);
bool remove_directory(const Path& p, std::string* err = nullptr);

std::string stem(const Path& p);      // filename without extension
std::string extension(const Path& p); // ".json", lowercased
std::string to_generic_string(const Path& p);  // '/' separators, for JSON/portable paths

// "My Level!! 2" -> "my-level-2". Used for folder names, never for the display name.
std::string slugify(const std::string& name);

// "<dir>/<stem><ext>", then "<dir>/<stem>-2<ext>", -3, ... without touching the filesystem
// unless a candidate exists.
Path unique_path(const Path& dir, const std::string& stem, const std::string& ext);

std::int64_t now_unix();
std::string format_utc(std::int64_t unix_seconds);  // "2026-09-10T08:31:02Z"

std::uint64_t hash64(const std::string& text);       // FNV-1a, for stable project ids
std::string hex(std::uint64_t value, unsigned digits = 8);

// Per-user writable root: %LOCALAPPDATA% on Windows, $XDG_DATA_HOME or ~/.local/share on
// Linux, the app-private dir injected by JNI on Android (defaults to "." if unset).
Path default_data_dir(const char* org, const char* app);
Path documents_dir();

}  // namespace lume::fs
