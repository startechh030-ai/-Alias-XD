#include "lume/json.hpp"
#include "lume/log.hpp"
#include "lume/project.hpp"
#include "lume/templates.hpp"
#include "test.hpp"

using namespace lume;
using lume::test::CHECK;
using lume::test::CHECK_EQ;
using lume::test::CHECK_MSG;
using lume::test::CHECK_STREQ;
using lume::test::CHECK_NEAR;

namespace {

void test_json_basics() {
  std::string err;
  json::Value v;
  const std::string text = R"({
    "schema": 1,
    "name": "Grey \"planet\" \u00e9",
    "nested": { "list": [1, 2.5, true, null, "x"] },
    "empty": {},
    "arr": [],
    "neg": -3
  })";
  CHECK_MSG(json::Value::parse(text, &v, &err), "parse: " + err);
  CHECK_EQ(json::get_int(v, "schema", -1), 1);
  CHECK_STREQ(json::get_string(v, "name"), "Grey \"planet\" \xc3\xa9");
  CHECK_EQ(v.at("nested").at("list").size(), static_cast<std::size_t>(5));
  CHECK_NEAR(v.at("nested").at("list").elements()[1].as_number(), 2.5, 1e-9);
  CHECK(v.at("empty").is_object() && v.at("empty").size() == 0);
  CHECK_EQ(json::get_int(v, "neg", 0), -3);
  CHECK(v.at("missing").is_null());
  CHECK_STREQ(v.at("missing").as_string(), "");

  // Round-trip must be stable, and unknown keys must survive it.
  const std::string dumped = v.dump(2);
  json::Value again;
  CHECK_MSG(json::Value::parse(dumped, &again, &err), "reparse: " + err);
  CHECK_STREQ(again.dump(2), dumped);

  json::Value patched = again;
  patched.set("extra", json::Value(static_cast<std::int64_t>(7)));
  CHECK_EQ(json::get_int(patched, "extra", 0), 7);

  // Malformed input reports instead of exploding.
  json::Value junk;
  CHECK(!json::Value::parse("{\"a\": }", &junk, &err));
  CHECK(!err.empty());
  CHECK(!json::Value::parse("{\"a\": 1", &junk, &err));
  CHECK(!json::Value::parse("{\"a\": 1}}", &junk, &err));
  CHECK(!json::Value::parse("", &junk, &err));
  CHECK(!json::Value::parse("{\"a\": tru}", &junk, &err));

  // Numbers that must not be written as "1.000000" or as garbage.
  json::Value nums = json::Value::make_object();
  nums.set("i", json::Value(static_cast<std::int64_t>(40)));
  nums.set("f", json::Value(0.25));
  CHECK_STREQ(nums.dump_compact(), "{\"i\":40,\"f\":0.25}");
}

void test_names_and_ids() {
  std::string err;
  CHECK_STREQ(ProjectManager::sanitize_name("  My   Level!!  "), "My Level!!");
  // Reserved-on-Windows characters become '-', so the same folder works everywhere.
  CHECK_STREQ(ProjectManager::sanitize_name("a<b>:c\"d/e\\f|g?h*i"), "a-b--c-d-e-f-g-h-i");
  CHECK_STREQ(ProjectManager::sanitize_name("keep\ttabs\nand lines"), "keep tabs and lines");
  CHECK_MSG(ProjectManager::sanitize_name("   ", &err).empty(), "blank must not become a name");
  CHECK(!err.empty());
  CHECK(ProjectManager::sanitize_name(".", &err).empty());
  CHECK(ProjectManager::sanitize_name("..", &err).empty());
  const std::string long_name = ProjectManager::sanitize_name(std::string(300, 'n'));
  CHECK_EQ(long_name.size(), static_cast<std::size_t>(64));

  CHECK_STREQ(fs::slugify("My Level!! 2"), "my-level-2");
  CHECK_STREQ(fs::slugify("-- weird ___ name --"), "weird-name");
  CHECK_EQ(fs::hash64("a") != fs::hash64("b"), true);
  const std::string id = ProjectManager::new_id("seed");
  CHECK_EQ(id.size(), static_cast<std::size_t>(16));
  CHECK_EQ(id, ProjectManager::new_id("seed"));

  // Age strings the hub prints on every card.
  const std::int64_t now = 1700000000;
  CHECK_STREQ(ProjectManager::humanize_age(0, now), "never opened");
  CHECK_STREQ(ProjectManager::humanize_age(now - 5, now), "just now");
  CHECK_STREQ(ProjectManager::humanize_age(now - 120, now), "2m ago");
  CHECK_STREQ(ProjectManager::humanize_age(now - 7200, now), "2h ago");
  CHECK_STREQ(ProjectManager::humanize_age(now - 3 * 86400, now), "3d ago");
  CHECK_STREQ(ProjectManager::humanize_age(now + 500, now), "just now");
}

void test_lifecycle(const fs::Path& root) {
  std::string err;
  ProjectManager pm(root);
  CHECK_EQ(pm.scan().size(), static_cast<std::size_t>(0));

  ProjectRef made;
  CHECK_MSG(pm.create("Grey Planet", ProjectKind::Generic3D, {}, &made, &err), "create: " + err);
  CHECK(!made.manifest.id.empty());
  CHECK(fs::is_directory(made.dir));
  CHECK(fs::exists(made.manifest_path));
  for (const char* sub : {"meshes", "textures", "animations", "imports", "scene", "autosave"}) {
    CHECK_MSG(fs::is_directory(made.dir / sub), std::string("missing folder ") + sub);
  }
  CHECK_STREQ(made.manifest.kind, "generic3d");
  CHECK_EQ(made.manifest.schema, kProjectSchemaVersion);
  CHECK(!made.manifest.created_utc.empty());
  CHECK_STREQ(made.manifest.created_utc, made.manifest.modified_utc);

  // Same name twice is a refusal, not a suffix.
  std::string dup_err;
  ProjectError code = ProjectError::None;
  CHECK(!pm.create("grey planet", ProjectKind::Generic3D, {}, nullptr, &dup_err, &code));
  CHECK_MSG(code == ProjectError::NameTaken, dup_err);

  // An empty manifest is not a project.
  CHECK(!pm.create("   ", ProjectKind::Generic3D, {}, nullptr, &dup_err, &code));
  CHECK_MSG(code == ProjectError::NameEmpty, "blank name must be rejected before any IO");

  // Open, edit, save.
  ProjectRef ref;
  CHECK_MSG(pm.open(made.dir, &ref, &err), "open: " + err);
  CHECK_STREQ(ref.manifest.name, "Grey Planet");
  CHECK(ref.last_opened > 0);
  ref.manifest.description = "a rock and a light";
  ref.manifest.tags = {"grey", "test"};
  ref.manifest.scene.grid_snap = 0.5f;
  ref.manifest.scene.physics.solver = "jolt";
  ref.manifest.scene.render.msaa = 8;
  CHECK_MSG(pm.save(ref, &err), "save: " + err);

  ProjectRef reloaded;
  CHECK(pm.open(made.dir, &reloaded, &err));
  CHECK_STREQ(reloaded.manifest.description, "a rock and a light");
  CHECK_EQ(reloaded.manifest.tags.size(), static_cast<std::size_t>(2));
  CHECK_NEAR(reloaded.manifest.scene.grid_snap, 0.5, 1e-6);
  CHECK_STREQ(reloaded.manifest.scene.physics.solver, "jolt");
  CHECK_EQ(reloaded.manifest.scene.render.msaa, 8);

  // Unknown top-level keys belong to the future and must survive a round trip.
  {
    json::Value raw;
    std::string text;
    CHECK(fs::read_text(reloaded.manifest_path, &text, &err));
    CHECK(json::Value::parse(text, &raw, &err));
    raw.set("lumeStep2TODO", json::Value("keep me"));
    CHECK(fs::write_text(reloaded.manifest_path, raw.dump(2) + "\n", &err));
  }
  ProjectRef with_future_key;
  CHECK(pm.open(made.dir, &with_future_key, &err));
  CHECK_STREQ(with_future_key.raw_manifest.at("lumeStep2TODO").as_string(), "keep me");
  CHECK(pm.save(with_future_key, &err));
  json::Value after;
  std::string after_text;
  CHECK(fs::read_text(with_future_key.manifest_path, &after_text, &err));
  CHECK(json::Value::parse(after_text, &after, &err));
  CHECK_MSG(after.at("lumeStep2TODO").as_string() == "keep me", "save must not drop foreign keys");

  // Rename changes the manifest, not the folder (folder names are ids, not titles).
  const std::string folder_before = made.dir.filename().string();
  CHECK_MSG(pm.rename(with_future_key, "Grey Planet II", &err), "rename: " + err);
  ProjectRef renamed;
  CHECK(pm.open(made.dir, &renamed, &err));
  CHECK_STREQ(renamed.manifest.name, "Grey Planet II");
  CHECK_STREQ(renamed.dir.filename().string(), folder_before);

  // Duplicate gets a fresh id and name; the source is untouched.
  ProjectRef copy;
  CHECK_MSG(pm.duplicate(renamed, "Grey Planet Copy", &copy, &err), "duplicate: " + err);
  CHECK(copy.dir != renamed.dir);
  CHECK(!copy.manifest.id.empty() && copy.manifest.id != renamed.manifest.id);
  CHECK_STREQ(copy.manifest.name, "Grey Planet Copy");
  CHECK_STREQ(copy.manifest.description, "a rock and a light");
  CHECK(fs::is_directory(renamed.dir));

  // Save As into another root creates the copy and leaves the original alone.
  const fs::Path other = root / "elsewhere";
  ProjectRef moved;
  CHECK_MSG(pm.save_as(copy, "Far Project", other, &moved, &err), "save_as: " + err);
  CHECK(fs::is_directory(moved.dir));
  CHECK(moved.dir.parent_path() == other);
  CHECK(fs::is_directory(copy.dir));
  ProjectRef far;
  CHECK(pm.open(moved.dir, &far, &err));
  CHECK_STREQ(far.manifest.name, "Far Project");
}

void test_recents_scan_and_delete(const fs::Path& root) {
  std::string err;
  ProjectManager pm(root);
  ProjectRef a, b, c;
  CHECK(pm.create("Alpha", ProjectKind::Generic3D, {}, &a, &err));
  CHECK(pm.create("Bravo", ProjectKind::PhysicsLab, {}, &b, &err));
  CHECK(pm.create("Charlie", ProjectKind::VoxelKit, {}, &c, &err));

  ProjectRef ignored;
  CHECK(pm.open(b.dir, &ignored, &err));
  CHECK(pm.open(a.dir, &ignored, &err));
  CHECK(pm.open(c.dir, &ignored, &err));

  std::vector<ProjectRef> recents = pm.recent();
  CHECK_EQ(recents.size(), static_cast<std::size_t>(3));
  CHECK_MSG(recents.size() == 3 && recents[0].manifest.name == "Charlie", "most recent first");
  CHECK_MSG(fs::exists(pm.recent_path()), "recent.json must exist after an open");

  // scan() is name-ordered and sees everything, opened or not.
  const std::vector<ProjectRef> all = pm.scan();
  CHECK_EQ(all.size(), static_cast<std::size_t>(3));
  CHECK_STREQ(all[0].manifest.name, "Alpha");
  CHECK_STREQ(all[2].manifest.name, "Charlie");

  // A project removed to the trash leaves the recent list, and can be fished out.
  ProjectRef b_ref;
  CHECK(pm.inspect(b.manifest_path.parent_path(), &b_ref, &err));
  CHECK(pm.remove(b_ref, &err));
  CHECK(!fs::is_directory(b.dir));
  CHECK_EQ(fs::list_directories(pm.trash_dir()).size(), static_cast<std::size_t>(1));
  CHECK_EQ(pm.recent().size(), static_cast<std::size_t>(2));
  CHECK_EQ(pm.scan().size(), static_cast<std::size_t>(2));
  CHECK(!pm.name_taken("Bravo"));
  CHECK(pm.name_taken("alpha"));  // case-insensitive

  // A folder that vanished behind the hub's back is pruned from recent on read.
  fs::remove_directory(a.dir, &err);
  const std::vector<ProjectRef> after = pm.recent();
  CHECK_EQ(after.size(), static_cast<std::size_t>(1));
  CHECK(!pm.recent_path().empty());
  CHECK(fs::exists(pm.recent_path()));

  // Permanent delete has to actually delete.
  ProjectManager pm2(root);
  pm2.settings().move_delete_to_trash = false;
  ProjectRef c_ref;
  CHECK(pm2.inspect(c.manifest_path.parent_path(), &c_ref, &err));
  CHECK(pm2.remove(c_ref, &err));
  CHECK(!fs::is_directory(c.dir));
  CHECK_EQ(pm2.scan().size(), static_cast<std::size_t>(0));

  // clear_recent leaves projects alone.
  ProjectManager pm3(root);
  ProjectRef fresh;
  CHECK(pm3.create("Delta", ProjectKind::Generic3D, {}, &fresh, &err));
  ProjectRef tmp;
  CHECK(pm3.open(fresh.dir, &tmp, &err));
  CHECK_EQ(pm3.recent().size(), static_cast<std::size_t>(1));
  pm3.clear_recent();
  CHECK_EQ(pm3.recent().size(), static_cast<std::size_t>(0));
  CHECK_EQ(pm3.scan().size(), static_cast<std::size_t>(1));
}

void test_broken_projects(const fs::Path& root) {
  std::string err;
  ProjectManager pm(root);
  ProjectRef ok;
  CHECK(pm.create("Broken", ProjectKind::Generic3D, {}, &ok, &err));

  const fs::Path dir = ok.dir;
  CHECK(fs::write_text(dir / kManifestFileName, "{ this is not json", &err));
  ProjectError code = ProjectError::None;
  ProjectRef out;
  CHECK(!pm.open(dir, &out, &err, &code));
  CHECK_MSG(code == ProjectError::BadJson, err);
  CHECK_EQ(pm.scan().size(), static_cast<std::size_t>(0));  // a hub must not die on one bad file

  CHECK(fs::write_text(dir / kManifestFileName, R"({"schema":99,"name":"Future"})", &err));
  code = ProjectError::None;
  CHECK(!pm.open(dir, &out, &err, &code));
  CHECK_MSG(code == ProjectError::UnsupportedSchema, err);
  CHECK(err.find("schema") != std::string::npos);

  // Missing fields are repaired when they are guessable, refused when they are not.
  CHECK(fs::write_text(dir / kManifestFileName, R"({"schema":1,"kind":"physics-lab"})", &err));
  CHECK(pm.open(dir, &out, &err));
  CHECK_STREQ(out.manifest.name, fs::stem(dir));

  CHECK(fs::write_text(dir / kManifestFileName, R"({"schema":1,"name":"X","kind":"wat"})", &err));
  CHECK(!pm.open(dir, &out, &err));

  // A directory that is not a project at all.
  const fs::Path not_a_project = root / "random-folder";
  fs::create_directories(not_a_project, &err);
  CHECK(!pm.open(not_a_project, &out, &err));
  CHECK(!pm.validate_dir(not_a_project, &err));

  // Out-of-range values clamp rather than corrupt the scene.
  json::Value scene = json::Value::make_object();
  scene.set("render", [] {
    json::Value r = json::Value::make_object();
    r.set("msaa", json::Value(static_cast<std::int64_t>(7)));
    r.set("near", json::Value(-1.0f));
    r.set("far", json::Value(-2.0f));
    return r;
  }());
  scene.set("fps", json::Value(999999.0));
  scene.set("upAxis", json::Value("q"));
  SceneSettings s;
  s.from_json(scene);
  CHECK_EQ(s.render.msaa, 4);
  CHECK(s.render.near_z > 0.0f);
  CHECK(s.render.far_z > s.render.near_z);
  CHECK_NEAR(s.fps, 60.0, 1e-9);
  CHECK_STREQ(s.up_axis, "y");
}

void test_settings(const fs::Path& root) {
  std::string err;
  {
    ProjectManager pm(root);
    pm.settings().accent = "rose";
    pm.settings().max_recent = 3;
    pm.settings().confirm_delete = false;
    pm.settings().last_tab = "templates";
    pm.settings().last_sort = "name";
    pm.settings().default_template = "physics-lab";
    CHECK_MSG(pm.save_settings(&err), "save_settings: " + err);
    CHECK(fs::exists(pm.settings_path()));
  }
  {
    ProjectManager pm(root);
    CHECK_STREQ(pm.settings().accent, "rose");
    CHECK_EQ(pm.settings().max_recent, 3);
    CHECK_EQ(pm.settings().confirm_delete, false);
    CHECK_STREQ(pm.settings().last_tab, "templates");
    CHECK_STREQ(pm.settings().default_template, "physics-lab");
  }
  // Junk in the settings file degrades to defaults, loudly but safely.
  CHECK(fs::write_text(root / kSettingsFileName, R"({"accent":"neon","maxRecent":99999})", &err));
  const HubSettings loaded = HubSettings::load(root / kSettingsFileName);
  CHECK_STREQ(loaded.accent, "teal");
  CHECK(loaded.max_recent > 0 && loaded.max_recent <= 256);
  CHECK(fs::write_text(root / kSettingsFileName, "not json at all", &err));
  CHECK_STREQ(HubSettings::load(root / kSettingsFileName).accent, "teal");

  // max_recent caps the Recent tab, oldest entries falling off.
  {
    ProjectManager pm(root);
    pm.settings().max_recent = 2;
    ProjectRef p1, p2, p3;
    CHECK(pm.create("Cap One", ProjectKind::Generic3D, {}, &p1, &err));
    CHECK(pm.create("Cap Two", ProjectKind::Generic3D, {}, &p2, &err));
    CHECK(pm.create("Cap Three", ProjectKind::Generic3D, {}, &p3, &err));
    ProjectRef tmp;
    CHECK(pm.open(p1.dir, &tmp, &err));
    CHECK(pm.open(p2.dir, &tmp, &err));
    CHECK(pm.open(p3.dir, &tmp, &err));
    CHECK_EQ(pm.recent().size(), static_cast<std::size_t>(2));
    CHECK(pm.name_taken("Cap One"));  // still a project, just not recent
  }
}

void test_templates(const fs::Path& root) {
  std::string err;
  const std::vector<ProjectTemplate> builtins = TemplateLibrary::builtin();
  CHECK(builtins.size() >= 4);
  bool has_empty = false, has_physics = false;
  for (const ProjectTemplate& t : builtins) {
    if (t.id == "empty-scene") has_empty = true;
    if (t.id == "physics-lab") {
      has_physics = true;
      CHECK_STREQ(t.kind, "physics-lab");
      CHECK(t.scene.is_object());
    }
  }
  CHECK(has_empty && has_physics);

  // A local template on disk overrides the built-in of the same id and adds new ones.
  const fs::Path tdir = root / "templates";
  ProjectTemplate custom;
  custom.dir = tdir / "my-kit";
  custom.id = "my-kit";
  custom.name = "My Kit";
  custom.description = "with seed assets";
  custom.kind = "voxel-kit";
  custom.icon = "grid";
  custom.copy = {"meshes"};
  custom.scene = json::Value::make_object();
  custom.scene.set("upAxis", json::Value("z"));
  CHECK_MSG(TemplateLibrary::write(custom, &err), "write template: " + err);
  CHECK(fs::exists(custom.dir / "template.json"));

  ProjectTemplate empty_override;
  empty_override.dir = tdir / "empty-scene";
  empty_override.id = "empty-scene";
  empty_override.name = "Blank Instead";
  empty_override.kind = "generic3d";
  CHECK(TemplateLibrary::write(empty_override, &err));

  TemplateLibrary lib(tdir);
  const std::vector<ProjectTemplate> found = lib.discover();
  CHECK(lib.find("my-kit", &custom));
  CHECK_STREQ(custom.name, "My Kit");
  CHECK_EQ(custom.copy.size(), static_cast<std::size_t>(1));
  ProjectTemplate overridden;
  CHECK(lib.find("empty-scene", &overridden));
  CHECK_STREQ(overridden.name, "Blank Instead");
  CHECK(!overridden.is_builtin());
  CHECK(found.size() >= 3);
  CHECK(!lib.find("nope", &overridden));
  CHECK(lib.ids().size() >= 3);

  // A template dir with junk inside is skipped, not fatal.
  fs::create_directories(tdir / "garbage", &err);
  CHECK(fs::write_text(tdir / "garbage" / "template.json", "][", &err));
  CHECK(lib.discover().size() == found.size());

  // Creating from a template copies its seed assets and keeps its kind.
  ProjectManager pm(root);
  fs::create_directories(custom.dir / "meshes", &err);
  CHECK(fs::write_text(custom.dir / "meshes" / "kit.obj", "v 0 0 0\n", &err));
  ProjectRef made;
  CHECK_MSG(pm.create("From Template", ProjectKind::VoxelKit, custom.dir, &made, &err),
            "create from template: " + err);
  CHECK_STREQ(made.manifest.kind, "voxel-kit");
  CHECK_STREQ(made.manifest.template_used, "my-kit");
  CHECK(fs::exists(made.dir / "meshes" / "kit.obj"));
  CHECK_STREQ(made.manifest.scene.up_axis, "z");
  CHECK(made.manifest.scene.grid_snap > 0.0f);
}

}  // namespace

int main() {
  lume::log::set_stderr(false);
  lume::log::set_min_level(lume::log::Level::Error);
  test::Scratch scratch("project");
  const fs::Path root = scratch.root();

  // Each group gets its own hub root: no test can be confused by another's projects.
  test_json_basics();
  test_names_and_ids();
  test_lifecycle(root / "lifecycle");
  test_recents_scan_and_delete(root / "recents");
  test_broken_projects(root / "broken");
  test_settings(root / "settings");
  test_templates(root / "templates");

  return test::finish("test_project");
}
