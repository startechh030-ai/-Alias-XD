# Lume

A lightweight 3D creation tool — the Blender-adjacent, MVP-first cousin. One engine core,
three targets: **Android, Windows, Linux**. No macOS, no iOS.

This repository currently contains **step 1 of the plan: the hub / project system.**
Nothing else is implemented on purpose. See [docs/roadmap.md](docs/roadmap.md).

```
Lume = hub (project system)  ->  renderer (Filament)  ->  geometry (libigl / meshops)
       -> textures + UV  ->  animation  ->  physics  ->  import/export (Assimp, glTF)
```

## Ground rules for this repo

| Rule | Why |
| --- | --- |
| One step at a time | The plan in `docs/roadmap.md` is sequential. A step lands complete or it does not land. |
| Nothing is built on the authoring machine | The dev box cannot compile Filament. **GitHub Actions does every compile.** |
| The workspace holds only code and assets | `.cpp/.hpp/.cmake/.json/.svg/.md`. No `build/`, no object files, no binaries. |
| The core is dependency-free | `lume-core` compiles with just a C++17 compiler, so CI can test logic on every push in seconds. |
| Renderer sits behind one seam | `lume-core` never includes a Filament header. Only `LUME_ENABLE_FILAMENT=ON` targets link it. |
| Dark by default | The hub theme is the product theme, not a toggle. Tokens live in `hub/src/theme.hpp`. |

## Layout

```
CMakeLists.txt              options + subdirectories
CMakePresets.json           ci-linux / ci-windows / android-arm64 (same presets CI uses)
core/                       lume-core: project files, JSON, filesystem, logging  (no deps)
  include/lume/
  src/
hub/                        lume-hub: the Roblox-Studio-style launcher window
  src/
tests/                      unit tests for the project system (run in CI, not locally)
.github/workflows/          ci.yml (linux+windows), android.yml (NDK arm64)
assets/                     SVG art: hub mockup, logo, UI icons
templates/                  new-project templates the hub offers as cards
docs/                       roadmap + the step 1 design spec
```

## Build (on a machine that can, or in CI)

```sh
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --preset ci-linux
```

Windows: `cmake --preset ci-windows`. Android (core only, until there is an APK):
`cmake --preset android-arm64`.

The hub binary in step 1 runs **headless**:

```sh
./build/ci-linux/bin/lume-hub --print --projects-dir ~/LumeProjects
```

It lists, filters, sorts, creates, opens, duplicates and deletes projects — and refuses to
open a window, because the window arrives in step 2 with Filament.

## Language plan

The stack you asked for, placed where it actually helps — and it is deliberately not all here
in step 1, because a binding written against a moving core becomes a second core.

| Language | Where | When |
| --- | --- | --- |
| **C++17** | `core/`, `hub/`, `render/` — everything that holds state | step 1, now |
| **D** | offline tooling: asset baker, `lume inspect` CLI, packer. Talks to the core through the C ABI, built with `dub` and hooked into the same CMake tree | step 3+ (opt-in, `LUME_BUILD_D_TOOLS`) |
| **Kotlin** | the Android shell: activity, surface, storage handoff (`LUME_DATA_DIR`), JNI bindings over the same C ABI | step 8 |

Step 1 keeps the C ABI out of the tree on purpose: `ProjectManager` is the seam, and the
`extern "C"` layer gets written once the object it wraps stops changing weekly.

## First push

```sh
git init -b main . && git add -A
git remote add origin git@github.com:<you>/lume.git
git commit -m "step 1: hub and project system"
git push -u origin main        # CI builds and tests Linux, Windows and Android arm64
```

Nothing needs to be installed locally. The Actions badge will be the fastest way to see
whether step 1 is green; `guardrails` also asserts that step 2's options refuse to configure
until they are implemented.

## Project format

A Lume project is a **folder**, not a blob. It survives being edited by hand, syncs cleanly
with git, and costs nothing to back up.

```
MyLevel/
  project.json          the manifest (schema versioned, the only required file)
  meshes/  textures/  animations/  imports/
  scene/                node graph + materials (written in step 3+)
  autosave/
```

Full schema and field-by-field rationale: [docs/01-hub-project-system.md](docs/01-hub-project-system.md).

## Status

- [x] **Step 1 — hub project system** (manifest, manager, hub state, dark theme, CI)
- [ ] Step 2 — Filament on Vulkan / GL ES 3 / Windows, shared render graph
- [ ] Step 3 — libigl mesh ops
- [ ] … [see roadmap](docs/roadmap.md)
# Animate-
# Animate-
