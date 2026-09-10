#include "hub_state.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "lume/log.hpp"
#include "theme.hpp"

namespace lume::hub {
namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool contains(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) return true;
  return haystack.find(needle) != std::string::npos;
}

// "2026-09-10T08:31:02Z" -> unix seconds. Civil-from-days by Howard Hinnant's algorithm;
// we avoid timegm because it is a GNU extension and the hub builds on MSVC too.
std::int64_t parse_utc(const std::string& iso) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) < 3) return 0;
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const std::int64_t yoe = y - era * 400;
  const std::int64_t mp = mo + (mo > 2 ? -3 : 9);
  const std::int64_t doy = (153 * mp + 2) / 5 + d - 1;
  const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const std::int64_t days = era * 146097 + doe - 719468;
  return days * 86400 + static_cast<std::int64_t>(h) * 3600 +
         static_cast<std::int64_t>(mi) * 60 + s;
}

std::string kind_label(const std::string& kind) {
  if (kind == "model-import") return "model";
  if (kind == "physics-lab") return "physics";
  if (kind == "animation-test") return "anim";
  if (kind == "voxel-kit") return "voxel";
  return "scene";
}

std::string join(const std::vector<std::string>& parts, const char* sep) {
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i) out += sep;
    out += parts[i];
  }
  return out;
}

}  // namespace

HubState::HubState(ProjectManager* manager, const fs::Path& templates_root)
    : pm_(manager), templates_(templates_root) {
  if (pm_) {
    Tab t = Tab::Recent;
    if (tab_from_id(pm_->settings().last_tab, &t)) set_tab(t);
    SortBy s = SortBy::LastOpened;
    if (sort_from_id(pm_->settings().last_sort, &s)) sort_ = s;
  }
  refresh();
}

const char* HubState::tab_label(Tab tab) {
  switch (tab) {
    case Tab::Recent: return "Recent";
    case Tab::Projects: return "My Projects";
    case Tab::Templates: return "Start";
    case Tab::Community: return "Community";
  }
  return "?";
}

const char* HubState::tab_id(Tab tab) {
  switch (tab) {
    case Tab::Recent: return "recent";
    case Tab::Projects: return "projects";
    case Tab::Templates: return "templates";
    case Tab::Community: return "community";
  }
  return "recent";
}

bool HubState::tab_from_id(const std::string& id, Tab* out) {
  if (!out) return false;
  for (Tab t : {Tab::Recent, Tab::Projects, Tab::Templates, Tab::Community}) {
    if (id == tab_id(t)) {
      *out = t;
      return true;
    }
  }
  return false;
}

const char* HubState::sort_label(SortBy sort) {
  switch (sort) {
    case SortBy::LastOpened: return "Recently opened";
    case SortBy::Name: return "Name (A-Z)";
    case SortBy::LastModified: return "Last edited";
    case SortBy::Created: return "Created";
  }
  return "?";
}

bool HubState::sort_from_id(const std::string& id, SortBy* out) {
  if (!out) return false;
  if (id == "opened") *out = SortBy::LastOpened;
  else if (id == "name") *out = SortBy::Name;
  else if (id == "modified") *out = SortBy::LastModified;
  else if (id == "created") *out = SortBy::Created;
  else return false;
  return true;
}

std::vector<Card> HubState::project_cards(const std::vector<ProjectRef>& refs, std::int64_t now) {
  std::vector<Card> out;
  out.reserve(refs.size());
  for (const ProjectRef& ref : refs) {
    Card c;
    c.kind = CardKind::Project;
    c.id = ref.manifest.id;
    c.title = ref.manifest.name;
    c.subtitle = "Edited " + ProjectManager::humanize_age(parse_utc(ref.manifest.modified_utc), now);
    c.note = ref.folder_name();
    c.badge = ref.has_autosave ? "autosave"
              : (ref.manifest.kind != "generic3d" ? kind_label(ref.manifest.kind) : "");
    c.thumbnail = ref.has_thumbnail ? fs::to_generic_string(ref.thumbnail_path()) : "";
    c.manifest_path = ref.manifest_path;
    c.missing = !ref.valid() || !fs::is_directory(ref.dir);
    c.has_autosave = ref.has_autosave;
    c.opened_at = ref.last_opened;
    c.modified_at = parse_utc(ref.manifest.modified_utc);
    c.created_at = parse_utc(ref.manifest.created_utc);
    c.tint = theme::card_tint(fs::hash64(c.id));
    c.query_blob = lower(join({c.title, ref.manifest.description, ref.manifest.kind,
                               ref.manifest.author, join(ref.manifest.tags, " "), c.note}, " "));
    out.push_back(std::move(c));
  }
  return out;
}

std::vector<Card> HubState::template_cards(const std::vector<ProjectTemplate>& templates) {
  std::vector<Card> out;
  out.reserve(templates.size());
  for (const ProjectTemplate& t : templates) {
    Card c;
    c.kind = CardKind::Template;
    c.id = t.id;
    c.title = t.name;
    c.subtitle = t.description;
    c.note = t.seeds_assets() ? "copies " + std::to_string(t.copy.size()) + " item(s)" : "starts empty";
    c.badge = kind_label(t.kind);
    c.template_dir = t.dir;
    c.tint = theme::card_tint(fs::hash64(t.id));
    c.query_blob = lower(join({t.name, t.description, t.kind, t.id, join(t.tags, " ")}, " "));
    out.push_back(std::move(c));
  }
  return out;
}

bool HubState::matches(const Card& card, const std::string& needle) {
  return contains(card.query_blob, lower(needle));
}

void HubState::rebuild() {
  if (!pm_) return;
  const std::int64_t now = fs::now_unix();
  recents_ = pm_->recent();
  projects_ = pm_->scan();
  templates_ = templates_.root().empty() ? TemplateLibrary::builtin() : templates_.discover();

  cards_.clear();
  switch (tab_) {
    case Tab::Recent: {
      cards_ = project_cards(recents_, now);
      if (cards_.empty()) {
        Card c;
        c.kind = CardKind::Notice;
        c.title = "Nothing opened yet";
        c.subtitle = "Projects you open land here first.";
        c.note = pm_->projects_dir().string();
        c.actionable = false;
        c.query_blob = lower(c.title + " " + c.subtitle);
        cards_.push_back(std::move(c));
      }
      break;
    }
    case Tab::Projects:
      cards_ = project_cards(projects_, now);
      break;
    case Tab::Templates:
      cards_ = template_cards(templates_);
      break;
    case Tab::Community: {
      Card c;
      c.kind = CardKind::Notice;
      c.title = "Community projects";
      c.subtitle = "Browse and fork shared scenes. Needs an account, a server, and a step number.";
      c.note = "planned: step 9";
      c.actionable = false;
      c.query_blob = "community browse fork shared";
      cards_.push_back(std::move(c));
      break;
    }
  }
  apply_view();
}

void HubState::apply_view() {
  view_.clear();
  view_.reserve(cards_.size());
  for (const Card& c : cards_) {
    if (!query_.empty() && !matches(c, query_)) continue;
    view_.push_back(c);
  }
  auto cmp_time = [](std::int64_t a, std::int64_t b) { return a > b; };
  switch (sort_) {
    case SortBy::Name:
      std::stable_sort(view_.begin(), view_.end(), [](const Card& a, const Card& b) {
        return lower(a.title) < lower(b.title);
      });
      break;
    case SortBy::LastModified:
      std::stable_sort(view_.begin(), view_.end(),
                       [&](const Card& a, const Card& b) { return cmp_time(a.modified_at, b.modified_at); });
      break;
    case SortBy::Created:
      std::stable_sort(view_.begin(), view_.end(),
                       [&](const Card& a, const Card& b) { return cmp_time(a.created_at, b.created_at); });
      break;
    case SortBy::LastOpened:
      std::stable_sort(view_.begin(), view_.end(),
                       [&](const Card& a, const Card& b) { return cmp_time(a.opened_at, b.opened_at); });
      break;
  }
  // Notices and templates keep their authored order: sort keys are 0 for them, so pin them
  // by kind when the user is on a tab where they are the whole content.
  if (tab_ == Tab::Templates || tab_ == Tab::Community) {
    std::stable_sort(view_.begin(), view_.end(), [](const Card& a, const Card& b) {
      return a.title < b.title;
    });
  }
}

void HubState::refresh() { rebuild(); }

void HubState::set_tab(Tab tab) {
  tab_ = tab;
  if (pm_) pm_->settings().last_tab = tab_id(tab);
  rebuild();
}

void HubState::set_sort(SortBy sort) {
  sort_ = sort;
  if (pm_) {
    pm_->settings().last_sort = sort == SortBy::LastOpened     ? "opened"
                               : sort == SortBy::Name          ? "name"
                               : sort == SortBy::LastModified  ? "modified"
                                                                 : "created";
  }
  apply_view();
}

std::size_t HubState::count_in(Tab tab) const {
  switch (tab) {
    case Tab::Recent: return recents_.size();
    case Tab::Projects: return projects_.size();
    case Tab::Templates: return templates_.size();
    case Tab::Community: return 0;
  }
  return 0;
}

const Card* HubState::card_at(std::size_t index) const {
  if (index >= view_.size()) return nullptr;
  return &view_[index];
}

UiAction HubState::activate(std::size_t card_index) {
  UiAction action;
  action.card_index = card_index;
  const Card* c = card_at(card_index);
  if (!c || !c->actionable) return action;
  if (c->kind == CardKind::Template) {
    action.type = UiAction::Type::CreateProject;
    action.text = c->id;
  } else if (c->kind == CardKind::Project) {
    action.type = UiAction::Type::OpenProject;
    action.text = fs::to_generic_string(c->manifest_path);
  }
  return action;
}

UiAction HubState::secondary(std::size_t card_index, int slot) {
  UiAction action;
  action.card_index = card_index;
  const Card* c = card_at(card_index);
  if (!c || c->kind != CardKind::Project) return action;
  if (slot == 0) {
    action.type = UiAction::Type::RevealInFolder;
    action.text = fs::to_generic_string(c->manifest_path.parent_path());
  } else if (slot == 1) {
    action.type = UiAction::Type::DuplicateProject;
  } else if (slot == 2) {
    action.type = UiAction::Type::DeleteProject;
  }
  return action;
}

bool HubState::open_at(std::size_t card_index, ProjectRef* out, std::string* err) {
  const Card* c = card_at(card_index);
  if (!c || c->kind != CardKind::Project) {
    if (err) *err = "that card is not a project";
    return false;
  }
  if (!pm_->open(c->manifest_path, out, err)) return false;
  refresh();
  return true;
}

bool HubState::create_project(const std::string& name, const std::string& template_id,
                             ProjectRef* out, std::string* err) {
  if (!pm_) return false;
  ProjectTemplate tmpl;
  const bool have = !template_id.empty() && templates_.find(template_id, &tmpl);
  ProjectKind kind = ProjectKind::Generic3D;
  project_kind_from(pm_->settings().default_kind, &kind);
  if (have) project_kind_from(tmpl.kind, &kind);

  ProjectRef ref;
  const fs::Path dir = have ? tmpl.dir : fs::Path{};
  if (!pm_->create(name, kind, dir, &ref, err)) return false;

  // A built-in template has no directory on disk, so create() could not read its overrides.
  if (have && tmpl.is_builtin()) {
    ref.manifest.template_used = tmpl.id;
    ref.manifest.description = tmpl.description;
    ref.manifest.tags = tmpl.tags;
    ref.manifest.scene.from_json(tmpl.scene);
    std::string save_err;
    if (!pm_->save(ref, &save_err)) {
      LUME_LOG_WARN << "created, but template scene defaults did not save: " << save_err;
    }
  }
  if (out) *out = ref;
  refresh();
  return true;
}

bool HubState::delete_at(std::size_t card_index, std::string* err) {
  const Card* c = card_at(card_index);
  if (!c || c->kind != CardKind::Project) {
    if (err) *err = "only projects can be deleted";
    return false;
  }
  ProjectRef ref;
  if (!pm_->inspect(c->manifest_path.parent_path(), &ref, err)) return false;
  if (!pm_->remove(ref, err)) return false;
  refresh();
  return true;
}

bool HubState::rename_at(std::size_t card_index, const std::string& new_name, std::string* err) {
  const Card* c = card_at(card_index);
  if (!c || c->kind != CardKind::Project) {
    if (err) *err = "only projects can be renamed";
    return false;
  }
  ProjectRef ref;
  if (!pm_->inspect(c->manifest_path.parent_path(), &ref, err)) return false;
  if (!pm_->rename(ref, new_name, err)) return false;
  refresh();
  return true;
}

bool HubState::duplicate_at(std::size_t card_index, std::string* err) {
  const Card* c = card_at(card_index);
  if (!c || c->kind != CardKind::Project) {
    if (err) *err = "only projects can be duplicated";
    return false;
  }
  ProjectRef ref;
  if (!pm_->inspect(c->manifest_path.parent_path(), &ref, err)) return false;
  ProjectRef copy;
  if (!pm_->duplicate(ref, {}, &copy, err)) return false;
  refresh();
  return true;
}

std::string HubState::status_line() const {
  std::size_t autosaves = 0;
  for (const Card& c : view_) {
    if (c.has_autosave) ++autosaves;
  }
  std::string out = std::to_string(projects_.size()) +
                    (projects_.size() == 1 ? " project" : " projects") + " in " +
                    fs::to_generic_string(pm_ ? pm_->projects_dir() : fs::Path()) +
                    " - showing " + std::to_string(view_.size());
  if (autosaves) out += ", " + std::to_string(autosaves) + " with autosave";
  if (!query_.empty()) out += " for \"" + query_ + "\"";
  return out;
}

}  // namespace lume::hub
