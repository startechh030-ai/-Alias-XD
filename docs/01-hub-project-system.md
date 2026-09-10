# Step 1 — the hub and the project system

A 3D tool is mostly file management pretending to be art. Step 1 builds that file management
properly, because every later step writes into it: the renderer reads `scene.render`, the mesh
layer writes `meshes/`, physics reads `scene.physics`, and a project that cannot survive being
copied between three platforms by hand is a project that will lose work.

Source: `core/include/lume/project.hpp`, `core/src/project.cpp`, `hub/src/hub_state.cpp`.

---

## Model

A **project is a directory**. It is valid the moment the manifest exists, and every other
folder is optional.

```
<projects>/grey-planet-1a2b3c4d/
  project.json        manifest: the only required file
  meshes/             .obj/.gltf authored or imported      (step 3+)
  textures/           png/ktx2                             (step 4)
  animations/         clips                                (step 5)
  imports/            untouched source drops (FBX etc.)    (step 7)
  scene/              node graph + materials               (step 3)
  autosave/           <id>.lume snapshots                  (step 9)
  meta.json           machine-local, git-ignored
```

The hub's own root holds what is *not* project state:

```
<root>/projects/ <root>/thumbs/ <root>/trash/ <root>/recent.json <root>/hub-settings.json
```

Rules the whole format follows from:

| Rule | Consequence in code |
| --- | --- |
| A folder is the unit of work | `scan()` = read every child dir; no registry, no database to corrupt |
| Paths inside a project are relative | `ProjectRef::dir` is only ever used for display and IO, never written to the manifest |
| Deleting is moving | `remove()` renames into `trash/`; `move_delete_to_trash: false` is the opt-out |
| An unknown key is somebody else's data | `save_manifest()` merges known fields over the parsed document instead of rewriting it |
| A corrupt project must not take the hub down | `scan()` skips anything that fails to validate; `open()` reports why |
| Never truncate on a failed write | `fs::write_text` writes `<file>.tmp` then renames |

---

## `project.json`

`schema: 1`. Any higher value is a hard read failure with `ProjectError::UnsupportedSchema` —
better a hub that says "open me with a newer build" than one that silently drops fields.

| field | type | default | notes |
| --- | --- | --- | --- |
| `schema` | int | `1` | read-time gate, never rewritten upward without a migration |
| `id` | 16 hex | derived | stable; keys `thumbs/<id>_320.png` and `autosave/<id>.lume` |
| `name` | string | — | display name, free-form, 1–64 chars after sanitising |
| `kind` | enum | `generic3d` | `generic3d` \| `model-import` \| `physics-lab` \| `animation-test` \| `voxel-kit` |
| `description` | string | `""` | searched |
| `author` | string | `untitled` | searched |
| `engineMin` | string | `0.1.0` | informational until step 10's updater exists |
| `template` | string | `""` | which card it came from |
| `created` / `modified` | UTC ISO-8601 | — | `modified` is stamped by `save()`, never by the hub UI |
| `tags` | string[] | `[]` | ≤ 32 chars each; searched |
| `scene.upAxis` | `y`\|`z` | `y` | clamped, not rejected — a typo must not orphan a project |
| `scene.unitsPerMeter` | >0 | `1.0` | |
| `scene.gridSize` / `gridSnap` | >0 | `10` / `0.25` | |
| `scene.fps` | 1–1000 | `60` | timeline rate; the render loop ignores it |
| `scene.autosave` / `autosaveMinutes` | bool / int | `true` / `5` | enforced in step 9 |
| `scene.render.*` | see header | `auto`, hdr, 4×MSAA | `msaa` outside {0,2,4,8} resets to 4 |
| `scene.physics.*` | see header | `builtin`, 60 Hz | `solver` is `builtin`\|`jolt`\|`off` |

Two deliberate choices worth keeping in mind while the rest of the engine grows:

- **`physics.fixedHz` lives in the project, not the runtime.** Step 6 may not vary it per
  scene or the determinism gate becomes meaningless.
- **`upAxis` is authored, not inferred.** The importer does not get to change it, which is
  why the `model-import` template sets `z` and the box stack in `physics-lab` sets `y`.

---

## Recents, settings, meta

`recent.json` — an ordered array of `{ "path": "<projects>/…/project.json", "lastOpened": n }`,
most recent first. Written by `touch_opened()`, read by `recent()`. **A read prunes it**: any
entry whose manifest no longer parses is dropped and the file is rewritten, so moving a project
by hand self-heals on the next launch instead of showing a dead card forever. `maxRecent` caps
the list (4–256).

`hub-settings.json` — per-machine, never per-project: `projectsRoot`, `templatesRoot`,
`maxRecent`, `confirmDelete`, `moveDeleteToTrash`, `showAutosaveBadge`, `accent`,
`defaultTemplate`, `defaultKind`, `lastTab`, `lastSort`. Unknown enum values are logged and
replaced with the default rather than failing the load: a typo in `accent: "neon"` must not
cost someone their recent list.

`meta.json` — written into the project folder, git-ignored: `lastOpened`, `lastOpenedUnix`,
`openedCount`, `hubVersion`. The manifest stays byte-identical when a project is merely
*looked at*, which is what makes it sane to keep projects in git.

---

## Templates

`templates/<id>/template.json` (`schema: 1`) — `id, name, description, kind, icon, tags, copy[], scene{}`.

`copy` lists paths relative to the template dir; `create()` copies them into the new project.
Resolution order:

1. Five built-ins are compiled in (`TemplateLibrary::builtin()`), so `New project` works on a
   fresh clone, and on Android before anything has synced.
2. A directory in the template root with the same `id` **replaces** the built-in of that name;
   a new id is appended. This is how a user overrides "Empty Scene" without forking the repo.
3. A template that fails to parse is skipped with a debug log — never fatal, because a broken
   folder must not empty the hub.

---

## The hub itself

`hub/src/hub_state.cpp` holds every behaviour the window will need, and knows nothing about
windows:

- **Tabs** — `Recent`, `My Projects`, `Start` (templates), `Community` (an honest disabled card
  until there is a server). The active tab and sort persist in `hub-settings.json`.
- **Search** — one case-insensitive substring over a per-card `query_blob` built from name,
  description, kind, author, tags and folder. `matches()` is pure so CI can assert that
  "terrain" finds a project by its tag, not by luck.
- **Sort** — `opened` (desc, never-opened last), `name`, `modified`, `created`. `created`/
  `modified` come from parsing the ISO-8601 stamps with a civil-from-days conversion; no
  `timegm`, because that is a GNU extension and the hub builds with MSVC too.
- **Cards** — `title / subtitle / badge / note / thumbnail / tint`. `badge` is `autosave` when
  `autosave/` is non-empty, else the kind for anything but a plain scene. `tint` is
  `hash(id) % 6` over six muted colours — stable across rebuilds, never per-frame.
- **Actions** — a card click yields `OpenProject`, or `CreateProject` with a template id.
  `secondary(i, 0|1|2)` yields reveal / duplicate / delete. That is the entire write surface;
  `hub_ui.cpp` cannot touch the filesystem directly even if someone tries.

### Theme (dark is the product)

Values below are `hub/src/theme.hpp`, and `assets/hub/hub-mockup.svg` is drawn from them.
Surfaces are four; anything softer than that in a mockup is a mistake.

| token | hex | used for |
| --- | --- | --- |
| `kWindow` | `0B0D10` | behind everything |
| `kPanel` | `111419` | top bar, sidebar, footer |
| `kCard` | `171B22` | card body |
| `kRaised` | `1E242D` | hover, selected fill |
| `kField` | `0E1116` | search and inputs |
| `kBorder` / `kBorderStrong` | `232A35` / `2E3846` | 1 px rules — borders do the work shadows cannot |
| `kTextHi/·/Dim/Faint` | `E9EDF3` `B9C2CF` `7C8796` `545E6C` | four text levels, no more |
| `kAccentTeal` | `4FD1C5` | default accent; `amber` `rose` `violet` are the other settings values |
| card | 320 × 208 | 160 px thumbnail, 16 px gaps, 24 px padding, radius 10 |
| chrome | 56 / 216 / 28 | top bar / sidebar / footer heights |

The grid reflows 1→4 columns; `HubUi::columns_for_width()` is pure arithmetic and unit-tested
(1280 → 3, 640 → 1, 2560 → 4 capped), because "cards overflow the window" is a layout bug
nobody catches in review.

---

## Android notes (step 1 part)

`lume_hub_logic` and `lume_core` build for `arm64-v8a` today, and CI proves the objects are
really AArch64. `fs::default_data_dir()` reads `LUME_DATA_DIR` on Android; the Kotlin layer in
step 8 sets it to the app-private directory, which is where the hub keeps `projects/`, so the
same manager code works with no storage permission. The `.exe` target is `EXCLUDE_FROM_ALL`
under Android — no window, no APK entry point yet.

## Coverage

`tests/test_project.cpp` — JSON round-trips and refusal of malformed input; name sanitising
(Windows-reserved characters, control bytes, blank, over-long); create/refuse-taken-name/open/
save/duplicate/save-as/rename/delete-to-trash/permanent-delete; foreign-key preservation across
`save()`; recents order, cap and self-healing prune; scan ordering; corrupt manifest, future
schema, missing-but-guessable name, bad `kind`; clamping of out-of-range scene values; settings
round-trip and junk degradation; template override/seed-copy/scene-merge.

`tests/test_hub_state.cpp` — empty-tab explanation; card building; search by tag and by kind;
case-insensitivity; sort orders; badge from a real `autosave/` file; stable tints; recent-vs-open
distinction (creating is not opening); every action's index bounds; template cards with no
template directory on disk; layout arithmetic; tab/sort id round-trips.
