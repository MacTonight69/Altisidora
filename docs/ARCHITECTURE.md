# Music Player Architecture

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
Qt Multimedia      SQLite
GStreamer          TagLib
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

## Dependency rules

GUI must not directly access SQLite.

GUI must not depend on low-level GStreamer implementation.

Application controllers may coordinate domain services.

Infrastructure modules must not depend on QML.

Circular dependencies must be avoided.

## Threading

UI operations must remain responsive.

Potential background operations:

* Library scanning.
* Metadata extraction.
* Database-heavy operations.
* Artwork processing.

Threading must be introduced only when required.

## Error handling

Errors should propagate through established interfaces.

Low-level implementation details should not leak into the GUI.

## Extensibility

The architecture should permit:

* Alternative audio backends.
* Additional metadata providers.
* Additional playlist types.
* Additional visualizers.
* Additional audio effects.

without requiring major changes to unrelated modules.
