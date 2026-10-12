# Altisidora — Tasks (implementation plan)

## Summary

| Option | What | Status |
|--------|------|--------|
| A | Quick wins (scan transaction, DB indexes, search debounce, scanner ext set) | [x] done (`perf/quick-wins`) |
| B | Controller caching + N+1 removal | [x] done (`perf/controller-cache`) |
| C | `SongListModel` + role-based delegates (C1–C4) | [x] done |
| Backlog | Micro-optimizations (player emissions, MPRIS coalesce, static core lib, shared DB note) | [x] done (`perf/backlog-microopts`) |
| **D** | **Tag editing (individual + batch)** | [ ] **new — spec below** |

A–C and the backlog are implemented and verified (build exit 0, 0 warnings,
`ctest` 12/12, smoke OK). The commit chain lives in git (base `e2b7692` →
`build/static-core-lib`); the detailed step-by-step specs were removed once
each option landed. **Option D is the only open work.**

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

## Option D — Tag editing (individual + batch) `[new]`

Let the user correct song metadata from the UI: title, artist, album, year
and artwork — on a single song, or applied to many at once. Batch edits are
**field-level reassignments only** (e.g. "set these 12 songs to album X" or
"rename this artist"); they must never equalize every field of the selected
songs (titles, track numbers and per-song data stay untouched).

### D1 — Database: update paths `[step 1]`

**Why.** Today the only writer is the scan (`upsertSong`), which always
takes the file's tags. User edits need their own update path that preserves
user data (`play_count`, `favorite`, `last_played`, `date_added`) and,
crucially, **survives future rescans** — otherwise the next scan silently
overwrites the user's corrections from the file tags.

**Files:** `src/database/DatabaseManager.h`, `src/database/DatabaseManager.cpp`.

**Do.**

1. **Schema v4 — `edited` flag.** Add `edited INTEGER NOT NULL DEFAULT 0` to
   `songs` (version-guarded migration, same pattern as v2/v3: read the stored
   version before rewriting `schema_version`). When `edited = 1`,
   `upsertSong()` skips the tag-derived columns (title/artist/album/genre/
   year/duration/artwork) and only refreshes `mtime`/`size` — the scan keeps
   its fingerprint behavior but never clobbers user data. This is the same
   "preserve user data" philosophy the scan already applies to play stats.

2. **`updateSong(const Song &)`** — update the tag-derived columns for one
   song by id, resolving artist/album/genre names through the existing
   `resolveNamedRow()` / `resolveAlbum()` (creating rows when the new name
   does not exist). Sets `edited = 1`. User-data columns are never touched.

3. **`updateSongs(const QList<Song> &)`** — batch version: one transaction,
   one commit, per-song `updateSong` logic. Used by both the batch UI and
   album/artist renames.

4. **`renameArtist(const QString &from, const QString &to)`** —
   `UPDATE artists SET name = :to WHERE name = :from` inside a transaction;
   songs follow through the FK. Refuse when `to` already exists (would merge
   two artists — a separate, explicit feature).

5. **`renameAlbum(const QString &title, const QString &artist, const QString &newTitle)`** —
   `UPDATE albums SET title = :new WHERE title = :title AND artist_id = (…)`.
   Refuse when an album with `(newTitle, artist)` already exists.

6. **`setArtwork(qint64 songId, const QString &imagePath)`** — extract the
   image with the same `extractArtwork()` pipeline used at scan time
   (content-hash name, `QSaveFile`, `AppDataLocation/artwork/`) and point the
   song's `artwork_path` at the cache file. Reuse, don't duplicate.

**Verify.** `ctest` (tst_database: updateSong preserves user data; rename
cascades; batch is atomic; `edited = 1` blocks rescan overwrite — pin it with
a scan-after-edit test). Build + smoke.

**Commit.** `feat(database): add tag update paths with an edited flag`

### D2 — LibraryController API `[step 2]`

**Files:** `src/library/LibraryController.h`, `src/library/LibraryController.cpp`.

**Do.**

1. `Q_INVOKABLE bool updateSong(const QVariantMap &songMap)` — validate the
   map has an `id`; fill a `Song`, call `m_db.updateSong()`, then refresh the
   caches (`m_songsStale` etc.), `m_songsModel.applySnapshot(m_songs)` and
   emit `libraryChanged()` once. Return false (with a `qWarning`) on bad
   input, never throw into QML.

2. `Q_INVOKABLE bool updateSongs(const QVariantList &songMaps)` — same for a
   list; one transaction inside `m_db.updateSongs()`.

3. `Q_INVOKABLE bool renameArtist(from, to)` / `renameAlbum(title, artist, newTitle)` —
   thin wrappers over the DB methods with the same cache/model/notify
   refresh.

4. `Q_INVOKABLE bool setArtwork(int songId, const QString &imagePath)` —
   wrapper over `m_db.setArtwork()`.

5. All mutators run on the main thread (DB connection is main-thread only) and
   emit `libraryChanged()` exactly once per call.

**Verify.** `ctest` (tst_library: each mutator emits `libraryChanged` once
and the caches reflect the change). Build + smoke.

**Commit.** `feat(library): expose tag editing to QML`

### D3 — QML UI `[step 3]`

**Files:** `resources/qml/views/SongsView.qml`,
`resources/qml/views/SongListDetailView.qml`,
`resources/qml/views/AlbumsView.qml`, `resources/qml/views/ArtistsView.qml`,
`resources/qml/main.qml`.

**Do.**

1. **Individual edit.** In the song row context menu (the same menu that
   already has add-to-playlist), an "Edit" action opening a dialog with
   title / artist / album / year fields and an artwork picker (file dialog →
   `Library.setArtwork`). Save calls `Library.updateSong(...)`. The dialog
   is a `Dialog` with `required property var model` (the binding-loop lesson
   from the QML fix applies — no `implicitWidth` self-reference).

2. **Batch edit from the selection bar.** The multi-select bar already exists
   (click / Ctrl / Shift / Ctrl+A). Add "Set album…" / "Set artist…" actions
   that open the same dialog pre-filled; saving calls
   `Library.updateSongs(selection.map(s => ({...s, album: newAlbum})))` —
   **only the reassigned field changes**; every other field is sent back
   unchanged from the selection. This is what enforces "no equalizing all
   data": the UI never offers a blanket overwrite, it only reassigns the
   chosen field.

3. **Album / artist rename.** In the album and artist views, an edit action
   on the header calls `Library.renameAlbum(...)` / `Library.renameArtist(...)`
   (confirm dialog first — it rewrites every song in the collection).

4. **Artwork picker.** Reuse the existing file-dialog pattern; the chosen
   image goes through `Library.setArtwork` (D2.4) so it lands in the content-
   hash cache like scan-extracted art.

**Verify.** Build + smoke + manual: edit one song (fields + artwork), batch
reassign album on a multi-selection, rename an artist, then rescan and confirm
the edits survive (the `edited` flag). No QML test harness exists for this —
manual verification is expected; say so in the commit message.

**Commit.** `feat(ui): edit song tags individually and in batch`

### D4 — Tests `[step 4]`

**Files:** `tests/unit/tst_database.cpp`, `tests/unit/tst_library.cpp`,
`tests/integration/tst_library_scan.cpp`.

**Do.**

- `tst_database`: `updateSong` changes tag columns and preserves user data;
  `updateSongs` is atomic (one bad row rolls back all); `renameArtist` /
  `renameAlbum` cascade and refuse collisions; `setArtwork` points at a real
  cache file; a song with `edited = 1` survives `upsertSong` with different
  file tags (the rescan policy — the most important regression pin).
- `tst_library`: each controller mutator emits `libraryChanged` exactly once
  and the QML-facing caches reflect the change.
- `tst_library_scan`: scan → edit → rescan keeps the edit.

**Verify.** Full `ctest` 12/12 (plus the new cases), build 0 warnings, smoke
OK.

**Commit.** `test(library): cover tag editing and the rescan policy`

---

## Decisions log

- **A1b (per-scan name→id cache): measured 2026-10-11 — do not implement.**
  Benchmark of the real pipeline (5000 songs, /tmp/opencode/upsert-bench):
  the cache cuts the commit phase 45% (30 403 → 6 243 statements) but saves
  only ~250 ms per 5000 songs — invisible next to TagLib I/O — and a stale
  cache silently corrupts scans (FK violations, 0 songs committed;
  reproduced). The `INSERT OR IGNORE … RETURNING` variant is a measured
  **pessimization** (+18–22% commit time). Do not retry without new evidence.

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
