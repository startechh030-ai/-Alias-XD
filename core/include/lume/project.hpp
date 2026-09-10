#pragma once
// The project system. This is the whole of step 1: how a Lume project is named, stored,
// found, opened, duplicated, and thrown away.
//
// Model: a project is a directory. `project.json` is the manifest and the only required
// file. Everything else is an asset folder that other steps fill in. A project must stay
// valid if a user drags it between machines, so all stored paths are relative and generic.

#include <cstdint>
#include <string>
#include <vector>

#include "lume/fs.hpp"
#include "lume/json.hpp"
#include "lume/settings.hpp"

namespace lume {

constexpr int kProjectSchemaVersion = 1;
constexpr const char* kManifestFileName = "project.json";
constexpr const char* kLocalFileName = "meta.json";       // machine-local, git-ignored
constexpr const char* kRecentFileName = "recent.json";
constexpr int kThumbnailWidth = 320;

enum class ProjectKind {
  Generic3D,     // empty scene, everything on the table
  ModelImport,   // started from an FBX/OBJ/glTF drop (step 7 wires the importer)
  PhysicsLab,    // solver playground, gravity on, colliders pre-created
  AnimationTest, // one skinned mesh + a clip, camera locked to it
  VoxelKit,      // blocky output target, snap-to-grid on
};

const char* to_string(ProjectKind kind);
bool project_kind_from(const std::string& text, ProjectKind* out);
std::vector<ProjectKind> all_project_kinds();

// ---- Scene defaults stored in the manifest ------------------------------------------
// Kept small and explicit: these are the values the editor's panels will write back, and
// the ones a project needs before any renderer exists to disagree about them.

struct RenderSettings {
  std::string backend = "auto";   // auto | opengl | vulkan   (step 2 owns the meaning)
  bool hdr = true;
  int msaa = 4;                   // 0 | 2 | 4 | 8
  float exposure = 1.0f;
  float fov_degrees = 60.0f;
  float near_z = 0.05f;
  float far_z = 500.0f;

  json::Value to_json() const;
  void from_json(const json::Value& v);
};

struct PhysicsSettings {
  bool enabled = true;
  std::vector<float> gravity{0.0f, -9.81f, 0.0f};
  double fixed_hz = 60.0;
  std::string solver = "builtin";  // builtin | jolt | off  (step 6)
  float default_density = 1.0f;
  float default_friction = 0.4f;
  float default_restitution = 0.1f;
  bool sleep_enabled = true;

  json::Value to_json() const;
  void from_json(const json::Value& v);
};

struct SceneSettings {
  std::string up_axis = "y";       // y (Blender-like) | z
  double units_per_meter = 1.0;
  float grid_size = 10.0f;         // metres across the floor grid
  float grid_snap = 0.25f;
  double fps = 60.0;
  bool autosave = true;
  int autosave_minutes = 5;

  RenderSettings render;
  PhysicsSettings physics;

  json::Value to_json() const;
  void from_json(const json::Value& v);
};

// ---- Manifest -------------------------------------------------------------------------

struct ProjectManifest {
  int schema = kProjectSchemaVersion;
  std::string id;              // stable 16-hex id, used for thumb/autosave names
  std::string name;            // display name, free-form
  std::string kind = "generic3d";
  std::string description;
  std::string author = "untitled";
  std::string created_utc;
  std::string modified_utc;
  std::string engine_min = "0.1.0";
  std::string template_used;   // informational, "empty-scene"
  std::vector<std::string> tags;

  SceneSettings scene;

  json::Value to_json() const;
  void from_json(const json::Value& v);
  bool validate(std::string* err) const;
};

// ---- A project as seen by the hub ------------------------------------------------------

enum class ProjectError {
  None,
  NotFound,
  NotADirectory,
  BadJson,
  MissingField,
  UnsupportedSchema,
  NameEmpty,
  NameTaken,
  Io,
};

const char* to_string(ProjectError code);

struct ProjectRef {
  fs::Path dir;
  fs::Path manifest_path;
  ProjectManifest manifest;
  json::Value raw_manifest;      // round-trips keys this build does not know about
  std::int64_t last_opened = 0;  // unix seconds, 0 when never opened
  bool has_autosave = false;
  bool has_thumbnail = false;

  bool valid() const { return !dir.empty() && !manifest.name.empty(); }
  // Set by the manager: <hub root>/thumbs/<id>_320.png (step 2 fills it with renders).
  fs::Path thumbs_dir;
  fs::Path thumbnail_path() const;
  fs::Path autosave_dir() const;
  fs::Path autosave_path() const;
  std::string folder_name() const;          // dir filename, e.g. "my-level-1a2b3c4d"
  std::string age_label(std::int64_t now) const;  // "3h ago" / "2d ago" / "never"
};

// ---- The manager: the hub's only door to project files ---------------------------------

class ProjectManager {
 public:
  // `root` holds projects/, thumbs/, trash/, recent.json, hub-settings.json.
  // Empty => fs::default_data_dir("Lume", "hub"), which is what a packaged build passes.
  explicit ProjectManager(fs::Path root = {});

  const fs::Path& root() const { return root_; }
  fs::Path projects_dir() const;
  fs::Path thumbnails_dir() const;
  fs::Path trash_dir() const;
  fs::Path recent_path() const;
  fs::Path settings_path() const;

  HubSettings& settings() { return settings_; }
  const HubSettings& settings() const { return settings_; }
  bool save_settings(std::string* err = nullptr) const;
  void set_projects_root(const fs::Path& root);  // also persists into settings

  // `template_dir` may be empty for a blank project; then only folders + manifest are made.
  bool create(const std::string& name, ProjectKind kind, const fs::Path& template_dir,
              ProjectRef* out, std::string* err = nullptr, ProjectError* code = nullptr);
  bool open(const fs::Path& manifest_or_dir, ProjectRef* out, std::string* err = nullptr,
            ProjectError* code = nullptr);
  bool save(const ProjectRef& ref, std::string* err = nullptr);
  bool save_manifest(const ProjectRef& ref, const ProjectManifest& manifest,
                     std::string* err = nullptr);
  bool save_as(const ProjectRef& ref, const std::string& new_name, const fs::Path& dest_root,
               ProjectRef* out, std::string* err = nullptr);
  bool duplicate(const ProjectRef& ref, const std::string& new_name, ProjectRef* out,
                 std::string* err = nullptr);
  bool rename(const ProjectRef& ref, const std::string& new_name, std::string* err = nullptr);
  bool remove(const ProjectRef& ref, std::string* err = nullptr);  // honours settings' trash flag
  bool touch_opened(const ProjectRef& ref, std::string* err = nullptr);

  bool validate_dir(const fs::Path& dir, std::string* err = nullptr) const;
  // Side-effect-free read: no recents, no meta touch. Used by the hub's card actions.
  bool inspect(const fs::Path& dir_or_manifest, ProjectRef* out, std::string* err = nullptr) const;

  std::vector<ProjectRef> scan() const;   // every valid project under projects_dir
  std::vector<ProjectRef> recent();       // most recent first, dead entries pruned + saved
  void clear_recent();

  bool name_taken(const std::string& name) const;
  fs::Path project_dir_for(const std::string& name) const;

  static std::string sanitize_name(const std::string& raw, std::string* err = nullptr);
  static std::string new_id(const std::string& seed);
  static std::string humanize_age(std::int64_t then, std::int64_t now);

 private:
  // Reads <dir>/project.json without touching recents or meta.
  bool read_ref(const fs::Path& dir, ProjectRef* out, std::string* err,
                ProjectError* code) const;
  bool clone_into(const ProjectRef& ref, const std::string& clean_name, const fs::Path& dest_root,
                  ProjectRef* out, std::string* err);
  bool write_local_meta(const ProjectRef& ref, std::string* err) const;
  bool load_recent(json::Array* out) const;
  bool save_recent(const json::Array& entries) const;

  fs::Path root_;
  HubSettings settings_;
};

}  // namespace lume
