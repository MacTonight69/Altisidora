# Altisidora Architecture

## Overview

The application is a native Linux music player primarily targeting CachyOS and Arch Linux.

The architecture is modular and separates presentation, application logic, domain logic and infrastructure.

## High-level architecture

```text
QML / Qt Quick
       │
       ▼
Application Controllers
       │
       ▼
Domain Models / Services
       │
       ├───────────────┐
       ▼               ▼
Audio           Library / Data
       │               │
       ▼               ▼
libmpv             SQLite
                   TagLib
```

## Modules

### GUI

Responsible for:

* Presentation.
* Navigation.
* User interaction.
* Animations.
* Visual states.

### Application

Responsible for:

* Coordinating operations.
* Connecting UI with domain services.
* Maintaining application-level state.

### Audio

Responsible for:

* Playback.
* Audio output.
* Playback state.
* Audio processing.

Implemented by `PlayerController`, which wraps a libmpv session (`src/audio/`).

### Library

Responsible for:

* Music scanning.
* Metadata.
* Music organization.
* Search.

### Database

Responsible for:

* Persistent data.
* Library storage.
* Playlists.
* Statistics.
* Settings where appropriate.

### System Integration

Responsible for:

* Notifications.
* System tray.
* Desktop integration.
* Multimedia keys.

Implemented by `MprisPlayer` (`src/mpris/`), which exposes the app as an MPRIS v2 media player over Qt DBus.

## Dependency rules

GUI must not directly access SQLite.

GUI must not depend on low-level libmpv implementation.

Application controllers may coordinate domain services.

Infrastructure modules must not depend on QML.

Circular dependencies must be avoided.

## Threading

UI operations must remain responsive. Library scanning and metadata extraction run on a worker thread (`LibraryController` + `QFutureWatcher`); other operations stay on the main thread until there is evidence they must move.

## Error handling

Public APIs report failures to their callers; low-level backend details must not leak into QML.
