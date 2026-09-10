# Roadmap — one step at a time, built in CI

The rule for this project: **a step lands complete or it does not land.** No half-wired
renderer, no speculative third-party drop, no "I'll finish the mesh layer when physics is in".
Each step has an exit gate, and the gate is a GitHub Actions job, not a feeling.

Two permanent constraints:

1. **Nothing is compiled on the authoring machine.** It cannot build Filament. The workspace
   holds source and assets; every compile happens in CI.
2. **Three targets only: Android, Windows, Linux.** No macOS, no iOS. `lume_platform.cmake`
   fails the configure on `APPLE` so a stray preset cannot quietly half-build.

| Step | Deliverable | Gate (CI job that must go green) |
| --- | --- | --- |
| **1** | **Hub / project system** — folders, `project.json`, recents, templates, settings, dark hub state + text hub | `host-tests` (ctest on Linux + Windows), `android` (NDK arm64 objects), `guardrails` |
| 2 | **Filament on three platforms** — window, swapchain, Vulkan/GL-ES backends, texture-uploaded ImGui hub, scene viewport with orbit camera, thumbnails for cards | `renderer-linux`, `renderer-windows`, `renderer-android` + a real `lume-hub.apk` artifact |
| 3 | **libigl mesh layer** — load/store geometry, normals, weld, triangulate, subdivide, box-projected UVs, decimation for mobile | `geometry` (numeric tolerances asserted, same on all three) |
| 4 | **Textures + UV** — PNG/KTX2 import, `textures/` store, checker fallback, UV editor surface | `textures` |
| 5 | **Animation** — node tracks, quaternion slerp, clip playback in the viewport, `animations/` | `anim` |
| 6 | **Physics** — built-in solver first (it is 200 lines and enough for a sandbox), Jolt behind `scene.physics.solver` | `physics` + determinism check (same seed, same result on win/linux) |
| 7 | **Import/export** — Assimp (FBX/OBJ/PLY/glTF), glTF export via cgltf, `.lume` package format | `interop` (round-trip: import → export → re-import, diff the geometry) |
| 8 | **Android shell** — JNI surface plugin, Kotlin activity, touch camera, project picker reading the app-private dir | `apk` (assembles, installs on an emulator image in CI) |
| 9 | **Edit loop** — gizmos, snapping, undo/redo over the manifest + scene, autosave write-back | `edit` (a scripted session that edits, quits, reopens, asserts state) |
| 10 | **Packaging** — signed Windows/Linux binaries, Play-ready AAB, release workflow | `release` |

## What step 1 is

See [01-hub-project-system.md](01-hub-project-system.md). In one line: a project is a folder
with a versioned `project.json`, and the hub can list, filter, sort, open, create, duplicate,
rename and delete those folders — identically on all three platforms, headlessly, with no
dependencies.

## What step 1 deliberately is not

- No window, no viewport, no Filament. `hub_ui.cpp` contains the ImGui hub, behind
  `LUME_WITH_IMGUI`; configuring `-DLUME_ENABLE_FILAMENT=ON` today is a **hard error with an
  explanation**, and the `guardrails` job asserts that it stays one.
- No `.obj`/`.gltf` reading. `meshes/` exists as a folder and nothing more.
- No scene format. The manifest reserves `scene/`; step 3 defines what is inside it.
- No undo. Snapshots of `project.json` are trivial once the scene format exists; snapshotting
  a format that does not exist yet is not.
- No D bindings, no Kotlin module, no C ABI. `lume-project` (the `extern "C"` surface the D
  tools and the Kotlin shell will share) is defined in step 3, once `ProjectRef` stops moving.
  Binding a core that is still changing is how you end up with two cores.

## CI contract

`.github/workflows/ci.yml`

- `host-tests` — matrix `ubuntu-latest` / `windows-latest`: configure with the same
  `CMakePresets.json` a developer uses, build, `ctest`, then **smoke the real binary**: create
  a project, open it, duplicate it, rename it, delete it, corrupt a manifest and prove the hub
  skips it instead of dying.
- `android` — NDK r27, `android-arm64` preset, builds `lume_core` + `lume_hub_logic`, then
  reads the ELF header of an object file and asserts `e_machine == 183` (AArch64). A build that
  silently produced x86 objects would otherwise pass.
- `guardrails` — asserts the step-2 options fail loudly with actionable messages, and validates
  every `templates/*/template.json` against the manifest keys the C++ reader requires.
- `style` — clang-format report, `continue-on-error` until the first formatting pass, then it
  becomes required. It is labelled informational in the job name so nobody misreads it.
- `summary` — the single check to make required in branch protection.

Warnings-as-errors exists (`LUME_WARNINGS_AS_ERRORS`) but is not on yet; switch it on in the
same commit as the first formatting pass, when the tree is stable enough to keep it clean.

## How to add a step

1. New directory or new option, never both at once.
2. `lume-core` stays dependency-free: if a step needs a third party, it goes above the core,
   behind an option, in the step that owns it.
3. CI first: write the gate, watch it fail on an empty branch, then implement until it passes.
   Every step's tests must run on all three platforms or explicitly say why one is exempt.
4. When a step ships, the roadmap table gets a check mark in the same PR. No separate changelog.
