# Agent Development Guidelines

## Project

This repository contains a native Linux music player primarily targeted at CachyOS and other Arch Linux based distributions.

The application is being developed primarily with:

* C++20/23
* Qt 6
* Qt Quick
* QML
* libmpv (audio backend)
* CMake
* SQLite
* TagLib
* Qt DBus (MPRIS desktop integration)
* CTest / Qt Test
* Git

The project must prioritize:

* Modularity
* Maintainability
* Performance
* Low resource consumption
* Clear separation of responsibilities
* Native Linux integration
* Testability
* Extensibility

## General rules

1. Do not rewrite existing functionality unless explicitly required.
2. Do not modify another module's public API without coordinating with the architecture agent.
3. Do not introduce dependencies without documenting why they are necessary.
4. Prefer standard C++ and Qt functionality before adding external libraries.
5. Keep business logic outside QML whenever possible.
6. QML must primarily be responsible for presentation and UI behavior.
7. C++ must contain application logic, data management and system integration.
8. Do not place database logic directly inside UI components.
9. Do not place audio-engine implementation directly inside UI components.
10. Avoid global state unless there is a documented architectural reason.
11. Follow existing naming conventions.
12. Keep classes focused on a single responsibility.
13. Avoid premature optimization.
14. Every significant new feature must include appropriate tests.
15. Existing tests must continue passing after modifications.

## Git rules

Do not commit unrelated changes.

Each task should produce a focused set of modifications.

Commit messages should describe the actual change.

Example:

feat(audio): add queue management

fix(database): prevent duplicate songs

test(player): add playback state tests

refactor(gui): separate playback controls

## Working procedure

Before modifying code:

1. Inspect the existing architecture.
2. Read the relevant documentation.
3. Inspect the affected module.
4. Identify dependencies with other modules.
5. Determine whether the requested change affects an existing public API.

After modifying code:

1. Build the project.
2. Run relevant tests.
3. Run the complete test suite when practical.
4. Check for compiler warnings.
5. Verify that unrelated functionality still works.
6. Update documentation when the architecture or public API changes.

## Agent communication

Agents must document changes that affect other modules.

The following files are authoritative:

* `docs/ARCHITECTURE.md`
* `docs/API_CONTRACTS.md`
* `docs/PROJECT_STATUS.md`

Do not silently redefine interfaces documented in these files.

## Definition of done

A task is complete only when:

* The requested functionality is implemented.
* The project builds successfully.
* Relevant tests pass.
* Existing functionality has not been unnecessarily broken.
* Documentation is updated when necessary.
* The final response clearly reports:

  * What was changed.
  * Which files were modified.
  * Tests performed.
  * Known limitations.
  * Any required follow-up work.
