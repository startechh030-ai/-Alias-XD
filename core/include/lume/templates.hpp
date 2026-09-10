#pragma once
// New-project templates, as offered by the hub's "Start" cards.
//
// A template is a directory holding `template.json`. It may carry seed assets; the manager
// copies those into the new project. If the template root is missing (a fresh checkout, an
// Android install before first sync) the hub still gets the built-in list, so `New project`
// can never dead-end.

#include <string>
#include <vector>

#include "lume/fs.hpp"
#include "lume/json.hpp"

namespace lume {

struct ProjectTemplate {
  std::string id;                 // directory name, e.g. "empty-scene"
  std::string name;               // card title, e.g. "Empty Scene"
  std::string description;
  std::string kind = "generic3d"; // ProjectKind name
  std::string icon = "cube";      // assets/ui/icons/<icon>.svg
  std::vector<std::string> tags;
  std::vector<std::string> copy;  // files/dirs copied into the new project
  json::Value scene;              // optional "scene" object merged over SceneSettings
  fs::Path dir;

  bool is_builtin() const { return dir.empty(); }
  bool seeds_assets() const { return !copy.empty(); }
};

class TemplateLibrary {
 public:
  TemplateLibrary() = default;
  explicit TemplateLibrary(fs::Path root) : root_(std::move(root)) {}

  void set_root(const fs::Path& root) { root_ = root; }
  const fs::Path& root() const { return root_; }

  std::vector<ProjectTemplate> discover() const;         // builtin list, overridden by disk
  bool find(const std::string& id, ProjectTemplate* out) const;
  std::vector<std::string> ids() const;

  static std::vector<ProjectTemplate> builtin();
  static bool load(const fs::Path& dir, ProjectTemplate* out, std::string* err = nullptr);
  static bool write(const ProjectTemplate& tmpl, std::string* err = nullptr);  // for tests

 private:
  fs::Path root_;
};

}  // namespace lume
