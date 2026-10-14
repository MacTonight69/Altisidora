# Close the verified gaps (group B)

## Objective

Close the three gaps verified against the code and recorded in
`docs/PROJECT_STATUS.md` → "Next steps" (items 4, 5 and 6), so the pending list
shrinks to work that genuinely needs a decision.

## Problem

Three small independent defects, each already verified in the source:

- `hicolor-icon-theme` is missing from the `PKGBUILD` `depends`, although
  `package()` installs into `share/icons/hicolor/512x512/apps/`.
- `MprisPlayer::metadata()` never publishes `mpris:artUrl`, so desktop shells
  and media controls show no artwork. `Song::toVariantMap()` already exposes the
  path under the QML key `artwork`.
- The artwork cache has no garbage collection: content-addressed cover files
  accumulate for the lifetime of the installation and nothing removes them, not
  even when the song is deleted from the database.

## Why now

They need no product decision, they are cheap, and they are the only remaining
pending items that are neither blocked nor undecided.

## Scope

In scope: the three gaps above, each with its test and its documentation update.

Out of scope: packaging (AppImage/Windows), the RDD process item, publishing,
and every other "Known limitations" entry not listed here (mojibake coverage,
non-atomic artwork save, playback queue scoping).

## Constraints

- The repo's removal precedent is deliberately conservative
  (`docs/PROJECT_STATUS.md`, scan-removal rules). The cache GC must follow it:
  never remove a file that any row still references.
- No public API the test suite exercises may be removed (`TASKS.md` guardrail).
- `docs/PROJECT_STATUS.md` → "Next steps" is updated as each item closes; a
  stale entry is a defect of the same kind this work is fixing.

## Tasks

| ID | Task | Route | Surfaces | Status |
|----|------|-------|----------|--------|
| B4 | Add `hicolor-icon-theme` to the PKGBUILD `depends` | delegated (small model) | `PKGBUILD` | CLOSED |
| B5 | Publish `mpris:artUrl` from `MprisPlayer::metadata()` | delegated | `src/mpris/MprisPlayer.cpp`, `tests/unit/tst_mpris.cpp` | CLOSED |
| B6 | Garbage-collect the artwork cache | delegated | `src/library/`, `src/database/`, `tests/unit/` | CLOSED |

### Route evidence

- **B4** — one mechanical, already-understood line with no research attached.
  Delegated because the user asked for a subagent per task, and run on a
  small/fast model precisely because it is trivial and independently verifiable
  with `makepkg --printsrcinfo`.
- **B5** — delegated: source plus test, and the MPRIS detail (`artUrl` is a URL,
  not a filesystem path) is exactly the kind of thing that has to be right.
- **B6** — delegated: the highest-consequence of the three (it deletes files) and
  the only one that spans the library, database and test layers.

## Acceptance criteria

- **B4** — `makepkg --printsrcinfo` succeeds and its `depends` contains
  `hicolor-icon-theme`.
- **B5** — `tst_mpris` asserts `mpris:artUrl` equals the `file://` URL of the
  song's artwork, and that the key is absent when the song has no artwork.
- **B6** — a test proves an orphaned artwork file is removed while a referenced
  one survives, and the existing artwork tests stay green.
- **All** — build exit 0 / 0 warnings, `ctest` green, offscreen smoke exit 124.

## Checks

```bash
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

plus, per task, the focused command named in that task's progress entry.

## Delivery forecast

Estimated authored changed lines (additions + deletions): ~150–250, under the
~400 budget. Strategy `ask-on-risk` (default): no chain is needed, so there is
nothing to ask. One slice; each task is its own work-unit commit.

## Progress log

<!-- one entry per task: commit identity, observed results, deviations -->

### B4 — declare `hicolor-icon-theme` in the PKGBUILD — CLOSED

- **Commit:** `fix(packaging): declare hicolor-icon-theme dependency`
- **Change:** `PKGBUILD` `depends` gained `'hicolor-icon-theme'` as its last
  entry. One line; nothing else touched.
- **Delegation:** `general` subagent on `opencode/mimo-v2.6-flash-free`
  (small-model profile — a single mechanical line, fully verifiable without
  judgment).
- **Verification** (writer-reported, then each command re-run by the parent):
  - `bash -n PKGBUILD` → exit 0
  - `makepkg --printsrcinfo` → exit 0, `depends = hicolor-icon-theme` present
  - `git diff --stat` → 1 file changed, +1 line
- **Docs:** the hicolor entry was removed from `docs/PROJECT_STATUS.md`
  → "Next steps" in the same commit.
- **Deviations:** none.
- **Note:** the writer reported `mem_judge` takes a `relation` key, not
  `verdict`, despite the protocol wording. Worth fixing in the protocol text.

### B5 — publish `mpris:artUrl` — CLOSED

- **Commit:** `feat(mpris): publish the track artwork as mpris:artUrl`
- **Change:** `MprisPlayer::metadata()` reads the song's `"artwork"` key (the
  QML-facing key from `Song::toVariantMap()`) and, when non-empty, publishes
  `mpris:artUrl` as `QUrl::fromLocalFile(path).toString()`. MPRIS requires a
  URL, not a path. The key is omitted entirely when there is no artwork, matching
  the existing empty-value style. `Song::toVariantMap()` is untouched.
- **Test:** `loadFakeSong()` gained an artwork entry;
  `testPlayerInterfaceAndMetadata()` asserts the `file://` URL; a new
  `loadFakeSongNoArtwork()` + `testMetadataNoArtwork()` assert the key is
  absent rather than empty.
- **Delegation:** first launch used the `gentle-ai-worker` agent, which
  succeeded and applied the change. A second launch was made in error (the
  parent had mis-read which of two parallel launches had failed) and
  independently re-verified the same diff.
- **Verification** (writer-reported, then re-run by the parent):
  - `cmake --build build -j"$(nproc)"` → exit 0, zero warnings
  - `ctest --test-dir build --output-on-failure` → 13/13
  - offscreen smoke → exit 124, no journal entries
  - `git diff --stat` → exactly the two intended files, +37 lines
- **Docs:** the `mpris:artUrl` entry was removed from `docs/PROJECT_STATUS.md`
  → "Next steps" in the same commit.
- **Deviations:** none in the artifact. The second writer produced RED evidence
  by stashing only the source file, because the change was already applied.

> **Hazard found (not caused by this work):** the repository already had a
> `stash@{0}` from `main` ("rename project to Benzaiten"). A writer that runs
> `git stash`/`git stash pop` around its own edits can pop the wrong stash. Any
> future delegation that stashes must scope it with `git stash push -- <path>`
> and verify what it popped.

### B6 — garbage-collect the artwork cache — CLOSED

- **Commit:** `feat(library): garbage-collect the artwork cache`
- **Change:** a prune entry point plus its call site and its reference query.
  - `MetadataReader::artworkCacheDir()` — a new file-static that is now the
    **single** definition of the cache location. The write path
    (`cacheArtworkBytes`) and the prune both resolve the directory through it,
    so the two cannot drift apart.
  - `MetadataReader::pruneArtworkCache(const std::optional<QSet<QString>> &)`
    — enumerates `QDir::Files | QDir::NoSymLinks` (regular files directly
    inside; no recursion, symlinks neither followed nor removed), removes each
    file whose absolute path is not referenced, logs and skips individual
    removal failures instead of aborting the pass, never removes the directory
    itself, and returns the count removed.
  - `DatabaseManager::referencedArtworkPaths()` — `SELECT DISTINCT artwork_path
    FROM songs WHERE artwork_path IS NOT NULL AND artwork_path != ''`.
  - Hooked in `LibraryController`'s scan-finished handler, after the removals
    and upserts and after `refreshAlbumArtwork()`, i.e. exactly where the
    database is authoritative.
- **Design decision (writer's, and an improvement on the plan):** the plan asked
  for a plain `QSet` and a documented "skip on failure" instruction. The writer
  instead made the parameter `std::optional<QSet<QString>>`, so "the reference
  set is unknown" is a state the type system carries and the function *cannot*
  silently treat as "nothing is referenced". The catastrophic failure mode (a
  failed query wiping the whole cover cache) is therefore structural rather than
  a convention a future caller could forget.
- **Tests (all observed PASS):**
  - `tst_metadata`: `testPruneArtworkCacheRemovesOnlyUnreferenced` — writes real
    entries through the public write path, asserts the referenced file survives,
    the unreferenced one is removed and counted (`QCOMPARE(removed, 1)`), that a
    nested directory is not recursed into, and that a symlink pointing outside
    the cache neither removes its target nor is itself removed.
  - `tst_metadata`: `testPruneArtworkCacheSkipsWhenReferencesUnavailable` —
    `nullopt` removes nothing.
  - `tst_library`: `testScanPrunesUnreferencedArtworkCache` — end-to-end: a real
    completed scan removes an orphaned cache file and keeps the file a song
    still points at.
- **Verification** (writer-reported, then re-run by the parent):
  - `cmake --build build -j"$(nproc)"` → exit 0, zero warnings
  - `ctest --test-dir build --output-on-failure` → 13/13
  - `tst_metadata` → 33 passed; `tst_library` → 29 passed
  - offscreen smoke → exit 124
  - `git diff --stat` → 7 files, +275/−2, all within the allowed surfaces
- **Docs:** the `Known gaps` section of `docs/PROJECT_STATUS.md` → "Next steps"
  is removed in this commit, because it is now empty. Nothing else changed in
  that list.
- **Deviations:** the `std::optional` upgrade described above, which was
  accepted deliberately.
