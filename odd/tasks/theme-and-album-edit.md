# Theme renames and album editing

## Objective
Land the last round of UI polish before release:
1. Editing an album also edits the artist name and the album artwork.
2. Rename the `Maya` theme to `Anarchism` and add a new `Maya` theme identical
   to it but using the Baskervville typeface.
3. Fix the theme-persistence bug: the app reopened on the default `Mimy` while
   Settings showed the theme it was closed with.

## Problem
- `Theme.qml` starts on `Mimy` and `main.cpp` never applies the stored theme;
  `main.qml` only re-applies on `AppSettings.settingsChanged`, which is not
  emitted at startup. So the first frame is always `Mimy` even though
  `AppSettings.theme` holds the saved value (which the Settings combo shows).
- The album drill-down "Rename" dialog edits only the album title; the artist
  and the artwork are not editable there.

## Scope
- `resources/qml/themes/Themes.js`, `themes/Theme.qml`
- `resources/qml/main.qml`
- `src/app/SettingsController.cpp`
- `resources/qml/views/SongListDetailView.qml`
- `tests/unit/tst_theme_fonts.cpp`
- `docs/PROJECT_STATUS.md` (theme list/behaviour if a claim changes)

## Constraints
- The `Anarchism` palette must stay byte-identical to the old `Maya` (only the
  key/name change); the new `Maya` is the same palette with
  `fontFamily: "Baskervville"`.
- Theme migrations may not create a rename loop: `Maya` remains a valid theme
  name, so only the dead `Anarchy` key is migrated (to `Anarchism`).
- Album editing must never equalize per-song data (titles/track numbers).

## Tasks
- [x] T2 Rename theme `Maya` -> `Anarchism` in `Themes.js` (key, `name`, `order`).
- [x] T3 Add the new `Maya` entry (same palette, `Baskervville`, scale 1.9,
      sidebar 260) after `Anarchism`.
- [x] T4 Apply the saved theme at startup (`main.qml` `Component.onCompleted`).
- [x] T5 Update `tst_theme_fonts` (add `Anarchism` to the scale maps; add a
      startup-apply regression test) and `SettingsController` (`Anarchy` ->
      `Anarchism`).
- [ ] T1 Album edit: artist + artwork (artist semantics pending a decision).

## Progress
- T2/T3: `Anarchism` holds the old `Maya` palette byte-for-byte (key and
  `name` changed only); the new `Maya` is that same palette with
  `fontFamily: "Baskervville"`. `order` is now
  `Mimy, Coco, Anarchism, Maya, Comunism, ...`.
- T4: `main.qml` root gained
  `Component.onCompleted: root.applyTheme(AppSettings.theme)`. The existing
  `Connections` only fires on later `settingsChanged` emissions, which is why
  the first frame stayed on `Theme.qml`'s default `Mimy` while the Settings
  combo showed the saved theme.
- T5: `SettingsController` now migrates the dead `Anarchy` key to `Anarchism`;
  `Maya` is deliberately not migrated because it still names a theme, so a
  remap would make the new theme unselectable. A new
  `tst_theme_fonts::testStartupAppliesSavedTheme` seals a config with
  `theme=Onyx`, rebuilds `main.qml`'s wiring and asserts the applied theme is
  `Onyx` (scale 1.4).

## Verification
- Build: exit 0.
- `ctest`: 12/12 (including the new test).
- RED proof: commenting out the startup apply makes the new test fail with
  `themeName == "Mimy"` vs expected `"Onyx"` — the exact reported bug.
- `qmllint`: exit 0 (only the pre-existing context-property warnings).
- Smoke (offscreen, isolated HOME, seeded `theme=Onyx`): stayed alive, empty
  stderr, no journald warnings; config still `theme=Onyx`.

## Route
Inline (already-understood, mechanical edits across a few small files).
