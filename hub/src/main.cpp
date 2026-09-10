// lume-hub, step 1: the project hub without a window.
//
// The dark hub in assets/hub/hub-mockup.svg is drawn by hub_ui.cpp, and needs a renderer -
// that is step 2 (Filament). Until then this binary is the hub: same HubState, same
// ProjectManager, same filters, sorted cards and all. Nothing here is a mock; it is the
// headless front end of the real thing, and it is what CI exercises.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "hub_state.hpp"
#include "hub_ui.hpp"
#include "lume/log.hpp"
#include "lume/project.hpp"
#include "lume/templates.hpp"
#include "theme.hpp"

namespace {

using lume::ProjectKind;
using lume::ProjectManager;
using lume::ProjectRef;
using lume::fs::Path;
using lume::hub::Card;
using lume::hub::CardKind;
using lume::hub::HubState;
using lume::hub::SortBy;
using lume::hub::Tab;

constexpr int kCellW = 26;

struct Options {
  Path projects_dir;
  Path templates_dir;
  std::string search;
  std::string tab = "recent";
  std::string sort;
  std::string new_name;
  std::string new_template;
  std::string new_kind;
  std::string open_path;
  std::string dup_path;
  std::string delete_path;
  std::string rename_path;
  std::string as_name;
  std::string validate_path;
  std::string dump_path;
  bool print = false;
  bool list_templates = false;
  bool clear_recent = false;
  bool quiet = false;
  bool help = false;
  bool version = false;
};

void usage() {
  std::printf(
      "lume hub (step 1 - project system, no viewport yet)\n"
      "\n"
      "  lume-hub [options]\n"
      "\n"
      "  --projects-dir <p>   where projects live (default: OS app-data dir)\n"
      "  --templates-dir <p>  template root (default: <repo>/templates)\n"
      "  --tab <recent|projects|templates|community>\n"
      "  --sort <opened|name|modified|created>\n"
      "  --search <text>      filter cards by name, kind, tag or author\n"
      "  --print              draw the hub as text and exit\n"
      "\n"
      "  --new <name>         create a project\n"
      "  --template <id>      start it from a template (see --list-templates)\n"
      "  --kind <id>          generic3d|model-import|physics-lab|animation-test|voxel-kit\n"
      "  --open <path>        open a project (updates the Recent tab)\n"
      "  --duplicate <path>   copy a project, optionally --as <name>\n"
      "  --rename <path> --as <name>\n"
      "  --delete <path>      move to the hub trash (permanent if hub-settings says so)\n"
      "  --validate <path>    check a folder is a readable project\n"
      "  --dump <path>        print its project.json, normalised\n"
      "  --list-templates     show what New project offers\n"
      "  --clear-recent       empty the Recent tab\n"
      "  --quiet, --help, --version\n");
}

std::string arg(int& i, int argc, char** argv, const char* flag) {
  if (i + 1 >= argc) {
    std::printf("error: %s needs a value\n", flag);
    std::exit(1);
  }
  return argv[++i];
}

bool parse(int argc, char** argv, Options* o) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](const char* f) { return arg(i, argc, argv, f); };
    if (a == "--help" || a == "-h") o->help = true;
    else if (a == "--version") o->version = true;
    else if (a == "--quiet") o->quiet = true;
    else if (a == "--print" || a == "--list") o->print = true;
    else if (a == "--list-templates") o->list_templates = true;
    else if (a == "--clear-recent") o->clear_recent = true;
    else if (a == "--projects-dir") o->projects_dir = next("--projects-dir");
    else if (a == "--templates-dir") o->templates_dir = next("--templates-dir");
    else if (a == "--tab") o->tab = next("--tab");
    else if (a == "--sort") o->sort = next("--sort");
    else if (a == "--search") o->search = next("--search");
    else if (a == "--new") o->new_name = next("--new");
    else if (a == "--template") o->new_template = next("--template");
    else if (a == "--kind") o->new_kind = next("--kind");
    else if (a == "--open") o->open_path = next("--open");
    else if (a == "--duplicate") o->dup_path = next("--duplicate");
    else if (a == "--delete") o->delete_path = next("--delete");
    else if (a == "--rename") o->rename_path = next("--rename");
    else if (a == "--as") o->as_name = next("--as");
    else if (a == "--validate") o->validate_path = next("--validate");
    else if (a == "--dump") o->dump_path = next("--dump");
    else {
      std::printf("error: unknown option \"%s\" (try --help)\n", a.c_str());
      return false;
    }
  }
  return true;
}

Path discover_templates_root(const Options& o) {
  if (!o.templates_dir.empty()) return o.templates_dir;
  if (const char* env = std::getenv("LUME_TEMPLATES")) {
    if (*env && lume::fs::is_directory(env)) return Path(env);
  }
  const std::vector<Path> guesses = {
#ifdef LUME_SOURCE_DIR
      Path(LUME_SOURCE_DIR) / "templates",
#endif
      Path("templates"),
      Path("..") / "templates",
  };
  for (const Path& p : guesses) {
    if (lume::fs::is_directory(p)) return p;
  }
  return {};  // built-in templates still work
}

std::string clip(const std::string& s, std::size_t w) {
  if (s.size() <= w) return s;
  if (w <= 1) return s.substr(0, w);
  return s.substr(0, w - 1) + "\xE2\x80\xA6";  // ellipsis, UTF-8 on every target
}

std::string pad(const std::string& s, std::size_t w) {
  std::string out = clip(s, w);
  out.append(w - std::min(w, out.size()), ' ');
  return out;
}

void print_row(const std::vector<const Card*>& row) {
  std::string top = "  ", bot = "  ";
  for (std::size_t i = 0; i < row.size(); ++i) {
    if (i) { top += "  "; bot += "  "; }
    top += "+" + std::string(static_cast<std::size_t>(kCellW) + 2, '-') + "+";
    bot += "+" + std::string(static_cast<std::size_t>(kCellW) + 2, '-') + "+";
  }
  std::printf("%s\n", top.c_str());
  const char* lines[3] = {"title", "sub", "note"};
  (void)lines;
  for (int l = 0; l < 3; ++l) {
    std::string out = "  |";
    for (std::size_t i = 0; i < row.size(); ++i) {
      const Card& c = *row[i];
      std::string text;
      if (l == 0) {
        text = c.title;
      } else if (l == 1) {
        text = c.subtitle;
        if (!c.badge.empty()) text += "  [" + c.badge + "]";
      } else {
        text = c.kind == CardKind::Template ? c.note + "  +" : c.note;
      }
      out += " " + pad(text, static_cast<std::size_t>(kCellW)) + " |";
    }
    std::printf("%s\n", out.c_str());
  }
  std::printf("%s\n", bot.c_str());
}

void print_hub(HubState& hub, const Options& o) {
  std::string header = "lume · hub";
  if (!o.search.empty()) header += " · search \"" + o.search + "\"";
  std::printf("\n%s\n", header.c_str());

  std::string tabs;
  for (Tab t : {Tab::Recent, Tab::Projects, Tab::Templates, Tab::Community}) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s%s (%zu)", hub.tab() == t ? "[" : " ",
                  HubState::tab_label(t), hub.count_in(t));
    tabs += buf;
    if (hub.tab() == t) tabs += "]";
  }
  std::printf("%s\n", tabs.c_str());
  std::printf("sort: %s   ·   %s\n", HubState::sort_label(hub.sort()),
              hub.status_line().c_str());
  std::printf("\n");

  const auto& cards = hub.cards();
  if (cards.empty()) {
    std::printf("  no cards.  lume-hub --new \"First Scene\" --template empty-scene --print\n\n");
    return;
  }
  const int cols = std::max(1, std::min(static_cast<int>(cards.size()), 2));
  for (std::size_t start = 0; start < cards.size(); start += static_cast<std::size_t>(cols)) {
    std::vector<const Card*> row;
    for (std::size_t i = start; i < cards.size() && i < start + static_cast<std::size_t>(cols); ++i) {
      row.push_back(&cards[i]);
    }
    print_row(row);
    std::printf("\n");
  }
}

void print_manifest(const ProjectRef& ref) {
  std::printf("name        %s\n", ref.manifest.name.c_str());
  std::printf("id          %s\n", ref.manifest.id.c_str());
  std::printf("kind        %s\n", ref.manifest.kind.c_str());
  std::printf("created     %s\n", ref.manifest.created_utc.c_str());
  std::printf("modified    %s\n", ref.manifest.modified_utc.c_str());
  std::printf("template    %s\n", ref.manifest.template_used.c_str());
  std::printf("folder      %s\n", lume::fs::to_generic_string(ref.dir).c_str());
  std::printf("thumbnail   %s\n",
              ref.has_thumbnail ? lume::fs::to_generic_string(ref.thumbnail_path()).c_str()
                                : "(none yet - step 2 renders these)");
  std::printf("autosave    %s\n", ref.has_autosave ? "present" : "none");
  std::printf("up axis     %s   grid %.2fm  snap %.2f\n", ref.manifest.scene.up_axis.c_str(),
              static_cast<double>(ref.manifest.scene.grid_size),
              static_cast<double>(ref.manifest.scene.grid_snap));
  std::printf("physics     %s  %s @ %.0fHz\n", ref.manifest.scene.physics.enabled ? "on" : "off",
              ref.manifest.scene.physics.solver.c_str(), ref.manifest.scene.physics.fixed_hz);
}

}  // namespace

int main(int argc, char** argv) {
  Options o;
  if (!parse(argc, argv, &o)) {
    usage();
    return 1;
  }
  if (o.help) {
    usage();
    return 0;
  }
  if (o.version) {
    std::printf("lume hub 0.1.0 (step 1: project system)\n");
    return 0;
  }
  if (o.quiet) lume::log::set_stderr(false);

  ProjectManager pm(o.projects_dir);
  const Path templates_root = discover_templates_root(o);
  if (!templates_root.empty()) pm.settings().templates_root = templates_root;

  HubState hub(&pm, templates_root);
  Tab tab = HubState::Recent;
  if (HubState::tab_from_id(o.tab, &tab)) hub.set_tab(tab);
  SortBy sort = SortBy::LastOpened;
  if (!o.sort.empty() && HubState::sort_from_id(o.sort, &sort)) hub.set_sort(sort);
  if (!o.search.empty()) hub.set_query(o.search);

  if (o.list_templates) {
    lume::hub::TemplateLibrary lib(templates_root);
    for (const auto& t : lib.discover()) {
      std::printf("%-16s %-14s %s\n", t.id.c_str(), t.kind.c_str(), t.name.c_str());
      if (!t.description.empty()) std::printf("%32s%s\n", "", t.description.c_str());
    }
    return 0;
  }

  std::string err;
  int rc = 0;
  bool mutated = false;

  if (!o.dump_path.empty()) {
    ProjectRef ref;
    if (!pm.inspect(o.dump_path, &ref, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    std::printf("%s\n", ref.manifest.to_json().dump(2).c_str());
    return 0;
  }

  if (!o.validate_path.empty()) {
    if (pm.validate_dir(o.validate_path, &err)) {
      std::printf("ok: %s\n", lume::fs::to_generic_string(o.validate_path).c_str());
    } else {
      std::printf("not a Lume project: %s\n", err.c_str());
      rc = 2;
    }
    return rc;
  }

  if (!o.new_name.empty()) {
    ProjectKind kind = ProjectKind::Generic3D;
    if (!o.new_kind.empty() && !project_kind_from(o.new_kind, &kind)) {
      std::printf("error: unknown kind \"%s\"\n", o.new_kind.c_str());
      return 1;
    }
    ProjectRef created;
    const std::string tmpl = o.new_template.empty() ? pm.settings().default_template : o.new_template;
    if (!hub.create_project(o.new_name, o.new_template, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    (void)tmpl;
    if (!o.new_kind.empty() || o.new_template.empty()) {
      // No template was applied by create(); honour an explicit --kind by rewriting the kind.
      if (pm.inspect(pm.project_dir_for(o.new_name), &created, nullptr) &&
          created.manifest.kind != to_string(kind)) {
        created.manifest.kind = to_string(kind);
        pm.save(created, &err);
      }
    }
    std::printf("created \"%s\"  ->  %s\n", o.new_name.c_str(),
                lume::fs::to_generic_string(pm.project_dir_for(o.new_name)).c_str());
    mutated = true;
  }

  if (!o.open_path.empty()) {
    ProjectRef ref;
    if (!pm.open(o.open_path, &ref, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    std::printf("opened \"%s\"\n", ref.manifest.name.c_str());
    print_manifest(ref);
    mutated = true;
  }

  if (!o.dup_path.empty()) {
    ProjectRef src;
    if (!pm.inspect(o.dup_path, &src, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    ProjectRef out;
    if (!pm.duplicate(src, o.as_name, &out, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    std::printf("duplicated -> \"%s\"  %s\n", out.manifest.name.c_str(),
                lume::fs::to_generic_string(out.dir).c_str());
    mutated = true;
  }

  if (!o.rename_path.empty()) {
    if (o.as_name.empty()) {
      std::printf("error: --rename needs --as <name>\n");
      return 1;
    }
    ProjectRef src;
    if (!pm.inspect(o.rename_path, &src, &err) || !pm.rename(src, o.as_name, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    std::printf("renamed to \"%s\" (folder keeps its old name; that is fine, it is an id)\n",
                o.as_name.c_str());
    mutated = true;
  }

  if (!o.delete_path.empty()) {
    ProjectRef src;
    if (!pm.inspect(o.delete_path, &src, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    if (!pm.remove(src, &err)) {
      std::printf("error: %s\n", err.c_str());
      return 2;
    }
    std::printf("%s \"%s\"\n", pm.settings().move_delete_to_trash ? "trashed" : "deleted",
                src.manifest.name.c_str());
    mutated = true;
  }

  if (o.clear_recent) {
    pm.clear_recent();
    std::printf("recent list cleared\n");
    mutated = true;
  }

  if (mutated) {
    std::string save_err;
    pm.save_settings(&save_err);
    hub.refresh();
    if (!o.tab.empty()) {
      Tab t = hub.tab();
      if (HubState::tab_from_id(o.tab, &t)) hub.set_tab(t);
    }
  }

  if (o.print || !mutated) {
    print_hub(hub, o);
    if (!o.print) {
      std::printf("  (nothing asked for: showing the hub. --help for the verbs.)\n\n");
    }
  }
  std::printf("  viewport, thumbnails and the windowed hub arrive with Filament in step 2.\n\n");
  return rc;
}
