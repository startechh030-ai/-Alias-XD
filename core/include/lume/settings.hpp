#pragma once
// Hub-level settings. Deliberately not project state: this is "how this machine's hub
// behaves", and it is the only file in the hub root the user edits by hand.

#include <string>

#include "lume/fs.hpp"
#include "lume/json.hpp"

namespace lume {

constexpr const char* kSettingsFileName = "hub-settings.json";

struct HubSettings {
  int schema = 1;

  // Where the hub writes projects. Empty means fs::default_data_dir("Lume","hub").
  fs::Path projects_root;
  // Where `New project` templates are discovered. Empty means <repo>/templates.
  fs::Path templates_root;

  int max_recent = 24;
  bool confirm_delete = true;
  bool move_delete_to_trash = true;   // false => permanent removal
  bool show_autosave_badge = true;

  // Accent is the one theming knob we expose; the dark base is not optional.
  std::string accent = "teal";        // teal | amber | rose | violet
  std::string default_template = "empty-scene";
  std::string default_kind = "generic3d";

  // Sort/tab remembered between launches, so reopening the hub lands where you left it.
  std::string last_tab = "recent";
  std::string last_sort = "opened";

  static fs::Path default_path();
  static HubSettings load(const fs::Path& file);
  bool save(const fs::Path& file, std::string* err = nullptr) const;

  json::Value to_json() const;
  void from_json(const json::Value& v);
};

}  // namespace lume
