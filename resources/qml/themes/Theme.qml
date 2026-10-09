// Theme role provider for Altisidora.
//
// Not a QML singleton: main.cpp instantiates this component once and
// exposes it as the "Theme" context property, so every QML document can
// bind to Theme.<role> without any import. When Theme.apply(name) runs,
// every binding that reads a role re-evaluates and the whole UI restyles
// live.
import QtQuick
import "Themes.js" as ThemeData

QtObject {
    id: theme

    // ---- active theme ----
    property string themeName: "Mimy"
    readonly property var themeNames: ThemeData.order

    // Typeface for the whole UI. Empty string means "system default", used
    // until a theme explicitly sets its own bundled font.
    property string fontFamily: ""

    // Multiplier applied to every font.pixelSize in the UI. Defaults to 1.0
    // so themes that omit it keep the standard size; themes can opt into a
    // larger effective typography by setting fontScale (e.g. 1.2).
    property real fontScale: 1.0

    // Fired at the end of apply(), once every role (font included) is in
    // place. main.cpp listens and mirrors the typeface into the application
    // font (QGuiApplication::setFont) for Qt-owned surfaces like dialogs.
    signal applied()

    // ---- base roles (defaults: the Mimy palette) ----
    property color bg: "#eef0f6"
    property color bgAlt: "#ffffff"
    property color viewBg: "#f7f8fc"
    property color field: "#ffffff"
    property color border: "#c3c9d6"
    property color textPrimary: "#1f2740"
    property color textSecondary: "#4a5470"
    property color textMuted: "#7d87a3"
    property color accent: "#ff7900"
    property color accentAlt: "#1d54c8"
    property color accentText: "#ffffff"
    property color selection: "#ffd9b3"
    property color selectionBorder: "#ff7900"
    property color selectionText: "#a14c00"
    property color selectionTextMuted: "#b06a20"
    property color danger: "#e0484d"
    property color dangerBg: "#fbdcdc"
    property color fieldSelection: "#ffcf9f"
    property color fieldSelectedText: "#4a3000"
    property color success: "#2e9e5b"

    // ---- derived roles (recomputed by apply()) ----
    property color accentHover
    property color accentPressed
    property color accentAltHover
    property color accentAltPressed
    property color rowHover
    property color rowActive
    property color rowCurrent
    property color fieldFocus
    property color borderPressed
    property color scrim

    function isLight(color) {
        return color.r * 0.299 + color.g * 0.587 + color.b * 0.114 > 0.5
    }

    // "Push" a tone: darken on light themes, lighten on dark themes.
    function mix(color, factor) {
        return isLight(color) ? Qt.darker(color, factor) : Qt.lighter(color, factor)
    }

    function apply(name) {
        var t = ThemeData.themes[name]
        if (!t)
            t = ThemeData.themes["Mimy"]

        themeName = t.name
        fontFamily = t.fontFamily !== undefined ? t.fontFamily : ""
        fontScale = t.fontScale !== undefined ? t.fontScale : 1.0
        bg = t.bg
        bgAlt = t.bgAlt
        viewBg = t.viewBg
        field = t.field
        border = t.border
        textPrimary = t.textPrimary
        textSecondary = t.textSecondary
        textMuted = t.textMuted
        accent = t.accent
        accentAlt = t.accentAlt
        accentText = t.accentText
        selection = t.selection
        selectionBorder = t.selectionBorder
        selectionText = t.selectionText
        selectionTextMuted = t.selectionTextMuted
        danger = t.danger
        dangerBg = t.dangerBg
        fieldSelection = t.fieldSelection
        fieldSelectedText = t.fieldSelectedText
        success = t.success

        // Interactive variants derived from the base tones.
        accentHover = Qt.lighter(t.accent, 1.3)
        accentPressed = Qt.darker(t.accent, 1.3)
        accentAltHover = Qt.lighter(t.accentAlt, 1.3)
        accentAltPressed = Qt.darker(t.accentAlt, 1.3)
        rowHover = mix(t.bgAlt, 1.10)
        rowActive = mix(t.bgAlt, 1.22)
        rowCurrent = mix(t.viewBg, 1.10)
        fieldFocus = mix(t.field, 1.05)
        borderPressed = mix(t.border, 1.25)
        scrim = Qt.rgba(t.bg.r, t.bg.g, t.bg.b, 0.85)

        applied()
    }

    Component.onCompleted: theme.apply(themeName)
}