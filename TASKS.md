# Altisidora — Tasks (implementation plan)

## Summary

| Option | What | Status |
|--------|------|--------|
| A | Quick wins (scan transaction, DB indexes, search debounce, scanner ext set) | [x] done (`perf/quick-wins`) |
| B | Controller caching + N+1 removal | [x] done (`perf/controller-cache`) |
| C | `SongListModel` + role-based delegates (C1–C4) | [x] done |
| Backlog | Micro-optimizations (player emissions, MPRIS coalesce, static core lib, shared DB note) | [x] done (`perf/backlog-microopts`) |
| D | Tag editing (individual + batch) | [x] done (`build/static-core-lib`) |
| E | Final polish: theme rename (`Anarchism`) + new `Maya`; startup-theme bug; album edit adds artist + artwork | [~] in progress (`build/static-core-lib`) |

Options A–D are implemented and verified (build exit 0, 0 warnings, `ctest`
12/12, smoke OK). The commit chain lives in git (base `e2b7692` →
`build/static-core-lib`); the detailed step-by-step specs were removed once
each option landed. **Open work is option E only**, tracked task-by-task in
`odd/tasks/theme-and-album-edit.md`. What remains after E is delivery and the
optional follow-ups tracked in `docs/PROJECT_STATUS.md` (publishing `v0.1.0`;
`mpris:artUrl`; artwork-cache GC; a player refresh API so the now-playing bar
follows an edit; `hicolor-icon-theme` in the PKGBUILD depends).

---

## Global verification (after every option)

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"          # expect exit 0
ctest --test-dir build --output-on-failure  # expect 12/12
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

Work on a feature branch per option; never edit on `review/project-cleanup`
directly.

---

## Decisions log

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
- Do not mix options in one commit; each commit above is a reviewable work
  unit with its own verification.
- `git switch -c` before starting an option; never edit on
  `review/project-cleanup` directly.
- Update `docs/PROJECT_STATUS.md` only if a number or behaviour claim changes.
- After each option: tree clean, build exit 0, `ctest` 12/12, smoke run OK.
