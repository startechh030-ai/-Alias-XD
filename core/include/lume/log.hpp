#pragma once
// Minimal logging. The hub surfaces warnings in a status strip (step 2), so every message
// goes through here rather than straight to stderr.

#include <sstream>
#include <string>

namespace lume::log {

enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

void set_min_level(Level level);
Level min_level();
void set_stderr(bool enabled);      // tests turn this off; the hub keeps it on
void set_sink(void (*sink)(Level, const std::string&));  // hub status strip hooks here

const char* to_string(Level level);
void write(Level level, const std::string& message);

class Line {
 public:
  explicit Line(Level level) : level_(level) {}
  ~Line() { write(level_, stream_.str()); }

  template <typename T>
  Line& operator<<(const T& value) {
    stream_ << value;
    return *this;
  }

 private:
  Level level_;
  std::ostringstream stream_;
};

}  // namespace lume::log

#define LUME_LOG(level) ::lume::log::Line(::lume::log::Level::level)
#define LUME_LOG_DEBUG LUME_LOG(Debug)
#define LUME_LOG_INFO LUME_LOG(Info)
#define LUME_LOG_WARN LUME_LOG(Warn)
#define LUME_LOG_ERROR LUME_LOG(Error)
