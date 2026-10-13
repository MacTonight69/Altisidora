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
  `fontFamily: "Baskervville"` and **Mimy's measurements** (the default 1.0
  scale / 210px sidebar), not Anarchism's 1.9 / 260.
- Theme migrations may not create a rename loop: `Maya` remains a valid theme
  name, so only the dead `Anarchy` key is migrated (to `Anarchism`).
- Album editing must never equalize per-song data (titles/track numbers).

## Tasks
- [x] T2 Rename theme `Maya` -> `Anarchism` in `Themes.js` (key, `name`, `order`).
- [x] T3 Add the new `Maya` entry (same palette, `Baskervville`, Mimy's
      measurements: default scale 1.0 / 210 sidebar) after `Anarchism`.
- [x] T4 Apply the saved theme at startup (`main.qml` `Component.onCompleted`).
- [x] T5 Update `tst_theme_fonts` (add `Anarchism` to the scale maps; add a
      startup-apply regression test) and `SettingsController` (`Anarchy` ->
      `Anarchism`).
- [x] T1 Album edit: artist + artwork (artist move is album-scoped, "Solo este
      álbum").

## Progress
- T2/T3: `Anarchism` holds the old `Maya` palette byte-for-byte (key and
  `name` changed only); the new `Maya` is that same palette with
  `fontFamily: "Baskervville"`. `order` is now
  `Mimy, Coco, Anarchism, Maya, Comunism, ...`.
  - **Owner decision (2026-10-13): Maya's measurements equal Mimy's.** The
    option-E spec had given the new Maya `fontScale 1.9` / `sidebarWidth 260`
    (copied from Anarchism); the owner ruled it should be "equal or similar to
    Mimy's", i.e. the defaults 1.0 / 210. The implementation already matched
    (Maya omits both keys), so this only realigns the spec and locks it with
    `tst_theme_fonts::testMayaKeepsMimyMeasurements`.
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
- T1 (commit `0a9615e`): the drill-down dialog is now "Edit album": title,
  artist and cover in one Save. The artist field moves ONLY this album (its
  songs follow through `songs.artist_id` while the card reads
  `albums.artist_id`; both sides move in one transaction, flagged
  `edited = 1`). The cover applies to every song of the album (bulk path,
  empty path removes it); the global artist rename stays in the Artists
  section.
- T1 correction found by the independent verifier: a combined title+artist
  edit was falsely refused when either intermediate step would collide
  (title-first against the old artist, artist-first against the target
  artist). `renameAlbum` + `setAlbumArtist` became ONE atomic
  `setAlbumIdentity(title, artist, newTitle, newArtist)` with a single
  collision check on the FINAL identity (excluding the edited row), so the
  intermediate state can never refuse a legal edit. Regression tests pin
  both holes in one scenario.

## Verification
- Build: exit 0.
- `ctest`: 13/13 (no-warning build; includes `tst_database`, `tst_library`
  additions and the new `tst_detail_view` QML-loading test).
- RED proof: the bugged collision (checked against the old artist) made
  `testSetAlbumIdentityHandlesACombinedTitleAndArtistChange` fail; restored,
  green. RED proofs on "both sides move" and "artwork covers every song"
  were repeatable in the T1 development pass.
- `qmllint`: exit 0 (only the pre-existing context-property warnings).
- Smoke (offscreen, isolated config `theme=Anarchism`): stayed alive, empty
  stderr.

## RDD outcome (T1)
- `gentle-ai review assess` range `79b9544..0a9615e`: `risk=medium`
  (`executable_change` in `SongListDetailView.qml`), 10 paths / 1159 lines →
  `review_due=true`, `slice_budget_reached`.
- START consented by the user (granted); one lens selected
  (`review-reliability`).
- Lens capture UNAVAILABLE: the host runtime cannot dispatch the reviewer
  subagent (tier restriction: "OpenCode's free tier can only be used from
  within OpenCode"), so the provider-bound task never produced output; the
  Go transport refused `provider_failed`. Declared the slot unachievable
  (`capture-unachievable`, `host_subagent_dispatch_unavailable`); STATUS
  stopped with `unachievable_lens_slot`. Not transient (a probe with a
  different registered agent failed the same way).
- Outcome recorded: `unavailable` (host dispatch). No receipt, no PASS; the
  stopped transaction stays in the authority inventory. Options for the
  user: disable RDD for this clone (`gentle-ai review mode disable --scope
  clone`), or leave it on and re-review this range from a runtime that can
  dispatch agents (real OpenCode client), via a fresh `review start` with
  `--base-ref 79b9544`.
- User decision (2026-10-12): leave RDD enabled; no mode toggle. The range
  `79b9544..0a9615e` stays pending for a fresh `review start` from a
  capable runtime; delivery follows ordinary repository policy.

## Route
Inline (already-understood, mechanical edits across a few small files);
independent read-only verification delegated per commit (found the
combined-edit bug, fixed).
