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

double SettingsController::fontScale() const
{
    return m_fontScale;
}

void SettingsController::setFontScale(double scale)
{
    if (m_fontScale != scale) {
        m_fontScale = scale;
        m_settings.setValue("fontScale", scale);
        emit settingsChanged();
    }
}

bool SettingsController::deleteFilesFromDisk() const
{
    return m_deleteFilesFromDisk;
}

void SettingsController::setDeleteFilesFromDisk(bool value)
{
    if (m_deleteFilesFromDisk != value) {
        m_deleteFilesFromDisk = value;
        m_settings.setValue("deleteFilesFromDisk", value);
        emit settingsChanged();
    }
}

void SettingsController::save()
{
    m_settings.setValue("musicDirectory", m_musicDirectory);
    m_settings.setValue("theme", m_theme);
    m_settings.setValue("fontScale", m_fontScale);
    m_settings.setValue("deleteFilesFromDisk", m_deleteFilesFromDisk);
    m_settings.sync();
}

void SettingsController::load()
{
    m_musicDirectory = m_settings.value("musicDirectory").toString();
    m_theme = m_settings.value("theme", QStringLiteral("Mimy")).toString();
    m_fontScale = qBound(0.5, m_settings.value("fontScale", 1.0).toDouble(), 2.0);
    m_deleteFilesFromDisk = m_settings.value("deleteFilesFromDisk", true).toBool();

    // Theme renames: keep users on their already-selected theme after an
    // upgrade instead of silently falling back to the default. "Maya" is
    // deliberately NOT migrated: it still names a theme (now the
    // Baskervville variant of the old palette), so remapping it would make
    // that theme impossible to select. Only the dead "Anarchy" key moves,
    // and it moves to the renamed palette.
    if (m_theme == QLatin1String("Desertic"))
        m_theme = QStringLiteral("Coco");
    else if (m_theme == QLatin1String("Anarchy"))
        m_theme = QStringLiteral("Anarchism");
}
