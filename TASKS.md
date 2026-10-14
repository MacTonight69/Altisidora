# Altisidora — Tasks (implementation plan)

## Summary

| Option | What | Status |
|--------|------|--------|
| A | Quick wins (scan transaction, DB indexes, search debounce, scanner ext set) | [x] done (`perf/quick-wins`) |
| B | Controller caching + N+1 removal | [x] done (`perf/controller-cache`) |
| C | `SongListModel` + role-based delegates (C1–C4) | [x] done |
| Backlog | Micro-optimizations (player emissions, MPRIS coalesce, static core lib, shared DB note) | [x] done (`perf/backlog-microopts`) |
| D | Tag editing (individual + batch) | [x] done (`build/static-core-lib`) |
| E | Final polish: theme rename (`Anarchism`) + new `Maya`; startup-theme bug; album edit adds artist + artwork | [x] done (`build/static-core-lib`) |

Options A–E are implemented and verified (build exit 0, 0 warnings, `ctest`
13/13, smoke OK). The commit chain lives in git (base `e2b7692` →
`build/static-core-lib`); the detailed step-by-step specs are removed once each
option lands, so this file is now the plan index and the record of the
decisions taken along the way. A review-driven **perf & lightening pass**
landed on the same branch afterwards — see the first entry in the Decisions
log, which carries its measurements.

Remaining work lives in `docs/PROJECT_STATUS.md` → "Next steps": delivery, the
two export targets, and the gaps verified against the code. The export guides
are `docs/PACKAGING_LINUX.md` (AppImage) and `docs/PACKAGING_WINDOWS.md`
(Windows), with the shared rules in `docs/PACKAGING.md`.

Plus one open process item:

- The RDD review of range `79b9544..0a9615e` never ran: the host runtime
  cannot dispatch the reviewer subagent (tier restriction). The user decided
  (2026-10-12) to leave RDD enabled and re-review that range from a runtime
  that can dispatch agents, via a fresh `review start --base-ref 79b9544`.

---

## Verification (expected after any change)

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"          # expect exit 0
ctest --test-dir build --output-on-failure  # expect 13/13
```

Smoke run (isolated HOME; must stay alive until timeout with empty stderr):

```bash
rm -rf /tmp/opencode/alt-run && mkdir -p /tmp/opencode/alt-run/home
cd /home/bautistaac/Proyectos/Altisidora
env HOME=/tmp/opencode/alt-run/home XDG_DATA_HOME=/tmp/opencode/alt-run/data \
    XDG_CONFIG_HOME=/tmp/opencode/alt-run/config QT_QPA_PLATFORM=offscreen \
    timeout 6 ./build/altisidora; echo "exit=$?  (124 = OK)"
```

QML errors go to journald even when stderr is redirected — check
`journalctl _COMM=altisidora --since "-5 min"` after the smoke run.

Work on a feature branch; never edit on `review/project-cleanup` directly.

---

## Decisions log

- **Perf & lightening pass (2026-10-14, `522b3b9`..`33b4acb` on
  `build/static-core-lib`).** A review of the startup path, the library
  model and the build produced seven findings. Each one was either landed as
  its own commit with its own measurement, or measured and declined — the
  measurements for the declined ones are below. What landed: multi-selection
  delete/hide in one batch each; artwork roll-up and row patching scoped to
  the album an edit touched; the four 2000x2000 brand logos re-encoded at
  512x512 (binary 2 539 200 -> 1 330 880 bytes, −48%); `parseStoredDateTime()`
  no longer re-resolving the system timezone per row (`getAllSongs()`
  829 -> 42 ms, `LibraryController::refresh()` 830 -> 45 ms); the redundant
  `idx_ignored_paths_path` dropped; `selectRange()` no longer building a
  `QVariantMap` per row (5000 rows 38 -> 3 ms); the unused
  `songs: Library.songs` launch binding removed (~14 ms); and the unused
  `LibraryController::search()` wrapper deleted. That last one is the single
  deliberate exception to the public-API guardrail below: the method had no
  production caller (QML searches through `setSearch()`/`searchModel`), only
  the tests used it, and those tests now run through the production path. If
  the guardrail should win instead, reverting that one commit is enough.

- **Perf pass: delegate "current row" binding (2026-10-14) — do not
  implement.** The row delegate tests `Player.currentSong.id === model.id`,
  which materialises the current song's `QVariantMap` as a JS object on every
  evaluation: measured 6.6 µs against 1.3 µs for a scalar id property (~5x).
  But the song lists are `ListView`s, so only the ~20-40 realised delegates
  evaluate it and a song change costs well under 0.3 ms. A `currentSongId`
  property would add a second source of truth to keep in sync with
  `currentSong` for no measurable gain.

- **Perf pass: lazy `Library.songs`/`albums`/`artists` (2026-10-14) — do not
  implement.** On the 5000-song library (500 albums / 500 artists) the eager
  aggregates measured 3.2 ms (`albums()`), 3.3 ms (`artists()`) and 14 ms
  (`songs()`), and each feeds a view that is on screen, so the work is not
  wasted. The one piece that *was* wasted — `main.qml` binding
  `songs: Library.songs` into a `SongsView` that is always fed by
  `songsModel`, so the list never read it — built the whole list on every
  launch and was removed instead (see the pass entry above).

- **Perf pass: defer font registration / AOT-compile the QML (2026-10-14) —
  do not implement.** Startup breakdown on the 5000-song library (offscreen,
  warm): fonts 1.8 ms, controllers 3.5 ms, DB refresh 50 ms, QML compile
  37 ms, QML instantiate 140 ms. Registering the seven bundled fonts costs
  ~5 ms once and ~1 ms when repeated (the font database caches them).
  `qmlcachegen` would need migrating `qt_add_resources` to
  `qt_add_qml_module`, which changes every QML URL (module paths, `qmldir`,
  the `Themes.js` import, the `Theme` singleton) to shave the 37 ms compile
  step. Do not retry without new evidence.

- **Perf pass: the scan walk (2026-10-14) — do not implement.** A 5000-file
  tree scans in ~35 ms, of which ~2.5 ms (7%) is the ignored-path match; the
  per-directory `found += scanEntries(...)` copy costs ~1.3 ms and
  precomputing normalised ignored prefixes saves ~3.5 ms more (~15% together).
  The walk already runs off the UI thread (`QtConcurrent::run`), so none of it
  is on the user's path, and it is dominated by the filesystem (readdir+stat
  ≈ 6.4 µs/file).

- **A1b (per-scan name→id cache): measured 2026-10-11 — do not implement.**
  Benchmark of the real pipeline (5000 songs, /tmp/opencode/upsert-bench):
  the cache cuts the commit phase 45% (30 403 → 6 243 statements) but saves
  only ~250 ms per 5000 songs — invisible next to TagLib I/O — and a stale
  cache silently corrupts scans (FK violations, 0 songs committed;
  reproduced). The `INSERT OR IGNORE … RETURNING` variant is a measured
  **pessimization** (+18–22% commit time). Do not retry without new evidence.
- **D rescan policy (2026-10-12): `edited` flag + CASE-guarded upsert, not
  `WHERE edited = 0`.** Freezing the whole `ON CONFLICT` update on
  `edited = 0` would also freeze `mtime`/`size`, so the scanner would re-read
  the edited file on every scan forever. The landed shape refreshes the
  fingerprint unconditionally and CASE-guards only the tag-derived columns.
  Pinned by `tst_database::testUpsertSongKeepsEditedTagColumns` and
  `tst_library_scan::testEditSurvivesRescan` (which rewrites the file with
  different tags to prove the CASE guard, not the fingerprint skip, is what
  saves the edit).

---

## Guardrails

- Keep the public API of every method tests call (`addSong`,
  `supportedExtensions`, `isSupportedAudioFile`, …) untouched; add the new
  methods beside them.
- One reviewable work unit per commit, each with its own verification; do not
  mix unrelated changes.
- Update `docs/PROJECT_STATUS.md` only if a number or behaviour claim changes.
- After any change: tree clean, build exit 0, `ctest` 13/13, smoke run OK.
