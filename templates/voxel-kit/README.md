# Voxel Kit project

Tuned for the Android target rather than the desktop editor:

- `gridSnap: 1.0` — every transform lands on a whole cell, so a build stays watertight.
- `render.backend: "opengl"` — the ES 3.1 path Filament is guaranteed to have on every device
  we care about; `"auto"` would prefer Vulkan and land you on a driver you did not test.
- `render.hdr: false`, `msaa: 2` — the two cheapest ways to keep a mid-range phone at 60 fps.
- `autosaveMinutes: 2` — blocky scenes get edited fast and crash on phones more often.

Imported meshes will not snap; that is expected. Snap applies to authored transforms only.
