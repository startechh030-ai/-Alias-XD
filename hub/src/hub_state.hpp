#pragma once
// Everything the hub shows, with no renderer and no immediate-mode library in sight.
//
// The split is deliberate: hub_state owns tab selection, search, sorting and card building,
// and is unit-tested headlessly on Linux, Windows and the NDK. hub_ui.cpp only ever asks for
// `cards()` and reports back an action. That keeps the step 2 renderer a dumb painting of a
// structure that already works.

#include <cstdint>
#include <string>
#include <vector>

#include "lume/fs.hpp"
#include "lume/project.hpp"
#include "lume/templates.hpp"

namespace lume::hub {

enum class Tab { Recent, Projects, Templates, Community };
enum class SortBy { LastOpened, Name, LastModified, Created };

enum class CardKind { Project, Template, Notice };

struct Card {
  CardKind kind = CardKind::Project;
  std::string id;
  std::string title;
  std::string subtitle;      // "Edited 2h ago" / "Starts from a model drop"
  std::string badge;         // small pill: "autosave", "physics-lab"
  std::string note;         // third line, dim: folder or template description
  std::string query_blob;    // lowercased haystack for search
  std::string thumbnail;     // empty => the card draws its tint gradient instead
  fs::Path manifest_path;    // projects
  fs::Path template_dir;     // templates
  std::uint32_t tint = 0;  // theme::card_tint(hash(id)) at build time
  bool actionable = true;
  bool missing = false;      // a recent entry whose folder is gone
  bool has_autosave = false;
  std::int64_t opened_at = 0;
  std::int64_t modified_at = 0;
  std::int64_t created_at = 0;
};

struct UiAction {
  enum class Type {
    None,
    OpenProject,
    CreateProject,
    DeleteProject,
    RenameProject,
    DuplicateProject,
    RevealInFolder,
    ClearRecent,
    OpenSettings,
    SwitchTab,
  };
  Type type = Type::None;
  std::size_t card_index = 0;  // index into HubState::cards()
  std::string text;            // payload: project name, path, tab id...
  std::string template_id;     // set for CreateProject
};

class HubState {
 public:
  HubState(ProjectManager* manager, const fs::Path& templates_root);

  // Re-reads projects/, recent.json and templates/. Called on focus, after any mutation.
  void refresh();

  Tab tab() const { return tab_; }
  void set_tab(Tab tab);
  SortBy sort() const { return sort_; }
  void set_sort(SortBy sort);
  const std::string& query() const { return query_; }
  void set_query(const std::string& query) {
    query_ = query;
    apply_view();
  }

  const std::vector<Card>& cards() const { return view_; }
  const std::vector<Card>& all_cards() const { return cards_; }
  std::size_t count_in(Tab tab) const;

  // Clicking a card. The hub's only write path into the project system.
  UiAction activate(std::size_t card_index);
  UiAction secondary(std::size_t card_index, int slot);  // 0 open, 1 duplicate, 2 delete

  // Creates the project, applies the template (built-in templates have no directory, so
  // their scene overrides land here), refreshes. `out` may be null.
  bool create_project(const std::string& name, const std::string& template_id,
                      ProjectRef* out, std::string* err = nullptr);
  bool delete_at(std::size_t card_index, std::string* err);
  bool rename_at(std::size_t card_index, const std::string& new_name, std::string* err);
  bool duplicate_at(std::size_t card_index, std::string* err);
  bool open_at(std::size_t card_index, ProjectRef* out, std::string* err);

  std::string status_line() const;   // "3 projects - 1 with autosave"
  std::string footer_path() const { return fs::to_generic_string(pm_->projects_dir()); }

  static const char* tab_label(Tab tab);
  static const char* tab_id(Tab tab);
  static bool tab_from_id(const std::string& id, Tab* out);
  static const char* sort_label(SortBy sort);
  static bool sort_from_id(const std::string& id, SortBy* out);
  static std::vector<Card> project_cards(const std::vector<ProjectRef>& refs, std::int64_t now);
  static std::vector<Card> template_cards(const std::vector<ProjectTemplate>& templates);
  static bool matches(const Card& card, const std::string& needle);

  ProjectManager& manager() { return *pm_; }
  const std::vector<ProjectRef>& projects() const { return projects_; }
  const std::vector<ProjectRef>& recents() const { return recents_; }
  const std::vector<ProjectTemplate>& templates() const { return templates_; }

 private:
  void rebuild();
  void apply_view();
  const Card* card_at(std::size_t index) const;

  ProjectManager* pm_ = nullptr;
  TemplateLibrary templates_;
  std::vector<Card> cards_;
  std::vector<Card> view_;
  std::vector<ProjectRef> recents_;
  std::vector<ProjectRef> projects_;
  std::vector<ProjectTemplate> templates_;
  Tab tab_ = Tab::Recent;
  SortBy sort_ = SortBy::LastOpened;
  std::string query_;
};

}  // namespace lume::hub
