#include <QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QUrl>
#include <QFont>
#include <QStringList>
#include <QVariantMap>

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
    // Exactly the four opt-in themes scale to 1.4; every other theme is 1.0.
    const QStringList scaled = {QStringLiteral("Coco"), QStringLiteral("Maya"),
                                QStringLiteral("Vacui"), QStringLiteral("The Niggest")};

    const auto names = m_theme->property("themeNames").toStringList();
    QVERIFY2(names.size() >= scaled.size(), "theme catalog must contain the scaled themes");

    int checked = 0;
    for (const QString &name : names) {
        applyTheme(name);
        const double expected = scaled.contains(name) ? 1.4 : 1.0;
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

    for (const QString &name : {QStringLiteral("Coco"), QStringLiteral("Maya"),
                                QStringLiteral("Vacui"), QStringLiteral("The Niggest")}) {
        applyTheme(name);
        QCOMPARE(m_theme->property("fontScale").toDouble(), 1.4);
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
    applyTheme(QStringLiteral("Vacui"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.4);
    applyTheme(QStringLiteral("Mimy"));
    QCOMPARE(m_theme->property("fontScale").toDouble(), 1.0);
}

QTEST_MAIN(tst_theme_fonts)
#include "tst_theme_fonts.moc"