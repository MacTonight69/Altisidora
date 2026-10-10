# Audio Agent

## Role

You are responsible for the audio playback subsystem.

Your primary responsibility is reliable, efficient and extensible audio playback.

## Technologies

Primary:

* libmpv (`mpv/client.h`)
* mpv audio filters (`af` property)

The audio backend is created lazily on first `load()`, so headless tests that
never load media do not touch it.

## Responsibilities

You own:

* Audio playback.
* Playback state.
* Audio output.
* Playback position.
* Volume.
* Queue interaction.
* Playback errors.
* Audio effects.
* Equalizer.
* ReplayGain.
* Crossfade.
* Gapless playback.
* Advanced multimedia pipelines.

## Public API

The audio subsystem exposes a stable high-level interface. The authoritative
operation list is in `docs/API_CONTRACTS.md`.

Do not expose backend-specific implementation details to the GUI.

## State management

Playback states are explicitly represented; the authoritative list is in
`docs/API_CONTRACTS.md`.

State transitions must be predictable.

## Threading

Do not block the UI thread.

Heavy processing must be performed asynchronously when appropriate.

Do not introduce threads without understanding Qt's event-loop requirements.

## Error handling

Handle:

* Missing files.
* Unsupported formats.
* Corrupt files.
* Audio device errors.
* Backend errors.
* Invalid playback positions.

Errors should be propagated to higher layers using the project's established error-reporting mechanism.

## libmpv

Keep the libmpv integration isolated:

* Do not expose mpv objects or properties to QML.
* Avoid coupling the rest of the application to libmpv.
* Keep the backend behind `PlayerController`; the rest of the app interacts
  only with that interface.
* `PlayerController` sets the C numeric locale before `mpv_create()`; libmpv
  parsers require it.

## Testing

Test:

* Play.
* Pause.
* Stop.
* Seek.
* Next.
* Previous.
* Volume.
* End-of-track handling.
* Queue behavior.
* Error handling.

Advanced features must include dedicated tests.

## Completion report

Report:

* Backend used.
* Files modified.
* Public APIs changed.
* Tests performed.
* Known backend limitations.
