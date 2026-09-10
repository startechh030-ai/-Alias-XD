# Vendored third-party code

Lume vendors nothing in step 1. This directory is the drop point for the sources each later
step needs, and it is git-ignored so the workspace keeps holding only code we wrote plus
assets.

| Step | Dependency | Where it comes from | How it lands here |
| --- | --- | --- | --- |
| 2 | `filament` | Google Filament **release** archives (prebuilt `releases/cmake/{linux,windows}`) | CI downloads, unpacks, sets `LUME_FILAMENT_DIST_DIR` |
| 2 | `imgui` | dear imgui 1.91 + its `backends/vulkan`-style Filament glue we write ourselves | vendored, ~30 files |
| 3 | `libigl` | libigl 2.5 (header-only core) | vendored, header-only |
| 3 | `eigen` | Eigen 3.4 (libigl requirement) | vcpkg or vendored |
| 4 | `stb` | stb_image / stb_image_write | vendored, 2 headers |
| 5 | `cgltf` | single-header glTF reader/writer | vendored |
| 6 | `joltphysics` | Jolt (3D rigid bodies; Box2D stays as the 2D reference) | fetched in CI only |
| 7 | `assimp` | Assimp via vcpkg on win/linux; NDK prebuilt on Android | vcpkg manifest |

Rules that keep this directory honest:

1. Nothing in `external/` is compiled unless the option that owns it is ON.
2. A dependency may be vendored only by the step that uses it — no speculative imports.
3. Every vendored drop records its version and license in `THIRD_PARTY.md` in the same commit.
4. CI, not a developer machine, performs the download so a build is reproducible from a clean
   checkout.
