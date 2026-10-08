#include "SettingsController.h"

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
    , m_settings("Altisidora", "Altisidora")
    , m_shuffle(false)
    , m_repeat(false)
    , m_theme(QStringLiteral("Mimy"))
{
    load();
}

QString SettingsController::musicDirectory() const
{
    return m_musicDirectory;
}

bool SettingsController::shuffle() const
{
    return m_shuffle;
}

bool SettingsController::repeat() const
{
    return m_repeat;
}

QString SettingsController::theme() const
{
    return m_theme;
}

void SettingsController::setMusicDirectory(const QString &path)
{
    if (m_musicDirectory != path) {
        m_musicDirectory = path;
        m_settings.setValue("musicDirectory", path);
        emit settingsChanged();
    }
}

void SettingsController::setShuffle(bool enabled)
{
    if (m_shuffle != enabled) {
        m_shuffle = enabled;
        m_settings.setValue("shuffle", enabled);
        emit settingsChanged();
    }
}

void SettingsController::setRepeat(bool enabled)
{
    if (m_repeat != enabled) {
        m_repeat = enabled;
        m_settings.setValue("repeat", enabled);
        emit settingsChanged();
    }
}

void SettingsController::setTheme(const QString &name)
{
    if (m_theme != name) {
        m_theme = name;
        m_settings.setValue("theme", name);
        emit settingsChanged();
    }
}

QVariant SettingsController::getSetting(const QString &key, const QVariant &defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void SettingsController::setSetting(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    emit settingsChanged();
}

void SettingsController::save()
{
    m_settings.setValue("musicDirectory", m_musicDirectory);
    m_settings.setValue("shuffle", m_shuffle);
    m_settings.setValue("repeat", m_repeat);
    m_settings.setValue("theme", m_theme);
    m_settings.sync();
}

void SettingsController::load()
{
    m_musicDirectory = m_settings.value("musicDirectory").toString();
    m_shuffle = m_settings.value("shuffle", false).toBool();
    m_repeat = m_settings.value("repeat", false).toBool();
    m_theme = m_settings.value("theme", QStringLiteral("Mimy")).toString();
}
