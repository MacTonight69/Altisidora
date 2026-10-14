# Platform media seam

## Objective

Move the Linux-only D-Bus/MPRIS integration behind a platform-neutral interface so
that a non-DBus build (Windows) is possible without touching playback, library or
QML code — with **zero behaviour change** on Linux.

## Problem

`src/mpris/` (~416 lines) is the only Linux-only code in the tree, plus the hard
`Qt6::DBus` dependency. On Windows there is no MPRIS and no D-Bus, so the Linux
build works but the same code cannot be compiled there at all. The seam separates
"what the app knows about the currently playing media" (platform-neutral) from
"how a platform advertises that" (D-Bus today, SMTC later, nothing tomorrow).

This is explicitly the first step recorded in
[`docs/PACKAGING_WINDOWS.md`](../../docs/PACKAGING_WINDOWS.md) §1–2, which also
argues the timing: do it while the code is already being touched.

## Why now

- It is the one pending item on `docs/PROJECT_STATUS.md` → "Next steps" that is
  **not** blocked on a user decision (the AppImage entry is; this one is recorded
  as "Linux-only work that can be done now").
- `src/mpris/` was touched in this same session (B5 published `mpris:artUrl`), and
  the guide's own argument is that doing it during a touch costs an hour while
  doing it later costs a re-read of everything.
- It is a pure refactor: no behaviour change, and `tst_mpris` already covers the
  observable surface.

## Scope

**In**

- A platform-neutral base (`MediaIntegration`) owning the shared state that the
  D-Bus adaptors currently read.
- The existing MPRIS implementation moved behind it, unchanged in behaviour.
- A factory the app calls instead of constructing `MprisPlayer` directly.
- A no-op backend and a CMake switch, so a build without D-Bus still links.

**Out (follow-ups, recorded so they are not mistaken for gaps)**

- `WindowsMediaIntegration` (SMTC). This seam only makes room for it.
- The global media-key hotkey path (`RegisterHotKey`).
- `MediaIntegration::metadata()` currently yields MPRIS-flavoured keys
  (`xesam:*`, `mpris:*`). That is the documented shape for now; a future Windows
  backend may need its own mapping. Deliberately not solved here — see
  "Accepted limitations".
- Anything about the `.ico`, the application manifest or `windeployqt` (guide
  §4–5), which need a Windows toolchain.

## Constraints and invariants

1. **No behaviour change on Linux.** The MPRIS service, its properties, signals
   and the `mpris:artUrl` behaviour from B5 must be unchanged.
2. **`tst_mpris` stays green**, and it is the test of record for the MPRIS
   backend. It may need its include and type updated; it must not lose a case or
   weaken an assertion.
3. **The seam is the only new abstraction.** No speculative interfaces, no
   plugin registry, no "backend manager". One base class, one factory, two
   implementations.
4. **No `#ifdef` in logic.** Where conditional compilation is needed, the switch
   is a CMake-provided define and the code reads as ordinary C++ either way.
5. **Playback, library, database and QML are untouched.** If a change there looks
   necessary, stop and report rather than widening the blast radius.
6. **Move, do not rewrite.** The MPRIS implementation body should survive as
   literally as possible; only the class name, the base class and the split of
   registration count as changes.
7. **No commits, no `git stash`.** The parent commits. The repository has a
   pre-existing unrelated `stash@{0}`; a stray `git stash pop` would land the
   wrong work.

## Tasks

| ID | Task | Route | Surfaces | Status |
|----|------|-------|----------|--------|
| S1 | Introduce the seam: base, MPRIS backend moved behind it, factory, `main.cpp` wired, tests updated. D-Bus still required. | delegated writer | `src/media/` (new), `src/mpris/`, `src/main.cpp`, `CMakeLists.txt`, `tests/unit/tst_mpris.cpp`, `tests/CMakeLists.txt` | CLOSED |
| S2 | Make the D-Bus backend optional: CMake switch, no-op backend, conditional factory, `metadata()` becomes pure virtual in the base with its body (and its `QDBusObjectPath`) moving to the MPRIS backend, tests guard, and proof that a D-Bus-off configure still builds. | delegated writer | `src/media/`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `docs/PACKAGING_WINDOWS.md` | pending |
| S3 | Record the outcome in `docs/PROJECT_STATUS.md` and this document. | inline (parent) | `docs/PROJECT_STATUS.md`, `odd/tasks/platform-seam.md` | pending |

## Acceptance criteria

- [ ] `src/mpris/` no longer contains the only platform-specific code path: the
      app constructs media integration through the factory and never names the
      D-Bus type.
- [ ] Linux behaviour is unchanged, proven by the existing test suite, and the
      MPRIS test still exercises a real D-Bus service.
- [ ] A configure with D-Bus disabled succeeds and the resulting binary links,
      which is the actual deliverable for Windows.
- [ ] No QML, playback, library or database file is modified.

## Checks

Per task, run on Linux and report the observed result:

- `cmake --build build -j"$(nproc)"` → exit 0, zero warnings
- `ctest --test-dir build --output-on-failure` → 13/13
- offscreen smoke →
  `env HOME=/tmp/opencode/alt-run/home XDG_DATA_HOME=/tmp/opencode/alt-run/data XDG_CONFIG_HOME=/tmp/opencode/alt-run/config QT_QPA_PLATFORM=offscreen timeout 6 ./build/altisidora` → exit 124
- S2 additionally: a fresh configure with D-Bus off in a throwaway build
  directory under `/tmp/opencode/`, built to completion.

## Route evidence

- **Delegated writer** for S1 and S2: both touch two or more non-trivial files,
  and S1 requires reading the whole MPRIS implementation before moving it, which
  is context the parent does not need to hold. The mapping read therefore belongs
  to the writer doing the write.
- **Inline** for S3: two documentation files, already-understood content.
- Writers run **sequentially**, one work unit each, so there is exactly one writer
  at a time and one commit per unit.

## Delivery forecast

- S1: the MPRIS implementation is ~416 lines moved, so roughly 830 changed lines
  counted as additions plus deletions, plus ~120 lines of new base and factory.
  Most of that is a mechanical move, not new logic.
- S2: ~80 lines (CMake switch, no-op backend, conditional factory, test guard).
- S3: ~30 lines of documentation.

Well past the ~400-per-task planning heuristic, and that is expected and
deliberate: a move is counted twice and the alternative — rewriting the MPRIS
implementation in place instead of moving it — would be worse to review. The
branch is therefore sized like a feature and should land as its own pull request
when the user decides to push.

## Accepted limitations

- `metadata()` yields MPRIS-flavoured keys (`xesam:*`, `mpris:*`) and, in S1's
  shape, still lives in the base TU with a `QDBusObjectPath` inside — so the
  base currently does not compile without D-Bus. This is deliberate for S1
  (zero behaviour change) and is resolved by S2: the base declares
  `virtual QVariantMap metadata() const = 0;` and the body, with its D-Bus
  object-path typing, moves to `MprisMediaIntegration`. Rationale: `metadata()`
  is protocol-shaped by nature — its `mpris:trackid` must marshal as D-Bus type
  `o` — so a base TU that must compile without D-Bus cannot own it. The keys
  stay MPRIS-shaped; a future non-MPRIS backend returns its own map.
- No test asserts that the factory picks the no-op backend when D-Bus is absent,
  because that only exists in the D-Bus-off configuration.

## Progress log

### S1 — introduce the seam — CLOSED

- **Commit:** `refactor(media): introduce the platform media seam`
- **Change:** `src/mpris/MprisPlayer` is now `src/media/MprisMediaIntegration`
  behind `src/media/MediaIntegration`, a platform-neutral base owning the shared
  state (`playbackStatus()`, `metadata()`, `positionMicros()`) plus the
  `start()`/`stop()` lifecycle. `src/main.cpp` constructs through
  `createMediaIntegration()` — the factory is the app's only construction point —
  and defers `start()` exactly as it deferred `registerService()`.
  `CMakeLists.txt` swaps the two source paths; D-Bus stays a hard component and
  link (that is S2). `src/mpris/` is gone. `tst_mpris` keeps every case and
  assertion; only its include, its type and `registerService()` → `start()`
  changed.
- **Fidelity:** the moved body's only real delta is the constructor initializer
  (`MediaIntegration(player, parent)` instead of
  `QObject(parent), m_player(player)`), verified by normalizing both bodies and
  diffing them. `start()` is the old `registerService()` body verbatim; `stop()`
  is new and does the inverse (unregisters object and service).
- **Deviations (accepted):**
  1. The base also exposes `player()`. The guide's shape block omitted it, but
     the moved adaptors need it for volume and transport. Moved unchanged, not
     invented.
  2. `metadata()` — and with it `QDBusObjectPath` — stayed in the base TU. That
     is the documented shape and S1 must not change behaviour, but it means the
     base does not compile without D-Bus. This drives the S2 design recorded
     under "Accepted limitations".
- **Verification** (writer-reported, then re-run by the parent):
  - `cmake --build build -j"$(nproc)"` → exit 0, zero warnings
  - `ctest --test-dir build --output-on-failure` → 13/13
  - `./build/tests/tst_mpris` → 7 passed, 0 failed, 0 skipped
  - offscreen smoke → exit 124
  - no `#ifdef` / `ALTISIDORA_HAS_DBUS` anywhere (grep)
  - `git diff -M --stat` → 7 files, +177/−104; rename detected, so the moved
    `.cpp` shows only 85 changed lines and the `.h` 30

_(one entry per task, written when the task is verified)_
