# Step 2 (planned) — Filament across Android, Windows, Linux

Not implemented. This file exists so that `-DLUME_ENABLE_FILAMENT=ON` can point at an
intention rather than a void, and so step 1's seams are reviewed now, while they are cheap to
change. `core/CMakeLists.txt` and `hub/CMakeLists.txt` already reserve the switches.

## What has to be true when step 2 lands

1. **The core never sees a graphics header.** `lume-core` keeps compiling with no include path
   beyond the STL; the renderer lives in `render/` and links Filament. This is what keeps
   `host-tests` a 40-second job and lets the NDK job build in under two minutes.
2. **One swapchain abstraction, three backends.** Linux → Vulkan (GL 4.x fallback for the
   driver-lucky), Windows → Vulkan with a D3D11-ish GL fallback, Android → Vulkan where
   available and GLES 3.1 where not. Filament's own backend selection does this; our job is
   the window/surface plumbing and *not* re-implementing it.
3. **`LUME_FILAMENT_DIST_DIR` points at an unpacked release**, never a source checkout.
   Compiling Filament from source blows past CI runner limits and is not a thing to debug in a
   3D-tool project. CI downloads the release archive, unpacks it, and hands CMake the directory.
4. **The hub UI is a texture, not a backend.** ImGui draws into a command list we rasterise
   into a Filament texture, so the hub, the viewport and the inspector share one renderer.
   `hub_ui.hpp` therefore only ever speaks to the renderer through
   `ImageDrawer = void(path, x, y, w, h)` — the card thumbnails in `thumbs/` go through the
   same call.
5. **Thumbnails are a renderer output.** `thumbs/<id>_320.png` is produced by rendering the
   scene's saved camera for ~200 ms when a project is opened or autosaved. The path, the name
   and `ProjectRef::has_thumbnail` already exist in step 1 so this is an addition, not a
   refactor.
6. **`scene.render.backend` is honoured.** Anything in a project's manifest that the renderer
   ignores must be deleted from the manifest rather than left decorative.

## CI shape to expect

- `renderer-linux` / `renderer-windows`: fetch Filament release, configure with
  `LUME_ENABLE_FILAMENT=ON -DLUME_FILAMENT_DIST_DIR=…`, build the viewport, run a headless
  render on a software driver (lavapipe / warp), assert the output image is not blank by
  checking its mean luminance against a golden threshold.
- `renderer-android`: build the APK with the JNI surface plugin, install on an emulator image,
  screenshot, assert non-black. Artifacts are for humans; only the assertion gates the step.
- The `guardrails` job that today asserts "Filament must fail to configure" flips to asserting
  "Filament must configure from a dist dir" in the same commit that removes the error.

## Known risks to retire first

- Filament's release naming/asset layout drifts between versions; pin one and bump it on
  purpose, in its own commit.
- ImGui + a custom backend is the part people underestimate; budget it as its own sub-step,
  with the text hub (`lume-hub --print`) kept alive as the fallback path for CI.
- GLES 3.1 on cheap Android devices means no async readback for thumbnails; fall back to a
  downscaled blit instead of blocking the frame.
