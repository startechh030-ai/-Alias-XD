#include "lume/project.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "lume/log.hpp"
#include "lume/templates.hpp"

namespace lume {
namespace {

constexpr std::size_t kMaxNameLength = 64;
const char* kProjectSubdirs[] = {"meshes", "textures", "animations", "imports", "scene",
                                 "autosave"};

std::string trim(const std::string& s) {
  std::size_t a = 0;
  std::size_t b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  std::string out;
  out.reserve(b - a);
  bool pending_space = false;
  for (std::size_t i = a; i < b; ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (std::isspace(c)) {
      pending_space = !out.empty();
      continue;
    }
    if (pending_space) {
      out.push_back(' ');
      pending_space = false;
    }
    out.push_back(static_cast<char>(c));
  }
  return out;
}

bool iequals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

json::Value floats_to_array(const std::vector<float>& values) {
  json::Array arr;
  arr.reserve(values.size());
  for (float f : values) arr.push_back(json::Value(f));
  return json::Value(std::move(arr));
}

std::vector<float> array_to_floats(const json::Value& v, std::size_t count,
                                   const std::vector<float>& fallback) {
  if (!v.is_array() || v.size() != count) return fallback;
  std::vector<float> out;
  out.reserve(count);
  for (const json::Value& e : v.elements()) out.push_back(e.as_float());
  return out;
}

}  // namespace

const char* to_string(ProjectKind kind) {
  switch (kind) {
    case ProjectKind::Generic3D: return "generic3d";
    case ProjectKind::ModelImport: return "model-import";
    case ProjectKind::PhysicsLab: return "physics-lab";
    case ProjectKind::AnimationTest: return "animation-test";
    case ProjectKind::VoxelKit: return "voxel-kit";
  }
  return "generic3d";
}

bool project_kind_from(const std::string& text, ProjectKind* out) {
  if (!out) return false;
  for (ProjectKind k : all_project_kinds()) {
    if (text == to_string(k)) {
      *out = k;
      return true;
    }
  }
  return false;
}

std::vector<ProjectKind> all_project_kinds() {
  return {ProjectKind::Generic3D, ProjectKind::ModelImport, ProjectKind::PhysicsLab,
          ProjectKind::AnimationTest, ProjectKind::VoxelKit};
}

const char* to_string(ProjectError code) {
  switch (code) {
    case ProjectError::None: return "none";
    case ProjectError::NotFound: return "not-found";
    case ProjectError::NotADirectory: return "not-a-directory";
    case ProjectError::BadJson: return "bad-json";
    case ProjectError::MissingField: return "missing-field";
    case ProjectError::UnsupportedSchema: return "unsupported-schema";
    case ProjectError::NameEmpty: return "name-empty";
    case ProjectError::NameTaken: return "name-taken";
    case ProjectError::Io: return "io-error";
  }
  return "unknown";
}

// ---- settings blocks ---------------------------------------------------------------

json::Value RenderSettings::to_json() const {
  json::Value v = json::Value::make_object();
  v.set("backend", json::Value(backend));
  v.set("hdr", json::Value(hdr));
  v.set("msaa", json::Value(static_cast<std::int64_t>(msaa)));
  v.set("exposure", json::Value(exposure));
  v.set("fov", json::Value(fov_degrees));
  v.set("near", json::Value(near_z));
  v.set("far", json::Value(far_z));
  return v;
}

void RenderSettings::from_json(const json::Value& v) {
  if (!v.is_object()) return;
  const std::string b = json::get_string(v, "backend", backend);
  if (b == "auto" || b == "opengl" || b == "vulkan") backend = b;
  hdr = json::get_bool(v, "hdr", hdr);
  msaa = static_cast<int>(json::get_int(v, "msaa", msaa));
  if (msaa != 0 && msaa != 2 && msaa != 4 && msaa != 8) msaa = 4;
  exposure = v.at("exposure").as_float(exposure);
  fov_degrees = v.at("fov").as_float(fov_degrees);
  near_z = v.at("near").as_float(near_z);
  far_z = v.at("far").as_float(far_z);
  if (near_z <= 0.0f || far_z <= near_z) {
    near_z = 0.05f;
    far_z = 500.0f;
  }
}

json::Value PhysicsSettings::to_json() const {
  json::Value v = json::Value::make_object();
  v.set("enabled", json::Value(enabled));
  v.set("gravity", floats_to_array(gravity));
  v.set("fixedHz", json::Value(fixed_hz));
  v.set("solver", json::Value(solver));
  v.set("density", json::Value(default_density));
  v.set("friction", json::Value(default_friction));
  v.set("restitution", json::Value(default_restitution));
  v.set("sleep", json::Value(sleep_enabled));
  return v;
}

void PhysicsSettings::from_json(const json::Value& v) {
  if (!v.is_object()) return;
  enabled = json::get_bool(v, "enabled", enabled);
  gravity = array_to_floats(v.at("gravity"), 3, gravity);
  fixed_hz = json::get_number(v, "fixedHz", fixed_hz);
  if (fixed_hz < 10.0 || fixed_hz > 240.0) fixed_hz = 60.0;
  const std::string s = json::get_string(v, "solver", solver);
  if (s == "builtin" || s == "jolt" || s == "off") solver = s;
  default_density = v.at("density").as_float(default_density);
  default_friction = v.at("friction").as_float(default_friction);
  default_restitution = v.at("restitution").as_float(default_restitution);
  sleep_enabled = json::get_bool(v, "sleep", sleep_enabled);
}

json::Value SceneSettings::to_json() const {
  json::Value v = json::Value::make_object();
  v.set("upAxis", json::Value(up_axis));
  v.set("unitsPerMeter", json::Value(units_per_meter));
  v.set("gridSize", json::Value(grid_size));
  v.set("gridSnap", json::Value(grid_snap));
  v.set("fps", json::Value(fps));
  v.set("autosave", json::Value(autosave));
  v.set("autosaveMinutes", json::Value(static_cast<std::int64_t>(autosave_minutes)));
  v.set("render", render.to_json());
  v.set("physics", physics.to_json());
  return v;
}

void SceneSettings::from_json(const json::Value& v) {
  if (!v.is_object()) return;
  const std::string up = json::get_string(v, "upAxis", up_axis);
  up_axis = (up == "z" || up == "y") ? up : "y";
  units_per_meter = json::get_number(v, "unitsPerMeter", units_per_meter);
  if (units_per_meter <= 0.0) units_per_meter = 1.0;
  grid_size = v.at("gridSize").as_float(grid_size);
  grid_snap = v.at("gridSnap").as_float(grid_snap);
  if (grid_snap <= 0.0f) grid_snap = 0.25f;
  fps = json::get_number(v, "fps", fps);
  if (fps < 1.0 || fps > 1000.0) fps = 60.0;
  autosave = json::get_bool(v, "autosave", autosave);
  autosave_minutes =
      static_cast<int>(std::max<std::int64_t>(1, json::get_int(v, "autosaveMinutes", autosave_minutes)));
  render.from_json(v.at("render"));
  physics.from_json(v.at("physics"));
}

// ---- manifest ----------------------------------------------------------------------

json::Value ProjectManifest::to_json() const {
  json::Value v = json::Value::make_object();
  v.set("schema", json::Value(static_cast<std::int64_t>(schema)));
  v.set("id", json::Value(id));
  v.set("name", json::Value(name));
  v.set("kind", json::Value(kind));
  v.set("description", json::Value(description));
  v.set("author", json::Value(author));
  v.set("engineMin", json::Value(engine_min));
  v.set("template", json::Value(template_used));
  v.set("created", json::Value(created_utc));
  v.set("modified", json::Value(modified_utc));
  json::set_string_array(v, "tags", tags);
  v.set("scene", scene.to_json());
  return v;
}

void ProjectManifest::from_json(const json::Value& v) {
  if (!v.is_object()) return;
  schema = static_cast<int>(json::get_int(v, "schema", schema));
  id = json::get_string(v, "id", id);
  name = json::get_string(v, "name", name);
  kind = json::get_string(v, "kind", kind);
  description = json::get_string(v, "description", description);
  author = json::get_string(v, "author", author);
  engine_min = json::get_string(v, "engineMin", engine_min);
  template_used = json::get_string(v, "template", template_used);
  created_utc = json::get_string(v, "created", created_utc);
  modified_utc = json::get_string(v, "modified", modified_utc);
  tags = json::get_string_array(v, "tags");
  scene.from_json(v.at("scene"));
}

bool ProjectManifest::validate(std::string* err) const {
  auto fail = [&](const std::string& msg) {
    if (err) *err = msg;
    return false;
  };
  if (schema > kProjectSchemaVersion) {
    return fail("project uses schema " + std::to_string(schema) + ", this build reads up to " +
                std::to_string(kProjectSchemaVersion));
  }
  if (trim(name).empty()) return fail("manifest is missing a project name");
  if (name.size() > kMaxNameLength * 4) return fail("project name is implausibly long");
  ProjectKind parsed = ProjectKind::Generic3D;
  if (!kind.empty() && !project_kind_from(kind, &parsed)) {
    return fail("unknown project kind \"" + kind + "\"");
  }
  if (scene.up_axis != "y" && scene.up_axis != "z") return fail("scene.upAxis must be y or z");
  for (const std::string& tag : tags) {
    if (tag.empty() || tag.size() > 32) return fail("tag \"" + tag + "\" is invalid");
  }
  return true;
}

// ---- ProjectRef ---------------------------------------------------------------------

fs::Path ProjectRef::thumbnail_path() const {
  if (thumbs_dir.empty()) return {};
  return thumbs_dir / (manifest.id + "_" + std::to_string(kThumbnailWidth) + ".png");
}

fs::Path ProjectRef::autosave_dir() const { return dir / "autosave"; }

fs::Path ProjectRef::autosave_path() const {
  return autosave_dir() / (manifest.id.empty() ? std::string("autosave") : manifest.id);
}

std::string ProjectRef::folder_name() const { return dir.filename().string(); }

std::string ProjectRef::age_label(std::int64_t now) const {
  return ProjectManager::humanize_age(last_opened, now);
}

// ---- manager ------------------------------------------------------------------------

ProjectManager::ProjectManager(fs::Path root) : root_(std::move(root)) {
  if (root_.empty()) root_ = fs::default_data_dir("Lume", "hub");
  settings_ = HubSettings::load(settings_path());
  if (settings_.templates_root.empty()) {
    const fs::Path beside = root_.parent_path() / "templates";
    if (fs::is_directory(beside)) settings_.templates_root = beside;
  }
}

fs::Path ProjectManager::projects_dir() const {
  return settings_.projects_root.empty() ? root_ / "projects" : settings_.projects_root;
}

fs::Path ProjectManager::thumbnails_dir() const { return root_ / "thumbs"; }
fs::Path ProjectManager::trash_dir() const { return root_ / "trash"; }
fs::Path ProjectManager::recent_path() const { return root_ / kRecentFileName; }
fs::Path ProjectManager::settings_path() const { return root_ / kSettingsFileName; }

bool ProjectManager::save_settings(std::string* err) const {
  return settings_.save(settings_path(), err);
}

void ProjectManager::set_projects_root(const fs::Path& root) {
  settings_.projects_root = root;
  fs::create_directories(root / "projects");
  std::string err;
  if (!save_settings(&err)) LUME_LOG_WARN << "could not persist hub settings: " << err;
}

std::string ProjectManager::sanitize_name(const std::string& raw, std::string* err) {
  std::string out;
  out.reserve(raw.size());
  for (unsigned char c : raw) {
    if (c < 0x20) continue;  // control characters never survive into a name
    // Reserved on Windows; harmless on the other two, so we reject them everywhere.
    if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' ||
        c == '?' || c == '*') {
      out.push_back('-');
      continue;
    }
    out.push_back(static_cast<char>(c));
  }
  out = trim(out);
  while (out.size() > kMaxNameLength) {
    out.erase(out.begin() + static_cast<std::ptrdiff_t>(kMaxNameLength), out.end());
    out = trim(out);
  }
  if (out.empty() || out == "." || out == "..") {
    if (err) *err = "a project name needs at least one letter or digit";
    return {};
  }
  return out;
}

std::string ProjectManager::new_id(const std::string& seed) {
  return fs::hex(fs::hash64(seed), 16);
}

std::string ProjectManager::humanize_age(std::int64_t then, std::int64_t now) {
  if (then <= 0) return "never opened";
  std::int64_t delta = now - then;
  if (delta < 0) delta = 0;
  if (delta < 60) return "just now";
  if (delta < 3600) return std::to_string(delta / 60) + "m ago";
  if (delta < 86400) return std::to_string(delta / 3600) + "h ago";
  if (delta < 86400 * 30) return std::to_string(delta / 86400) + "d ago";
  if (delta < 86400 * 365) return std::to_string(delta / (86400 * 30)) + "mo ago";
  return std::to_string(delta / (86400 * 365)) + "y ago";
}

bool ProjectManager::name_taken(const std::string& name) const {
  for (const ProjectRef& ref : scan()) {
    if (iequals(ref.manifest.name, name)) return true;
  }
  return false;
}

fs::Path ProjectManager::project_dir_for(const std::string& name) const {
  const std::string slug = fs::slugify(name);
  return projects_dir() / (slug.empty() ? std::string("project") : slug);
}

bool ProjectManager::read_ref(const fs::Path& dir, ProjectRef* out, std::string* err,
                              ProjectError* code) const {
  auto fail = [&](ProjectError c, const std::string& msg) {
    if (code) *code = c;
    if (err) *err = msg;
    return false;
  };
  if (!out) return fail(ProjectError::Io, "no output slot");
  if (!fs::is_directory(dir)) return fail(ProjectError::NotADirectory, "not a folder: " + dir.string());

  ProjectRef ref;
  ref.dir = dir;
  ref.thumbs_dir = thumbnails_dir();
  ref.manifest_path = dir / kManifestFileName;

  std::string text;
  if (!fs::read_text(ref.manifest_path, &text, err)) {
    return fail(ProjectError::NotFound, "no " + std::string(kManifestFileName) + " in " + dir.string());
  }
  json::Value v;
  std::string parse_err;
  if (!json::Value::parse(text, &v, &parse_err)) {
    return fail(ProjectError::BadJson, ref.manifest_path.string() + ": " + parse_err);
  }
  ref.raw_manifest = v;
  ref.manifest.from_json(v);
  if (ref.manifest.name.empty()) ref.manifest.name = fs::stem(dir);  // recover, do not dead-end
  if (ref.manifest.id.empty()) ref.manifest.id = fs::hex(fs::hash64(fs::to_generic_string(dir)), 16);

  std::string v_err;
  if (!ref.manifest.validate(&v_err)) {
    const ProjectError c = ref.manifest.schema > kProjectSchemaVersion
                               ? ProjectError::UnsupportedSchema
                               : ProjectError::MissingField;
    return fail(c, ref.manifest_path.string() + ": " + v_err);
  }

  // Machine-local state lives beside the manifest and is never committed.
  json::Value local;
  std::string local_text;
  std::string local_err;
  if (fs::read_text(dir / kLocalFileName, &local_text, &local_err) &&
      json::Value::parse(local_text, &local, &local_err) && local.is_object()) {
    ref.last_opened = json::get_int(local, "lastOpenedUnix", 0);
  }
  ref.has_autosave = !fs::list_files(ref.dir / "autosave").empty();
  ref.has_thumbnail = fs::exists(ref.thumbnail_path());

  *out = std::move(ref);
  return true;
}

bool ProjectManager::write_local_meta(const ProjectRef& ref, std::string* err) const {
  // Deliberately tiny and git-ignored: what this machine last did with the project.
  json::Value v = json::Value::make_object();
  v.set("lastOpened", json::Value(fs::format_utc(ref.last_opened)));
  v.set("lastOpenedUnix", json::Value(ref.last_opened));
  v.set("openedCount", json::Value(ref.last_opened > 0 ? 1 : 0));
  v.set("hubVersion", json::Value("0.1.0"));
  return fs::write_text(ref.dir / kLocalFileName, v.dump(2) + "\n", err);
}

bool ProjectManager::create(const std::string& name, ProjectKind kind, const fs::Path& template_dir,
                           ProjectRef* out, std::string* err, ProjectError* code) {
  auto fail = [&](ProjectError c, const std::string& msg) {
    if (code) *code = c;
    if (err) *err = msg;
    return false;
  };
  std::string why;
  const std::string clean = sanitize_name(name, &why);
  if (clean.empty()) return fail(ProjectError::NameEmpty, why);
  if (name_taken(clean)) {
    return fail(ProjectError::NameTaken, "\"" + clean + "\" already exists in " + projects_dir().string());
  }

  const std::int64_t now = fs::now_unix();
  const std::string id = new_id(clean + ":" + std::to_string(now));
  const std::string slug = fs::slugify(clean);
  const fs::Path dir = fs::unique_path(projects_dir(), slug + "-" + id.substr(0, 8), "");
  std::string mk_err;
  if (!fs::create_directories(dir, &mk_err)) return fail(ProjectError::Io, mk_err);
  for (const char* sub : kProjectSubdirs) {
    if (!fs::create_directories(dir / sub, &mk_err)) {
      return fail(ProjectError::Io, "could not create " + dir.string() + "/" + sub);
    }
  }

  ProjectManifest manifest;
  manifest.schema = kProjectSchemaVersion;
  manifest.id = id;
  manifest.name = clean;
  manifest.kind = to_string(kind);
  manifest.created_utc = fs::format_utc(now);
  manifest.modified_utc = manifest.created_utc;
  manifest.author = "local";

  if (!template_dir.empty()) {
    ProjectTemplate tmpl;
    std::string t_err;
    if (TemplateLibrary::load(template_dir, &tmpl, &t_err)) {
      manifest.kind = tmpl.kind;
      manifest.template_used = tmpl.id;
      manifest.description = tmpl.description;
      manifest.tags = tmpl.tags;
      manifest.scene.from_json(tmpl.scene);
      for (const std::string& rel : tmpl.copy) {
        std::string c_err;
        if (!fs::copy_directory(template_dir / rel, dir / rel, &c_err)) {
          LUME_LOG_WARN << "template \"" << tmpl.id << "\": " << c_err;
        }
      }
    } else {
      LUME_LOG_WARN << "template at " << fs::to_generic_string(template_dir)
                    << " is not usable: " << t_err;
    }
  }

  ProjectRef ref;
  ref.dir = dir;
  ref.thumbs_dir = thumbnails_dir();
  ref.manifest_path = dir / kManifestFileName;
  ref.manifest = manifest;
  ref.raw_manifest = manifest.to_json();

  std::string w_err;
  if (!fs::write_text(ref.manifest_path, ref.raw_manifest.dump(2) + "\n", &w_err)) {
    return fail(ProjectError::Io, "could not write manifest: " + w_err);
  }
  write_local_meta(ref, &w_err);

  if (out) *out = ref;
  LUME_LOG_INFO << "created project \"" << clean << "\" at " << fs::to_generic_string(dir);
  return true;
}

bool ProjectManager::open(const fs::Path& manifest_or_dir, ProjectRef* out, std::string* err,
                        ProjectError* code) {
  fs::Path dir = manifest_or_dir;
  if (fs::extension(dir) == ".json" || !fs::is_directory(dir)) dir = dir.parent_path();
  ProjectRef ref;
  if (!read_ref(dir, &ref, err, code)) return false;
  if (!out) return true;
  // Stamp before handing the ref over: touch_opened() works on its own copy, so assigning
  // afterwards would leave the caller with a stale last_opened (and the Recent tab sorted on
  // whatever meta.json happened to hold).
  ref.last_opened = fs::now_unix();
  std::string touch_err;
  touch_opened(ref, &touch_err);
  *out = ref;
  return true;
}

bool ProjectManager::touch_opened(const ProjectRef& ref_in, std::string* err) {
  ProjectRef ref = ref_in;
  ref.last_opened = fs::now_unix();
  std::string local_err;
  write_local_meta(ref, &local_err);

  json::Array entries;
  load_recent(&entries);
  const std::string mine = fs::to_generic_string(ref.manifest_path);
  json::Array kept;
  kept.reserve(entries.size() + 1);
  json::Value head = json::Value::make_object();
  head.set("path", json::Value(mine));
  head.set("lastOpened", json::Value(ref.last_opened));
  kept.push_back(std::move(head));
  for (const json::Value& e : entries) {
    if (!e.is_object()) continue;
    const std::string p = json::get_string(e, "path");
    if (p.empty() || p == mine) continue;
    if (!fs::exists(fs::Path(p))) continue;  // dead entries drop out here
    kept.push_back(e);
    if (kept.size() >= static_cast<std::size_t>(std::max(1, settings_.max_recent))) break;
  }
  if (!save_recent(kept) && err) *err = "opened, but the recent list could not be saved";
  return true;
}

bool ProjectManager::save_manifest(const ProjectRef& ref, const ProjectManifest& manifest,
                                   std::string* err) {
  // Preserve keys this build does not know about: unknown top-level keys are how future
  // steps (and plugins) survive a round-trip through an older editor.
  json::Value merged = ref.raw_manifest;
  if (!merged.is_object()) merged = json::Value::make_object();
  const json::Value fresh = manifest.to_json();
  for (const json::Member& m : fresh.members()) merged.set(m.first, m.second);
  std::string write_err;
  if (!fs::write_text(ref.manifest_path, merged.dump(2) + "\n", &write_err)) {
    if (err) *err = write_err;
    return false;
  }
  return true;
}

bool ProjectManager::save(const ProjectRef& ref, std::string* err) {
  ProjectManifest m = ref.manifest;
  m.schema = kProjectSchemaVersion;
  m.modified_utc = fs::format_utc(fs::now_unix());
  std::string v_err;
  if (!m.validate(&v_err)) {
    if (err) *err = v_err;
    return false;
  }
  if (!save_manifest(ref, m, err)) return false;
  std::string local_err;
  write_local_meta(ref, &local_err);
  return true;
}

bool ProjectManager::clone_into(const ProjectRef& ref, const std::string& clean_name,
                               const fs::Path& dest_root, ProjectRef* out, std::string* err) {
  const std::int64_t now = fs::now_unix();
  const std::string id = new_id(clean_name + ":" + std::to_string(now) + ":" + ref.manifest.id);
  const fs::Path dest = fs::unique_path(dest_root, fs::slugify(clean_name) + "-" + id.substr(0, 8), "");
  std::string copy_err;
  if (!fs::copy_directory(ref.dir, dest, &copy_err)) {
    if (err) *err = copy_err;
    return false;
  }

  ProjectRef clone;
  clone.dir = dest;
  clone.thumbs_dir = thumbnails_dir();
  clone.manifest_path = dest / kManifestFileName;
  clone.raw_manifest = ref.raw_manifest;
  clone.manifest = ref.manifest;
  clone.manifest.id = id;                 // new id: thumbnails and autosaves must not collide
  clone.manifest.name = clean_name;
  clone.manifest.created_utc = fs::format_utc(now);
  clone.manifest.modified_utc = clone.manifest.created_utc;
  if (!save_manifest(clone, clone.manifest, err)) {
    fs::remove_directory(dest);
    return false;
  }
  std::string local_err;
  write_local_meta(clone, &local_err);
  if (out) *out = clone;
  return true;
}

bool ProjectManager::duplicate(const ProjectRef& ref, const std::string& new_name, ProjectRef* out,
                              std::string* err) {
  const std::string name = new_name.empty() ? ref.manifest.name + " copy" : new_name;
  std::string why;
  const std::string clean = sanitize_name(name, &why);
  if (clean.empty()) {
    if (err) *err = why;
    return false;
  }
  if (!clone_into(ref, clean, projects_dir(), out, err)) return false;
  LUME_LOG_INFO << "duplicated "" << ref.manifest.name << "" as "" << clean << """;
  return true;
}

bool ProjectManager::save_as(const ProjectRef& ref, const std::string& new_name,
                             const fs::Path& dest_root, ProjectRef* out, std::string* err) {
  std::string why;
  const std::string clean = sanitize_name(new_name, &why);
  if (clean.empty()) {
    if (err) *err = why;
    return false;
  }
  if (!out) {
    // No output slot asked for: this is a rename in place, nothing is copied.
    ProjectRef renamed = ref;
    renamed.manifest.name = clean;
    return save(renamed, err);
  }
  const fs::Path root = dest_root.empty() ? projects_dir() : dest_root;
  std::string mk_err;
  if (!fs::create_directories(root, &mk_err)) {
    if (err) *err = mk_err;
    return false;
  }
  // The source stays exactly where it was; the hub's "current project" becomes the copy.
  if (!clone_into(ref, clean, root, out, err)) return false;
  std::string touch_err;
  touch_opened(*out, &touch_err);
  return true;
}

bool ProjectManager::rename(const ProjectRef& ref, const std::string& new_name, std::string* err) {
  std::string why;
  const std::string clean = sanitize_name(new_name, &why);
  if (clean.empty()) {
    if (err) *err = why;
    return false;
  }
  if (!iequals(clean, ref.manifest.name)) {
    for (const ProjectRef& other : scan()) {
      if (iequals(other.manifest.name, clean) && other.dir != ref.dir) {
        if (err) *err = "\"" + clean + "\" is already used by another project";
        return false;
      }
    }
  }
  ProjectRef updated = ref;
  updated.manifest.name = clean;
  return save(updated, err);
}

bool ProjectManager::remove(const ProjectRef& ref, std::string* err) {
  json::Array entries;
  load_recent(&entries);
  const std::string mine = fs::to_generic_string(ref.manifest_path);
  json::Array kept;
  for (const json::Value& e : entries) {
    if (e.is_object() && json::get_string(e, "path") == mine) continue;
    kept.push_back(e);
  }
  save_recent(kept);

  const fs::Path thumb = ref.thumbnail_path();
  if (fs::exists(thumb)) {
    std::string rm_err;
    fs::remove_file(thumb, &rm_err);  // orphaned thumbs are pruned by the hub on scan
  }

  if (settings_.move_delete_to_trash) {
    const std::int64_t now = fs::now_unix();
    const fs::Path dest =
        trash_dir() / (fs::slugify(ref.manifest.name) + "-" + fs::hex(static_cast<std::uint64_t>(now), 8));
    std::string move_err;
    fs::create_directories(trash_dir(), &move_err);
    if (fs::is_directory(trash_dir())) {
      std::error_code ec;
      std::filesystem::rename(ref.dir, dest, ec);
      if (!ec) {
        LUME_LOG_INFO << "moved \"" << ref.manifest.name << "\" to trash: " << fs::to_generic_string(dest);
        return true;
      }
      if (fs::copy_directory(ref.dir, dest, &move_err) && fs::remove_directory(ref.dir, &move_err)) {
        return true;
      }
    }
    if (err) *err = "could not move to trash: " + move_err;
    return false;
  }
  return fs::remove_directory(ref.dir, err);
}

bool ProjectManager::validate_dir(const fs::Path& dir, std::string* err) const {
  ProjectRef ref;
  return read_ref(dir, &ref, err, nullptr);
}

bool ProjectManager::inspect(const fs::Path& dir_or_manifest, ProjectRef* out, std::string* err) const {
  fs::Path dir = dir_or_manifest;
  if (fs::extension(dir) == ".json" || !fs::is_directory(dir)) dir = dir.parent_path();
  return read_ref(dir, out, err, nullptr);
}

std::vector<ProjectRef> ProjectManager::scan() const {
  std::vector<ProjectRef> out;
  for (const fs::Path& dir : fs::list_directories(projects_dir())) {
    ProjectRef ref;
    std::string err;
    if (read_ref(dir, &ref, &err, nullptr)) {
      out.push_back(std::move(ref));
    } else if (!err.empty()) {
      LUME_LOG_DEBUG << "skipping " << fs::to_generic_string(dir) << ": " << err;
    }
  }
  std::sort(out.begin(), out.end(), [](const ProjectRef& a, const ProjectRef& b) {
    return lower(a.manifest.name) < lower(b.manifest.name);
  });
  return out;
}

bool ProjectManager::load_recent(json::Array* out) const {
  if (!out) return false;
  std::string text;
  std::string err;
  if (!fs::read_text(recent_path(), &text, &err)) return false;
  json::Value v;
  if (!json::Value::parse(text, &v, &err) || !v.is_object()) return false;
  const json::Value& list = v.at("recent");
  if (!list.is_array()) return false;
  *out = list.elements();
  return true;
}

bool ProjectManager::save_recent(const json::Array& entries) const {
  json::Value v = json::Value::make_object();
  v.set("version", json::Value(static_cast<std::int64_t>(1)));
  v.set("updatedAt", json::Value(fs::format_utc(fs::now_unix())));
  v.set("recent", json::Value(entries));
  std::string err;
  if (!fs::write_text(recent_path(), v.dump(2) + "\n", &err)) {
    LUME_LOG_WARN << "could not save recent list: " << err;
    return false;
  }
  return true;
}

std::vector<ProjectRef> ProjectManager::recent() {
  json::Array entries;
  if (!load_recent(&entries)) return {};

  std::vector<ProjectRef> out;
  std::set<std::string> seen;
  json::Array live;
  for (const json::Value& e : entries) {
    if (!e.is_object()) continue;
    const std::string path = json::get_string(e, "path");
    if (path.empty() || !seen.insert(path).second) continue;
    ProjectRef ref;
    std::string err;
    if (!read_ref(fs::Path(path).parent_path(), &ref, &err, nullptr)) continue;
    ref.last_opened = json::get_int(e, "lastOpened", ref.last_opened);
    live.push_back(json::Value(e));
    out.push_back(std::move(ref));
  }
  if (live.size() != entries.size()) save_recent(live);  // persist the pruning
  return out;
}

void ProjectManager::clear_recent() {
  json::Array none;
  save_recent(none);
}

}  // namespace lume
