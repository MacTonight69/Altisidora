#include <QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QUrl>
#include <QFont>
#include <QSettings>
#include <QTemporaryDir>
#include <QStringList>
#include <QVariantMap>

#include "../../src/app/SettingsController.h"

// Directory holding Theme.qml, injected by tests/CMakeLists.txt.
#ifndef THEME_QML_DIR
#define THEME_QML_DIR ""
#endif

/**
 * @brief Regression tests for the per-theme typography scaling mechanism.
 *
 * Reproduces how the real app wires Theme: main.cpp instantiates Theme.qml
 * once and exposes it as the "Theme" context property, so every QML document
 * can bind to Theme.<role>. On Theme.apply(name) every role (fontScale
 * included) is assigned and every binding re-evaluates. These tests pin
 * that contract so a future change cannot silently break live re-scaling.
 */
class tst_theme_fonts : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testThemeDataScaleRule();
    void testUnknownThemeFallsBackToMimy();
    void testApplyAssignsFontScale();
    void testBindingRerendersOnApply();
    void testScaleDropsBackOnMimy();
    void testRuntimeThemeSwitchChain();
    void testStartupAppliesSavedTheme();
    void testVacuiUsesSystemFontFamily();
    void testSidebarWidthPerTheme();

private:
    void applyTheme(const QString &name);
    static void settle();

    QQmlEngine *m_engine = nullptr;
    QObject *m_theme = nullptr;
};

void tst_theme_fonts::initTestCase()
{
    m_engine = new QQmlEngine(this);

    QQmlComponent component(m_engine,
                            QUrl::fromLocalFile(QStringLiteral(THEME_QML_DIR)
                                                + QStringLiteral("/Theme.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));

    m_theme = component.create();
    QVERIFY(m_theme != nullptr);

    // Mirror src/main.cpp: the app exposes Theme as a root context property.
    m_engine->rootContext()->setContextProperty(QStringLiteral("Theme"), m_theme);
}

void tst_theme_fonts::applyTheme(const QString &name)
{
    QVERIFY(QMetaObject::invokeMethod(m_theme, "apply", Q_ARG(QVariant, QVariant(name))));
}

void tst_theme_fonts::settle()
{
    // Let layout polish runs for text metrics to take effect.
    QCoreApplication::processEvents();
    QTest::qWait(20);
    QCoreApplication::processEvents();
}

void tst_theme_fonts::testThemeDataScaleRule()
{
    // Per-theme opt-in typography scale; every other theme stays at 1.0.
    // Maya is NOT in this list: the Baskervville Maya added with the
    // Maya->Anarchism rename uses the default scale, and the 1.9
    // red-and-black palette lives under "Anarchism". (The option-E spec had
    // given the new Maya 1.9 + a 260px sidebar; those two overrides were
    // dropped in 96fadcf without updating this test, which is the divergence
    // this map now pins.)
    const QHash<QString, double> scales = {
        {QStringLiteral("Coco"), 1.4},
        {QStringLiteral("Anarchism"), 1.9},
        {QStringLiteral("Onyx"), 1.4},
    };

    const auto names = m_theme->property("themeNames").toStringList();
    QVERIFY2(names.size() >= scales.size(), "theme catalog must contain the scaled themes");

    int checked = 0;
    for (const QString &name : names) {
        applyTheme(name);
        const double expected = scales.value(name, 1.0);
        QCOMPARE(m_theme->property("fontScale").toDouble(), expected);
        ++checked;
    }
    QVERIFY(checked > 1);
}

void tst_theme_fonts::testUnknownThemeFallsBackToMimy()
{
    applyTheme(QStringLiteral("No Such Theme"));
    QCOMPARE(m_theme->property("themeName").toString(), QStringLiteral("Mimy"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.0);
}

void tst_theme_fonts::testApplyAssignsFontScale()
{
    applyTheme(QStringLiteral("Mimy"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.0);

    // Same per-theme contract: each opt-in theme applies its own scale.
    const QHash<QString, double> scales = {
        {QStringLiteral("Coco"), 1.4},
        {QStringLiteral("Anarchism"), 1.9},
        {QStringLiteral("Onyx"), 1.4},
    };
    const QList<QString> names = scales.keys();
    for (const QString &name : names) {
        applyTheme(name);
        QCOMPARE(m_theme->property("fontScale").toDouble(), scales.value(name));
    }
}

void tst_theme_fonts::testBindingRerendersOnApply()
{
    QQmlComponent component(m_engine);
    component.setData(
        QStringLiteral("import QtQuick\n"
                       "import QtQuick.Controls\n"
                       "ApplicationWindow {\n"
                       "    visible: true\n"
                       "    width: 400\n"
                       "    height: 100\n"
                       "    Text {\n"
                       "        id: probe\n"
                       "        objectName: \"probe\"\n"
                       "        text: \"Altisidora Fuentes 0123\"\n"
                       "        font.family: \"sans-serif\"\n"
                       "        font.pixelSize: 13 * Theme.fontScale\n"
                       "    }\n"
                       "}\n").toUtf8(),
        QUrl());
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));

    auto *window = qobject_cast<QQuickWindow *>(component.create());
    QVERIFY(window != nullptr);
    window->show();
    settle();

    auto *text = window->findChild<QQuickItem *>(QStringLiteral("probe"));
    QVERIFY(text != nullptr);

    const auto pixelSize = [text]() {
        return text->property("font").value<QFont>().pixelSize();
    };

    // Baseline: Mimy -> fontScale 1.0 -> 13px.
    applyTheme(QStringLiteral("Mimy"));
    settle();
    const double basePixel = pixelSize();
    const double baseWidth = text->implicitWidth();
    QCOMPARE(basePixel, 13.0);

    // Coco -> fontScale 1.4 -> the binding must produce 18.2px (QFont
    // rounds to whole pixels: 18) and a wider text.
    applyTheme(QStringLiteral("Coco"));
    settle();
    const double scaledPixel = pixelSize();
    const double scaledWidth = text->implicitWidth();
    QCOMPARE(scaledPixel, double(qRound(13.0 * 1.4)));
    QVERIFY2(scaledWidth > baseWidth,
             qPrintable(QStringLiteral("implicitWidth did not grow: base %1 scaled %2")
                            .arg(baseWidth)
                            .arg(scaledWidth)));

    // Back to Mimy -> 13px again (scale is fully reversible).
    applyTheme(QStringLiteral("Mimy"));
    settle();
    QCOMPARE(pixelSize(), basePixel);
    QVERIFY(text->implicitWidth() < scaledWidth);
}

void tst_theme_fonts::testScaleDropsBackOnMimy()
{
    // Anarchism is the widest-typeface theme that opts into 1.9.
    applyTheme(QStringLiteral("Anarchism"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.9);
    applyTheme(QStringLiteral("Mimy"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.0);
}

void tst_theme_fonts::testVacuiUsesSystemFontFamily()
{
    // Vacui ships no bundled typeface: it names an OS-installed family.
    applyTheme(QStringLiteral("Vacui"));
    QCOMPARE(m_theme->property("themeName").toString(), QStringLiteral("Vacui"));
    QCOMPARE(m_theme->property("fontFamily").toString(), QStringLiteral("Liberation Serif"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.0);
}

void tst_theme_fonts::testSidebarWidthPerTheme()
{
    // Themes that do not override it keep the standard sidebar width, Maya
    // (Baskervville) included: only the themes that opt into the larger type
    // pay for a wider sidebar.
    applyTheme(QStringLiteral("Mimy"));
    QCOMPARE(m_theme->property("sidebarWidth").toDouble(), 210.0);

    applyTheme(QStringLiteral("Maya"));
    QCOMPARE(m_theme->property("sidebarWidth").toDouble(), 210.0);

    // Anarchism opts into a wider sidebar so its 1.9-scaled nav labels fit.
    applyTheme(QStringLiteral("Anarchism"));
    QVERIFY2(m_theme->property("sidebarWidth").toDouble() > 210.0,
             "Anarchism must widen the sidebar for its larger typeface");

    // Switching away must restore the default: apply() always assigns the
    // role, so the previous theme's width must not stick.
    applyTheme(QStringLiteral("Mimy"));
    QCOMPARE(m_theme->property("sidebarWidth").toDouble(), 210.0);
}

void tst_theme_fonts::testRuntimeThemeSwitchChain()
{
    // Isolate QSettings so the production SettingsController writes to a
    // temporary config instead of the real user config. XDG_CONFIG_HOME is
    // the authoritative UserScope root on Linux and cannot be bypassed by
    // QSettings format/path resolution.
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QByteArray oldXdg = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", tempDir.path().toUtf8());

    // The real production settings object (same class src/main.cpp exposes).
    SettingsController settings;
    QCOMPARE(settings.theme(), QStringLiteral("Mimy")); // fresh default

    // Expose it exactly like src/main.cpp does.
    m_engine->rootContext()->setContextProperty(QStringLiteral("AppSettings"), &settings);

    // The exact wiring from resources/qml/main.qml: the Connections handler
    // and applyTheme(), copied verbatim. This is the runtime chain the user
    // exercises with the Settings combo: setTheme -> settingsChanged ->
    // Connections -> applyTheme -> Theme.apply + persist.
    QQmlComponent wiring(m_engine);
    wiring.setData(
        QStringLiteral("import QtQuick\n"
                       "Item {\n"
                       "    id: root\n"
                       "    function applyTheme(name) {\n"
                       "        Theme.apply(name)\n"
                       "        if (name !== AppSettings.theme)\n"
                       "            AppSettings.theme = name\n"
                       "        AppSettings.fontScale = Theme.fontScale\n"
                       "    }\n"
                       "    Connections {\n"
                       "        target: AppSettings\n"
                       "        function onSettingsChanged() {\n"
                       "            root.applyTheme(AppSettings.theme)\n"
                       "        }\n"
                       "    }\n"
                       "}\n").toUtf8(),
        QUrl());
    QVERIFY2(wiring.status() == QQmlComponent::Ready,
             qPrintable(wiring.errorString()));
    QScopedPointer<QObject> root(wiring.create());
    QVERIFY(root != nullptr);

    // Drive the same path the Settings combo uses: switching to a scaled
    // theme must re-apply the palette AND the typography scale, and persist
    // both to the config. Anarchism carries the largest rhythm check: 1.9 in
    // both the applied palette and the config.
    settings.setTheme(QStringLiteral("Anarchism"));
    QCOMPARE(settings.theme(), QStringLiteral("Anarchism"));
    QCOMPARE(m_theme->property("themeName").toString(), QStringLiteral("Anarchism"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.9);
    QCOMPARE(settings.fontScale(), 1.9);

    // Both values must be persisted to the config file.
    QSettings check(QStringLiteral("Altisidora"), QStringLiteral("Altisidora"));
    QCOMPARE(check.value(QStringLiteral("theme")).toString(),
             QStringLiteral("Anarchism"));
    QCOMPARE(check.value(QStringLiteral("fontScale")).toDouble(), 1.9);

    // Switching back to a non-scaled theme drops the scale and persists it.
    settings.setTheme(QStringLiteral("Mimy"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.0);
    QCOMPARE(settings.fontScale(), 1.0);
    QCOMPARE(check.value(QStringLiteral("fontScale")).toDouble(), 1.0);

    // Restore the environment for any code that runs after this test.
    if (oldXdg.isEmpty())
        qunsetenv("XDG_CONFIG_HOME");
    else
        qputenv("XDG_CONFIG_HOME", oldXdg);
}

void tst_theme_fonts::testStartupAppliesSavedTheme()
{
    // Reproduces the closed-then-reopened app: a theme saved in the config
    // must be the one applied from the first frame. Isolate QSettings the
    // same way testRuntimeThemeSwitchChain does.
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QByteArray oldXdg = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", tempDir.path().toUtf8());

    // A previous session that closed on Onyx.
    {
        QSettings seed(QStringLiteral("Altisidora"), QStringLiteral("Altisidora"));
        seed.setValue(QStringLiteral("theme"), QStringLiteral("Onyx"));
        seed.sync();
    }

    // The app loads the saved theme into SettingsController at construction.
    SettingsController settings;
    QCOMPARE(settings.theme(), QStringLiteral("Onyx"));

    m_engine->rootContext()->setContextProperty(QStringLiteral("AppSettings"), &settings);

    // The exact wiring from resources/qml/main.qml, including the startup
    // apply. Before the fix the handler only ran on later settingsChanged
    // emissions, so the UI stayed on the Theme.qml default (Mimy) while the
    // Settings combo showed the saved theme.
    QQmlComponent wiring(m_engine);
    wiring.setData(
        QStringLiteral("import QtQuick\n"
                       "Item {\n"
                       "    id: root\n"
                       "    function applyTheme(name) {\n"
                       "        Theme.apply(name)\n"
                       "        if (name !== AppSettings.theme)\n"
                       "            AppSettings.theme = name\n"
                       "        AppSettings.fontScale = Theme.fontScale\n"
                       "    }\n"
                       "    Connections {\n"
                       "        target: AppSettings\n"
                       "        function onSettingsChanged() {\n"
                       "            root.applyTheme(AppSettings.theme)\n"
                       "        }\n"
                       "    }\n"
                       "    Component.onCompleted: root.applyTheme(AppSettings.theme)\n"
                       "}\n").toUtf8(),
        QUrl());
    QVERIFY2(wiring.status() == QQmlComponent::Ready,
             qPrintable(wiring.errorString()));
    QScopedPointer<QObject> root(wiring.create());
    QVERIFY(root != nullptr);

    // The saved theme (and its typography scale) must be live immediately.
    QCOMPARE(m_theme->property("themeName").toString(), QStringLiteral("Onyx"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.4);

    if (oldXdg.isEmpty())
        qunsetenv("XDG_CONFIG_HOME");
    else
        qputenv("XDG_CONFIG_HOME", oldXdg);

    // Leave the shared Theme object neutral for any code running after.
    applyTheme(QStringLiteral("Mimy"));
}

QTEST_MAIN(tst_theme_fonts)
#include "tst_theme_fonts.moc"