# Feature: AppImage packaging

## Objective

Produce one self-contained `Altisidora-0.1.0-x86_64.AppImage` that runs on
current Linux distributions without the user installing Qt, mpv or TagLib, and
commit the reproducible pipeline that builds it inside a container.

## Problem

`docs/PACKAGING_LINUX.md` described the intended pipeline but recorded it as
**planned**; nothing existed. The development machine is CachyOS (glibc 2.44), so
building there produces a bundle that starts almost nowhere. The pipeline had to
run inside an old-glibc container.

## Why now

The user asked for the AppImage explicitly and delegated every technical
decision: "convierte el programa en un AppImage con las tecnologías más
convenientes y compatibles con otras distros de Linux. Toma tú todas las
decisiones necesarias."

## Decisions (locked by the parent)

| Decision | Value | Rationale |
|---|---|---|
| Base image | `ubuntu:22.04` — **glibc 2.35** | Lowest floor that still ships a native, modern toolchain and is the reference host for Qt's official Linux binaries. Covers every mainstream distro at or above that floor: Ubuntu 22.04+/24.04+, Debian 12/13, Fedora 36+, Mint 21+, openSUSE Leap 15.6, RHEL/Rocky/Alma 10+, Arch. RHEL/Rocky/Alma **9** ships glibc 2.34 and is therefore **out of scope** here; including it would mean a 2.34 base with its own C++20 compiler and mpv packages, recorded as a future option alongside 2.31. |
| Floor vs. the doc's 2.31 | Deliberately **2.35, not 2.31** | 2.31 would only add Ubuntu 20.04 (EOL May 2025) and Debian 11 (EOL Aug 2026) — both end-of-life — at the cost of a self-built toolchain and a fragile Qt install. Recorded as a future option, not done now. |
| Qt | **6.8.x via `aqtinstall`** (official `linux_gcc_64` binaries) | The app uses QML `FolderDialog`, introduced in Qt 6.3, so distro Qt 6.2.4 (ubuntu:22.04) is too old. Qt 6.8 is the current open-source LTS and is close to the 6.12 dev version. Official binaries carry a glibc 2.28 floor. |
| Compiler | `g++-12` (native on ubuntu:22.04) | Full C++20; the project sets `CMAKE_CXX_STANDARD 20`. |
| TagLib | **2.0.2, built from source** | `MetadataReader` uses `TagLib::FileRef::complexProperties("PICTURE")` returning `List<VariantMap>` — a **TagLib 2.x** API. No base image at glibc ≤ 2.36 ships TagLib 2.x (ubuntu:22.04 has 1.11, debian:12 has 1.13). |
| mpv/FFmpeg | Ubuntu 22.04's `libmpv-dev` (0.34.1), bundled as-is | Audio-only FFmpeg build is the big size lever (~50 MB) but costs a full FFmpeg+mpv build, which the 1.8 GB Docker VM cannot do reliably. Deferred; size is accepted and recorded. |
| C++ runtime (`libstdc++` / `libgcc_s`) | **Not bundled** | The AppDir precedes the host on `LD_LIBRARY_PATH`, so a bundled `libstdc++` shadows the host's and a later-loaded host C++ library fails to resolve. Measured on Ubuntu 24.04 (GCC 13): the host's `libjack.so.0` needs `GLIBCXX_3.4.32`, the build container's libstdc++ (GCC 12) stops at `GLIBCXX_3.4.30`, and the application does not start. Bundling is only needed when a supported host's runtime can be *older* than the build one — coupled to the glibc floor. At 2.35 every supported distribution ships GCC ≥ 12 (`GLIBCXX_3.4.30`) and the bundle only needs `GLIBCXX_3.4.29`. |
| Packaging tools | `linuxdeploy` + `linuxdeploy-plugin-qt` + `appimagetool` (continuous release) | Pre-extracted in the image; FUSE is unavailable in the container. |
| AppStream id | `org.altisidora.Altisidora` | Placeholder reverse-DNS; must be reused for the Windows target. Easy to change if the project is ever published. |
| Artifact | `dist/Altisidora-0.1.0-x86_64.AppImage` (gitignored) | Build output, not committed. |
| CI | Not added | CI was removed from the repo deliberately; re-adding it needs the user's say-so. The build is a documented one-liner instead. |

## Scope

In:

- A container build environment (`packaging/appimage/Dockerfile`) that installs
  the whole stack with the intended glibc floor.
- The build recipe (`packaging/appimage/build-in-container.sh`) run **inside**
  that container: configure, build, install to an `AppDir`, deploy with
  `linuxdeploy`, emit the AppImage into `dist/`. The host entry point is
  `packaging/appimage/make.sh`.
- A minimal AppStream metainfo file.
- `.gitignore` entries for the build output.
- Doc updates recording the outcome and measurements.

Out:

- Building FFmpeg/mpv audio-only (size optimization, later).
- CI wiring.
- Publishing / uploading the artifact.
- Any application source change.
- Any glibc-floor lowering below 2.35.

## Acceptance criteria

1. `bash packaging/appimage/make.sh` on the host exits 0 and writes
   `dist/Altisidora-0.1.0-x86_64.AppImage`.
2. `objdump` glibc scan over the extracted bundle shows a floor of **2.35**, not
   higher.
3. `usr/lib` contains no `libc`, `libGL`, `libEGL` or `libdrm`, and does **not**
   contain `libstdc++.so.6` or `libgcc_s.so.1`.
4. No `libsqlite3.so.0` is needed, because Qt's `libqsqlite.so` statically
   embeds SQLite.
5. The AppImage starts and reaches the QML window in `debian:12` **and** in
   `ubuntu:24.04` with the desktop runtime installed, i.e. it stays running
   until killed, exit **124** under `timeout`.
6. The Wayland platform plugins (`libqwayland-egl.so`, `libqwayland-generic.so`)
   and `libqoffscreen.so` are deployed alongside `libqxcb.so`.
7. The whole pipeline is committed and reproducible from a clean checkout.

## Tasks

- **A1 — Spike + decisions.** Probe candidate base images for glibc, compiler,
  cmake, Qt, TagLib, libmpv. Decide base, Qt source, TagLib, size strategy.
  *Done (see Decisions table).*
- **A2 — Build environment and recipe.** `packaging/appimage/Dockerfile`,
  `packaging/appimage/build-in-container.sh` (recipe), `packaging/appimage/make.sh`
  (host entry point), `metadata/org.altisidora.Altisidora.appdata.xml`,
  `.gitignore`. *Done.*
- **A3 — Produce the AppImage.** Build the image, run the recipe, get the
  artifact. Iterate on failures. *Done — `dist/Altisidora-0.1.0-x86_64.AppImage`,
  122,497,528 bytes (~117 MiB, ~122 MB); gitignored, not committed.*
- **A4 — Verify.** Glibc scan, bundled-library inventory, run inside `debian:12`
  and `ubuntu:24.04` with the desktop runtime installed, plus bare `debian:12`
  and `debian:11` (expected to fail with `libGLX.so.0` missing) with
  `--appimage-extract-and-run`. *Done — see Verification evidence.*
- **A5 — Docs.** Update `docs/PACKAGING_LINUX.md` (planned → working, record the
  floor and the measured artifact size), `docs/PACKAGING.md` table,
  `docs/PROJECT_STATUS.md`. *Done.*

## Authorized scope

Commits on `feat/platform-seam` (continuing the current branch). No push. No
application source changes. New files under `packaging/` and `metadata/`.

## Delivery strategy

`ask-on-risk` (default). The whole pipeline is one reviewable work unit; the
generated AppImage is not committed.

## Progress log

- A1 done — probed `debian:11`, `ubuntu:20.04`, `ubuntu:22.04`, `debian:12`.
  Findings that drove the decisions: `debian:11` has g++-13 but cmake 3.18
  (< the required 3.20) and no Qt; `ubuntu:22.04` has native cmake 3.22, g++-12
  and Qt 6.2.4; every candidate ships TagLib ≤ 1.13, so TagLib 2.x must be built
  from source regardless. Docker Desktop's daemon was down; started it via its
  user service (no sudo needed). Its VM has ~1.8 GB RAM, which rules out
  building Qt or FFmpeg from source.
- A2–A5 done — built the environment and recipe, produced the AppImage, verified
  it, and updated the docs. Key correction during A3/A4: the earlier decision to
  bundle `libstdc++` / `libgcc_s` was reversed after `ubuntu:24.04` failed to
  start with `GLIBCXX_3.4.32 not found`; not bundling is correct and is coupled
  to the 2.35 floor. `libvulkan.so.1` and `libva*.so*` are bundled on purpose
  (loaders, not drivers).

## Verification evidence

Measured on the extracted `dist/Altisidora-0.1.0-x86_64.AppImage`:

- Artifact: **122,497,528 bytes** (~117 MiB, ~122 MB).
- glibc floor: exactly **GLIBC_2.35**. The highest requirement comes from the
  bundled FFmpeg / Cairo / slang libraries, not the application binary, whose own
  requirement tops out at **GLIBC_2.34**.
- Highest C++ runtime requirement anywhere: **GLIBCXX_3.4.29** (GCC 11). The app
  is compiled with `g++-12` but uses no C++ library symbol newer than GCC 11's
  runtime.
- `usr/lib`: **250** bundled libraries.
- Platform plugins deployed: `libqxcb.so`, `libqwayland-egl.so`,
  `libqwayland-generic.so`, `libqoffscreen.so`. Only `libqxcb.so` ships by
  default; the Wayland and offscreen plugins are forced via
  `EXTRA_PLATFORM_PLUGINS`.
- SQL driver: `libqsqlite.so` only.
- Applied `linuxdeploy` excludelist: **50 entries** (read from the tool's own
  binary). It does not contain `libvulkan.so.1` or `libva*.so*`; those are ICD /
  VA-API **loaders** and are bundled on purpose (the drivers live on the host).
  Excluding them would make `libmpv.so.2` fail to load on a host without
  `libvulkan1`.

Container runs (command shape: `docker run --rm -v "$PWD/dist":/dist:ro <image>
bash -c 'export HOME=/tmp QT_QPA_PLATFORM=offscreen; timeout 20
/dist/Altisidora-0.1.0-x86_64.AppImage --appimage-extract-and-run'`, after
installing the desktop runtime):

- **`debian:12`** → exit **124** (still running when `timeout` killed it, i.e.
  it started). Output: benign UTF-8 locale notice and `MPRIS: no session bus
  available`, the integration's designed fallback.
- **`ubuntu:24.04`** (newer than the build container: GCC 13, glibc 2.39) →
  exit **124**. This run exposed the bundled-`libstdc++` bug before the fix.
- Bare `debian:12` and bare `debian:11` → `libGLX.so.0: cannot open shared
  object file`, the expected consequence of the host-provided desktop stack.

## Next step

Nothing left in this feature. Only **publishing** remains — a `v0.1.0` tag and
real `sha256sums` — and it needs a push.
