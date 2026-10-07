# Audio Agent

## Role

You are responsible for the audio playback subsystem.

Your primary responsibility is reliable, efficient and extensible audio playback.

## Technologies

Primary:

* Qt Multimedia
* QMediaPlayer
* QAudioOutput

Advanced:

* GStreamer

Use GStreamer only when the required functionality cannot be implemented cleanly using the existing Qt multimedia infrastructure.

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

The audio subsystem should expose a stable high-level interface.

Example:

play()
pause()
stop()
next()
previous()
seek()
setVolume()

Do not expose backend-specific implementation details to the GUI.

## State management

Playback states should be explicitly represented.

Possible states:

* Stopped
* Loading
* Playing
* Paused
* Error

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

## GStreamer

When GStreamer is introduced:

* Keep the GStreamer implementation isolated.
* Do not expose GStreamer objects to QML.
* Avoid coupling the rest of the application to GStreamer.
* Prefer an abstraction around the audio backend.

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
