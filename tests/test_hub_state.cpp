#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "hub_state.hpp"
#include "hub_ui.hpp"
#include "lume/fs.hpp"
#include "lume/log.hpp"
#include "lume/project.hpp"
#include "test.hpp"
#include "theme.hpp"

using namespace lume;
using lume::hub::Card;
using lume::hub::CardKind;
using lume::hub::HubState;
using lume::hub::HubUi;
using lume::hub::SortBy;
using lume::hub::Tab;
using lume::hub::UiAction;
using lume::test::CHECK;
using lume::test::CHECK_EQ;
using lume::test::CHECK_MSG;
using lume::test::CHECK_STREQ;

namespace {

std::string title_at(const std::vector<Card>& cards, std::size_t i) {
  return i < cards.size() ? cards[i].title : std::string("<none>");
}

std::size_t index_of_title(const HubState& hub, const std::string& title) {
  const std::vector<Card>& cards = hub.cards();
  for (std::size_t i = 0; i < cards.size(); ++i) {
    if (cards[i].title == title) return i;
  }
  return cards.size();  // out of range: activate()/delete_at() must refuse, which is also a test
}

void test_empty_hub(ProjectManager& pm) {
  HubState hub(&pm, {});
  CHECK(hub.tab() == Tab::Recent);
  CHECK(hub.sort() == SortBy::LastOpened);
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  CHECK_MSG(hub.cards()[0].kind == CardKind::Notice, "an empty Recent tab explains itself");
  CHECK(!hub.cards()[0].actionable);
  CHECK(hub.activate(0).type == UiAction::Type::None);
  CHECK(hub.status_line().find("0 projects") != std::string::npos);
  // Indices past the end are refused rather than read.
  CHECK_EQ(index_of_title(hub, "nope"), hub.cards().size());
  CHECK(hub.activate(7).type == UiAction::Type::None);
  std::string err;
  CHECK(!hub.delete_at(7, &err));
  CHECK(!err.empty());
}

void test_cards_and_filters(ProjectManager& pm) {
  std::string err;
  ProjectRef grey, alpha, zulu;
  CHECK(pm.create("Grey Planet", ProjectKind::Generic3D, {}, &grey, &err));
  CHECK(pm.create("alpha rocks", ProjectKind::PhysicsLab, {}, &alpha, &err));
  CHECK(pm.create("Zulu", ProjectKind::VoxelKit, {}, &zulu, &err));
  grey.manifest.tags = {"terrain", "one-material"};
  grey.manifest.description = "volcanic, flat shaded";
  CHECK(pm.save(grey, &err));

  HubState hub(&pm, {});
  hub.set_tab(Tab::Projects);
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(3));
  CHECK_STREQ(title_at(hub.cards(), 0), "alpha rocks");
  CHECK_STREQ(title_at(hub.cards(), 2), "Zulu");

  hub.set_query("terrain");  // matches a tag, not the title
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  CHECK_STREQ(title_at(hub.cards(), 0), "Grey Planet");
  hub.set_query("TERRAIN");  // and is case-insensitive
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  hub.set_query("volcanic");  // description is searchable too
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  hub.set_query("voxel");  // kind is searchable
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  hub.set_query("nothing-matches-this");
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(0));
  hub.set_query("");
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(3));

  hub.set_sort(SortBy::Name);
  CHECK_STREQ(title_at(hub.cards(), 0), "alpha rocks");
  hub.set_sort(SortBy::LastModified);
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(3));

  // Badges: the kind for anything but a plain scene; tint is non-zero and note is the folder.
  bool saw_physics = false;
  for (const Card& c : hub.cards()) {
    if (c.badge == "physics") saw_physics = true;
    if (c.title == "Zulu") {
      CHECK(c.tint != 0u);
      // The note is the folder, which is slug + a short id - never the display name.
      CHECK(c.note.rfind("zulu-", 0) == 0);
    }
  }
  CHECK_MSG(saw_physics, "a physics-lab project must carry its kind badge");

  // An autosave file on disk must surface as a badge, because that is the whole point of it.
  CHECK(fs::write_text(grey.dir / "autosave" / (grey.manifest.id + ".lume"), "x", &err));
  HubState again(&pm, {});
  again.set_tab(Tab::Projects);
  bool saw_autosave = false;
  for (const Card& c : again.cards()) {
    if (c.title == "Grey Planet") saw_autosave = c.has_autosave && c.badge == "autosave";
  }
  CHECK_MSG(saw_autosave, err);

  // Tints are driven by the id, so they are identical across rebuilds, never per-frame noise.
  std::vector<std::uint32_t> t1, t2;
  for (const Card& c : hub.cards()) t1.push_back(c.tint);
  for (const Card& c : again.cards()) t2.push_back(c.tint);
  CHECK(!t1.empty());
  std::sort(t1.begin(), t1.end());
  std::sort(t2.begin(), t2.end());
  CHECK_EQ(t1, t2);
}

void test_recents_and_actions(ProjectManager& pm) {
  std::string err;
  HubState hub(&pm, {});
  CHECK(hub.create_project("Recent One", "empty-scene", nullptr, &err));
  CHECK(hub.create_project("Recent Two", {}, nullptr, &err));
  CHECK(hub.create_project("Recent Three", {}, nullptr, &err));
  CHECK_EQ(hub.count_in(Tab::Projects), static_cast<std::size_t>(3));

  // Creating is not opening: Recent still only explains itself.
  hub.set_tab(Tab::Recent);
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  CHECK(hub.cards()[0].kind == CardKind::Notice);

  hub.set_tab(Tab::Projects);
  const std::size_t two = index_of_title(hub, "Recent Two");
  CHECK(two < hub.cards().size());
  ProjectRef opened;
  CHECK_MSG(hub.open_at(two, &opened, &err), err);
  CHECK_STREQ(opened.manifest.name, "Recent Two");

  hub.set_tab(Tab::Recent);
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  CHECK_STREQ(title_at(hub.cards(), 0), "Recent Two");

  const UiAction act = hub.activate(0);
  CHECK(act.type == UiAction::Type::OpenProject);
  CHECK(!act.text.empty());
  CHECK(act.text.size() > std::string("project.json").size());

  CHECK(hub.secondary(0, 0).type == UiAction::Type::RevealInFolder);
  CHECK(hub.secondary(0, 1).type == UiAction::Type::DuplicateProject);
  CHECK(hub.secondary(0, 2).type == UiAction::Type::DeleteProject);
  CHECK(hub.secondary(999, 0).type == UiAction::Type::None);

  CHECK_MSG(hub.duplicate_at(0, &err), err);
  CHECK_EQ(hub.count_in(Tab::Projects), static_cast<std::size_t>(4));
  hub.set_tab(Tab::Projects);
  bool found_copy = false;
  for (const Card& c : hub.cards()) {
    if (c.title == "Recent Two copy") found_copy = true;
  }
  CHECK_MSG(found_copy, "a nameless duplicate must append \" copy\"");

  const std::size_t one = index_of_title(hub, "Recent One");
  CHECK_MSG(hub.rename_at(one, "Renamed Here", &err), err);
  bool found_rename = false;
  for (const Card& c : hub.cards()) {
    if (c.title == "Renamed Here") found_rename = true;
  }
  CHECK(found_rename);

  hub.set_query("Renamed");
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  CHECK_MSG(hub.delete_at(0, &err), err);
  CHECK_EQ(hub.count_in(Tab::Projects), static_cast<std::size_t>(3));

  // Deleting a template/notice card is refused, and so is a stale index.
  hub.set_query("");
  hub.set_tab(Tab::Templates);
  CHECK(!hub.delete_at(0, &err));
  CHECK(!err.empty());
}

void test_template_cards_and_create(ProjectManager& pm, const fs::Path& templates_root) {
  std::string err;
  HubState hub(&pm, templates_root);
  hub.set_tab(Tab::Templates);
  CHECK_EQ(hub.cards().size(), hub.count_in(Tab::Templates));
  CHECK(hub.cards().size() >= static_cast<std::size_t>(4));
  bool saw_empty = false;
  std::size_t voxel_index = 0;
  for (std::size_t i = 0; i < hub.cards().size(); ++i) {
    const Card& c = hub.cards()[i];
    CHECK(c.kind == CardKind::Template);
    CHECK(c.actionable);
    CHECK(!c.subtitle.empty());
    CHECK_EQ(c.note, std::string("starts empty"));
    if (c.id == "empty-scene") saw_empty = true;
    if (c.id == "voxel-kit") voxel_index = i;
  }
  CHECK_MSG(saw_empty, "the hub must offer an empty scene even with no template folder");

  const UiAction act = hub.activate(voxel_index);
  CHECK(act.type == UiAction::Type::CreateProject);
  CHECK_STREQ(act.text, "voxel-kit");

  ProjectRef made;
  CHECK_MSG(hub.create_project("Voxel Thing", "voxel-kit", &made, &err), err);
  CHECK_MSG(made.manifest.template_used == "voxel-kit" && made.manifest.kind == "voxel-kit" &&
                made.manifest.scene.grid_snap > 0.9f,
            "a built-in template's scene overrides must reach the manifest");
  CHECK_STREQ(made.manifest.name, "Voxel Thing");

  // Names that cannot become folders are refused before any IO.
  std::string taken_err;
  CHECK(!hub.create_project("Voxel Thing", "empty-scene", nullptr, &taken_err));
  CHECK(taken_err.find("already exists") != std::string::npos);
  CHECK(!hub.create_project("", "empty-scene", nullptr, &taken_err));
  CHECK(!hub.create_project("   ", "empty-scene", nullptr, &taken_err));
  CHECK(!hub.create_project("///", "empty-scene", nullptr, &taken_err));

  // Unknown template ids fall back to the settings' default kind instead of failing.
  ProjectRef loose;
  CHECK(hub.create_project("Loose Project", "not-a-template", &loose, &err));
  CHECK(loose.manifest.template_used.empty());  // nothing was applied, and it claims so

  hub.set_query("voxel");
  CHECK_EQ(hub.cards().size(), static_cast<std::size_t>(1));
  hub.set_query("");
}

void test_hub_ui_layout(ProjectManager& pm) {
  HubState hub(&pm, {});
  HubUi ui(&hub, 1280.0f, 720.0f);
  CHECK_EQ(ui.columns_for_width(1280.0f), 3);
  CHECK_EQ(ui.columns_for_width(640.0f), 1);
  CHECK_EQ(ui.columns_for_width(2560.0f), 4);  // capped: no twelve-across card soup
  CHECK_EQ(ui.columns_for_width(100.0f), 1);   // nonsense width still lays out
  ui.set_message("hello");
  CHECK_STREQ(ui.message(), "hello");
  CHECK(!ui.wants_quit());
  ui.request_quit();
  CHECK(ui.wants_quit());
#ifndef LUME_WITH_IMGUI
  // Headless: a frame is a no-op that asks for nothing. (With ImGui this needs a context and
  // a device, which is step 2's job - so the paint path is not exercised here.)
  CHECK(ui.frame(0.016f).type == UiAction::Type::None);
#endif
  // Theme tokens the mockup in assets/hub/hub-mockup.svg is drawn from.
  CHECK_EQ(theme::kCardW, 320.0f);
  CHECK(theme::kWindow < theme::kPanel);
  CHECK(theme::kPanel < theme::kCard);
}

void test_tab_and_sort_ids() {
  Tab tab = Tab::Community;
  CHECK(HubState::tab_from_id("projects", &tab));
  CHECK(tab == Tab::Projects);
  CHECK(HubState::tab_from_id("community", &tab));
  CHECK(!HubState::tab_from_id("nope", &tab));
  CHECK(!HubState::tab_from_id("", &tab));
  CHECK(!HubState::tab_from_id("recent", nullptr));
  CHECK_STREQ(HubState::tab_label(Tab::Templates), "Start");
  CHECK_STREQ(HubState::tab_id(Tab::Projects), "projects");

  SortBy sort = SortBy::Created;
  CHECK(HubState::sort_from_id("name", &sort));
  CHECK(sort == SortBy::Name);
  CHECK(HubState::sort_from_id("created", &sort));
  CHECK(sort == SortBy::Created);
  CHECK(!HubState::sort_from_id("random", &sort));

  Card probe;
  probe.query_blob = "grey planet terrain";
  CHECK(HubState::matches(probe, "planet"));
  CHECK(HubState::matches(probe, "GREY"));
  CHECK(!HubState::matches(probe, "cube"));
  CHECK(HubState::matches(probe, ""));
}

}  // namespace

int main() {
  lume::log::set_stderr(false);
  lume::log::set_min_level(lume::log::Level::Error);
  test::Scratch scratch("hub");

  test_tab_and_sort_ids();
  {
    ProjectManager pm(scratch.root() / "empty");
    test_empty_hub(pm);
  }
  {
    ProjectManager pm(scratch.root() / "filters");
    test_cards_and_filters(pm);
  }
  {
    ProjectManager pm(scratch.root() / "actions");
    test_recents_and_actions(pm);
  }
  {
    // A template root that does not exist: the hub still has to be able to make a project.
    ProjectManager pm(scratch.root() / "templates");
    test_template_cards_and_create(pm, scratch.root() / "no-such-templates");
    test_hub_ui_layout(pm);
  }

  return test::finish("test_hub_state");
}
