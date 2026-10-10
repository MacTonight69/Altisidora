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
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY settingsChanged)
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY settingsChanged)
    Q_PROPERTY(bool deleteFilesFromDisk READ deleteFilesFromDisk WRITE setDeleteFilesFromDisk NOTIFY settingsChanged)

public:
    explicit SettingsController(QObject *parent = nullptr);

    QString musicDirectory() const;
    QString theme() const;
    double fontScale() const;
    bool deleteFilesFromDisk() const;

public slots:
    void setMusicDirectory(const QString &path);
    void setTheme(const QString &name);
    void setFontScale(double scale);
    void setDeleteFilesFromDisk(bool value);
    void save();
    void load();

signals:
    void settingsChanged();

private:
    QSettings m_settings;
    QString m_musicDirectory;
    QString m_theme;
    double m_fontScale = 1.0;
    bool m_deleteFilesFromDisk = true;
};
