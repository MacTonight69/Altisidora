#include "SettingsController.h"

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
    , m_settings("Altisidora", "Altisidora")
    , m_theme(QStringLiteral("Mimy"))
{
    load();
}

QString SettingsController::musicDirectory() const
{
    return m_musicDirectory;
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
    m_settings.setValue("theme", m_theme);
    m_settings.sync();
}

void SettingsController::load()
{
    m_musicDirectory = m_settings.value("musicDirectory").toString();
    m_theme = m_settings.value("theme", QStringLiteral("Mimy")).toString();

    // Theme renames: keep users on their already-selected theme after an
    // upgrade instead of silently falling back to the default.
    if (m_theme == QLatin1String("Desertic"))
        m_theme = QStringLiteral("Coco");
    else if (m_theme == QLatin1String("Anarchy"))
        m_theme = QStringLiteral("Maya");
}
