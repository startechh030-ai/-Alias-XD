# assets/

Design sources and art, kept in the repo next to the code so a mockup and the thing it describes
change in one commit.

```
brand/lume-logo.svg      wordmark + mark, one file, currentColor-friendly
hub/hub-mockup.svg       the hub as step 2 must render it: geometry, spacing, colours
ui/icons/*.svg            24x24 stroke icons referenced by ProjectTemplate::icon
```

Rules:

- **SVG only, until a raster is genuinely needed.** These files stay reviewable in a diff.
- **Colours come from `hub/src/theme.hpp`.** Nothing is eyeballed; the mockup's fills are the
  hex values in the table in `docs/01-hub-project-system.md`.
- `icon` in a `template.json` is a filename under `ui/icons/`, minus `.svg`. A missing icon
  falls back to `cube.svg`, so an unknown name is ugly rather than broken.
- Project thumbnails are *not* assets. They are renderer output, cached in
  `<hub root>/thumbs/<project id>_320.png`, and this directory never holds one.
