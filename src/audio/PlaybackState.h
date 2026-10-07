#pragma once

#include <QMetaObject>

/**
 * @brief Playback state enumeration for the audio subsystem.
 *
 * Represents the possible states of the media player.
 * Used by PlayerController to communicate state to QML.
 */
class PlaybackState
{
    Q_GADGET
public:
    enum State {
        Stopped,
        Loading,
        Playing,
        Paused,
        Error
    };
    Q_ENUM(State)
};
