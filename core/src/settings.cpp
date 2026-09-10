#include "lume/settings.hpp"

#include <algorithm>

#include "lume/log.hpp"

namespace lume {
namespace {

const char* kAccents[] = {"teal", "amber", "rose", "violet"};
const char* kTabs[] = {"recent", "projects", "templates", "community"};
const char* kSorts[] = {"opened", "name", "modified", "created"};

template <std::size_t N>
std::string one_of(const std::string& value, const char* (&options)[N], const char* fallback) {
  for (const char* o : options) {
    if (value == o) return value;
  }
  LUME_LOG_WARN << "ignoring unknown hub setting \"" << value << "\", using \"" << fallback
                << "\"";
  return fallback;
}

}  // namespace

fs::Path HubSettings::default_path() {
  return fs::default_data_dir("Lume", "hub") / kSettingsFileName;
}

json::Value HubSettings::to_json() const {
  json::Value v = json::Value::make_object();
  v.set("schema", json::Value(static_cast<std::int64_t>(schema)));
  v.set("projectsRoot", json::Value(fs::to_generic_string(projects_root)));
  v.set("templatesRoot", json::Value(fs::to_generic_string(templates_root)));
  v.set("maxRecent", json::Value(static_cast<std::int64_t>(max_recent)));
  v.set("confirmDelete", json::Value(confirm_delete));
  v.set("moveDeleteToTrash", json::Value(move_delete_to_trash));
  v.set("showAutosaveBadge", json::Value(show_autosave_badge));
  v.set("accent", json::Value(accent));
  v.set("defaultTemplate", json::Value(default_template));
  v.set("defaultKind", json::Value(default_kind));
  v.set("lastTab", json::Value(last_tab));
  v.set("lastSort", json::Value(last_sort));
  return v;
}

void HubSettings::from_json(const json::Value& v) {
  if (!v.is_object()) return;
  schema = static_cast<int>(json::get_int(v, "schema", schema));
  const std::string pr = json::get_string(v, "projectsRoot");
  if (!pr.empty()) projects_root = fs::Path(pr);
  const std::string tr = json::get_string(v, "templatesRoot");
  if (!tr.empty()) templates_root = fs::Path(tr);
  max_recent = static_cast<int>(json::get_int(v, "maxRecent", max_recent));
  max_recent = std::clamp(max_recent, 4, 256);
  confirm_delete = json::get_bool(v, "confirmDelete", confirm_delete);
  move_delete_to_trash = json::get_bool(v, "moveDeleteToTrash", move_delete_to_trash);
  show_autosave_badge = json::get_bool(v, "showAutosaveBadge", show_autosave_badge);
  accent = one_of(json::get_string(v, "accent", accent), kAccents, "teal");
  default_template = json::get_string(v, "defaultTemplate", default_template);
  default_kind = json::get_string(v, "defaultKind", default_kind);
  last_tab = one_of(json::get_string(v, "lastTab", last_tab), kTabs, "recent");
  last_sort = one_of(json::get_string(v, "lastSort", last_sort), kSorts, "opened");
}

HubSettings HubSettings::load(const fs::Path& file) {
  HubSettings out;
  std::string text;
  std::string err;
  if (!fs::read_text(file, &text, &err)) {
    LUME_LOG_DEBUG << "no hub settings yet (" << file.string() << ")";
    return out;
  }
  json::Value v;
  if (!json::Value::parse(text, &v, &err)) {
    LUME_LOG_WARN << "hub settings unreadable, using defaults: " << err;
    return out;
  }
  out.from_json(v);
  return out;
}

bool HubSettings::save(const fs::Path& file, std::string* err) const {
  return fs::write_text(file, to_json().dump(2) + "\n", err);
}

}  // namespace lume
