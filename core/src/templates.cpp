#include "lume/templates.hpp"

#include <algorithm>
#include <cctype>

#include "lume/log.hpp"
#include "lume/project.hpp"

namespace lume {
namespace {

constexpr const char* kTemplateFileName = "template.json";

ProjectTemplate make_builtin(const char* id, const char* name, const char* desc,
                             ProjectKind kind, const char* icon,
                             const std::vector<std::string>& tags) {
  ProjectTemplate t;
  t.id = id;
  t.name = name;
  t.description = desc;
  t.kind = to_string(kind);
  t.icon = icon;
  t.tags = tags;
  return t;
}

bool iequal(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::vector<ProjectTemplate> TemplateLibrary::builtin() {
  std::vector<ProjectTemplate> out = {
      make_builtin("empty-scene", "Empty Scene", "One camera, one light, a floor grid. Nothing else.",
                   ProjectKind::Generic3D, "cube", {"starter"}),
      make_builtin("model-import", "Model Import", "Drop an FBX, OBJ or glTF and start from that mesh.",
                   ProjectKind::ModelImport, "cube-plus", {"starter", "import"}),
      make_builtin("physics-lab", "Physics Lab",
                   "Gravity on, a ground plane and a stack of boxes to knock over.",
                   ProjectKind::PhysicsLab, "gravity", {"starter", "physics"}),
      make_builtin("animation-test", "Animation Test", "A rigged mesh and one clip, camera locked to it.",
                   ProjectKind::AnimationTest, "play", {"starter", "animation"}),
      make_builtin("voxel-kit", "Voxel Kit", "Snap-to-grid on, small triangles, built for blocky output.",
                   ProjectKind::VoxelKit, "grid", {"starter", "mobile"}),
  };
  for (ProjectTemplate& t : out) {
    json::Value scene = json::Value::make_object();
    if (t.kind == std::string(to_string(ProjectKind::VoxelKit))) {
      scene.set("gridSnap", json::Value(1.0f));
      scene.set("gridSize", json::Value(16.0f));
    }
    if (t.kind == std::string(to_string(ProjectKind::AnimationTest))) {
      scene.set("fps", json::Value(30.0));
    }
    if (t.kind == std::string(to_string(ProjectKind::PhysicsLab))) {
      json::Value physics = json::Value::make_object();
      physics.set("enabled", json::Value(true));
      scene.set("physics", physics);
    }
    t.scene = scene;
  }
  return out;
}

bool TemplateLibrary::load(const fs::Path& dir, ProjectTemplate* out, std::string* err) {
  if (!out) return false;
  const fs::Path file = dir / kTemplateFileName;
  std::string text;
  if (!fs::read_text(file, &text, err)) {
    if (err) *err = "no " + std::string(kTemplateFileName) + " in " + dir.string();
    return false;
  }

  json::Value v;
  std::string parse_err;
  if (!json::Value::parse(text, &v, &parse_err)) {
    if (err) *err = file.string() + ": " + parse_err;
    return false;
  }
  if (!v.is_object()) {
    if (err) *err = file.string() + ": template.json must hold an object";
    return false;
  }

  ProjectTemplate t;
  t.dir = dir;
  t.id = json::get_string(v, "id", fs::stem(dir));
  t.name = json::get_string(v, "name", t.id);
  t.description = json::get_string(v, "description");
  t.icon = json::get_string(v, "icon", "cube");
  const std::string kind = json::get_string(v, "kind", "generic3d");
  ProjectKind parsed = ProjectKind::Generic3D;
  if (!project_kind_from(kind, &parsed)) {
    LUME_LOG_WARN << "template \"" << t.id << "\" has unknown kind \"" << kind
                  << "\", using generic3d";
  } else {
    t.kind = kind;
  }
  t.tags = json::get_string_array(v, "tags");
  t.copy = json::get_string_array(v, "copy");
  if (!v.at("scene").is_null()) t.scene = v.at("scene");

  *out = std::move(t);
  return true;
}

bool TemplateLibrary::write(const ProjectTemplate& tmpl, std::string* err) {
  if (tmpl.dir.empty()) {
    if (err) *err = "template has no directory to write into";
    return false;
  }
  json::Value v = json::Value::make_object();
  v.set("schema", json::Value(static_cast<std::int64_t>(1)));
  v.set("id", json::Value(tmpl.id));
  v.set("name", json::Value(tmpl.name));
  v.set("description", json::Value(tmpl.description));
  v.set("kind", json::Value(tmpl.kind));
  v.set("icon", json::Value(tmpl.icon));
  json::set_string_array(v, "tags", tmpl.tags);
  json::set_string_array(v, "copy", tmpl.copy);
  if (tmpl.scene.is_object()) v.set("scene", tmpl.scene);
  return fs::write_text(tmpl.dir / kTemplateFileName, v.dump(2) + "\n", err);
}

std::vector<ProjectTemplate> TemplateLibrary::discover() const {
  std::vector<ProjectTemplate> out = builtin();
  if (fs::is_directory(root_)) {
    for (const fs::Path& dir : fs::list_directories(root_)) {
      ProjectTemplate t;
      std::string err;
      if (!load(dir, &t, &err)) {
        if (!err.empty()) LUME_LOG_DEBUG << "ignoring " << fs::to_generic_string(dir) << ": " << err;
        continue;
      }
      bool replaced = false;
      for (ProjectTemplate& existing : out) {
        if (iequal(existing.id, t.id)) {
          existing = t;  // a local template of the same id wins over the built-in one
          replaced = true;
          break;
        }
      }
      if (!replaced) out.push_back(std::move(t));
    }
  }
  std::sort(out.begin(), out.end(), [](const ProjectTemplate& a, const ProjectTemplate& b) {
    return a.name < b.name;
  });
  return out;
}

bool TemplateLibrary::find(const std::string& id, ProjectTemplate* out) const {
  if (id.empty() || !out) return false;
  for (const ProjectTemplate& t : discover()) {
    if (iequal(t.id, id)) {
      *out = t;
      return true;
    }
  }
  return false;
}

std::vector<std::string> TemplateLibrary::ids() const {
  std::vector<std::string> out;
  for (const ProjectTemplate& t : discover()) out.push_back(t.id);
  return out;
}

}  // namespace lume
