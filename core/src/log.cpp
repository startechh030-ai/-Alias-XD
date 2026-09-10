#include "lume/log.hpp"

#include <cstdio>

namespace lume::log {
namespace {

Level g_min_level = Level::Info;
bool g_stderr = true;
void (*g_sink)(Level, const std::string&) = nullptr;

const char* kNames[] = {"debug", "info", "warn", "error"};

}  // namespace

void set_min_level(Level level) { g_min_level = level; }
Level min_level() { return g_min_level; }
void set_stderr(bool enabled) { g_stderr = enabled; }
void set_sink(void (*sink)(Level, const std::string&)) { g_sink = sink; }

const char* to_string(Level level) {
  const int i = static_cast<int>(level);
  return (i >= 0 && i <= 3) ? kNames[i] : "?";
}

void write(Level level, const std::string& message) {
  if (static_cast<int>(level) < static_cast<int>(g_min_level)) return;
  if (message.empty()) return;
  if (g_sink) g_sink(level, message);
  if (g_stderr) {
    std::fprintf(stderr, "[lume %s] %s\n", to_string(level), message.c_str());
  }
}

}  // namespace lume::log
