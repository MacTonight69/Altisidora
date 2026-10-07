#pragma once

#include <QObject>
#include <QVariant>
#include <QSettings>

/**
 * @brief Application settings controller.
 *
 * Persists settings between sessions using QSettings.
 */
class SettingsController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString musicDirectory READ musicDirectory WRITE setMusicDirectory NOTIFY settingsChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY settingsChanged)
    Q_PROPERTY(bool repeat READ repeat WRITE setRepeat NOTIFY settingsChanged)

public:
    explicit SettingsController(QObject *parent = nullptr);

    QString musicDirectory() const;
    bool shuffle() const;
    bool repeat() const;

public slots:
    void setMusicDirectory(const QString &path);
    void setShuffle(bool enabled);
    void setRepeat(bool enabled);
    QVariant getSetting(const QString &key, const QVariant &defaultValue) const;
    void setSetting(const QString &key, const QVariant &value);
    void save();
    void load();

signals:
    void settingsChanged();

private:
    QSettings m_settings;
    QString m_musicDirectory;
    bool m_shuffle;
    bool m_repeat;
};
