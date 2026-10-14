#pragma once

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusAbstractAdaptor>

#include "MediaIntegration.h"

/**
 * @brief Exposes Altisidora playback as an MPRIS v2 media player.
 *
 * Registers org.mpris.MediaPlayer2.altisidora on the session bus so the
 * desktop shell and system media controls (MangoWM media indicators,
 * media keys, sound menus) recognise the app while audio is playing.
 */
class MprisMediaIntegration : public MediaIntegration
{
    Q_OBJECT

public:
    explicit MprisMediaIntegration(PlayerController *player, QObject *parent = nullptr);

    /**
     * @brief Register the D-Bus service and object.
     * @return true when the MPRIS service is live on the session bus
     */
    bool start() override;

    /**
     * @brief Unregister the D-Bus service and object.
     */
    void stop() override;
};

/**
 * @brief org.mpris.MediaPlayer2 root interface.
 */
class MediaPlayer2Adaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")

    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

public:
    explicit MediaPlayer2Adaptor(MprisMediaIntegration *parent);

    bool canQuit() const { return true; }
    bool canRaise() const { return false; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("Altisidora"); }
    QString desktopEntry() const { return QStringLiteral("altisidora"); }
    QStringList supportedUriSchemes() const { return {QStringLiteral("file")}; }
    QStringList supportedMimeTypes() const { return {}; }

public slots:
    void Raise() {}
    void Quit();
};

/**
 * @brief org.mpris.MediaPlayer2.Player transport interface.
 */
class PlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    Q_PROPERTY(QString LoopStatus READ loopStatus CONSTANT)
    Q_PROPERTY(double Rate READ rate CONSTANT)
    Q_PROPERTY(bool Shuffle READ shuffle CONSTANT)
    Q_PROPERTY(QVariantMap Metadata READ metadata NOTIFY metadataChanged)
    Q_PROPERTY(double Volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(qint64 Position READ position NOTIFY positionChanged)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext NOTIFY canGoNextChanged)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious NOTIFY canGoPreviousChanged)
    Q_PROPERTY(bool CanPlay READ canPlay CONSTANT)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek CONSTANT)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)

public:
    explicit PlayerAdaptor(MprisMediaIntegration *parent);

    QString playbackStatus() const;
    QString loopStatus() const { return QStringLiteral("None"); }
    double rate() const { return 1.0; }
    bool shuffle() const { return false; }
    QVariantMap metadata() const;
    double volume() const;
    void setVolume(double volume);
    qint64 position() const;
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const { return true; }
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qint64 offset);
    void SetPosition(const QDBusObjectPath &trackId, qint64 position);
    void OpenUri(const QString &uri);

signals:
    void playbackStatusChanged();
    void metadataChanged();
    void volumeChanged();
    void positionChanged();
    void canGoNextChanged();
    void canGoPreviousChanged();

private:
    void emitPropertiesChanged(const QVariantMap &changed);

    MprisMediaIntegration *m_player;
    // Last whole second relayed through the (non-standard) positionChanged
    // D-Bus signal; used to coalesce the ~6.7/s poll into ~1/s.
    qint64 m_lastPositionSecond = -1;
};
