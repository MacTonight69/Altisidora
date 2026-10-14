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
| B4 | Add `hicolor-icon-theme` to the PKGBUILD `depends` | delegated (small model) | `PKGBUILD` | pending |
| B5 | Publish `mpris:artUrl` from `MprisPlayer::metadata()` | delegated | `src/mpris/MprisPlayer.cpp`, `tests/unit/tst_mpris.cpp` | pending |
| B6 | Garbage-collect the artwork cache | delegated | `src/library/`, `src/database/`, `tests/unit/` | pending |

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
