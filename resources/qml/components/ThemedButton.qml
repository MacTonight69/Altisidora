import QtQuick
import QtQuick.Controls

// Flat, chrome-less text button shared by the play toolbar and the song /
// playlist selection bars. It replaces the repeated custom Button +
// contentItem + transparent background boilerplate.
//
// The label colour is driven by the interaction state; callers only need to
// override the hover tone (e.g. `hoverColor: Theme.accent`) when it differs
// from the default danger tint.
Button {
    id: control

    focusPolicy: Qt.NoFocus
    flat: true
    font.family: Theme.fontFamily
    font.pixelSize: 12 * Theme.fontScale

    property color idleColor: Theme.textSecondary
    property color hoverColor: Theme.danger
    property color disabledColor: Theme.textMuted

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? control.disabledColor
               : control.hovered ? control.hoverColor : control.idleColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle { color: "transparent" }
}
