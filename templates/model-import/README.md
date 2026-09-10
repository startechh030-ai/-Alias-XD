# Model Import project

Put your source file in `imports/` — FBX, OBJ, glTF/GLB, PLY. The importer is step 7 of
`docs/roadmap.md`, so until then:

1. Drop the file into `imports/` of this project folder.
2. Add its path to `project.json` under a `"pendingImport"` list (any unknown key survives
   round-trips, which is exactly why the hub preserves them).

```json
"pendingImport": ["imports/rocket.fbx"]
```

Nothing else to do here — the hub will pick that list up when the importer exists.
