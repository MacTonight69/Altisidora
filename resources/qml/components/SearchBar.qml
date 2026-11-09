import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: searchBar

    signal queryChanged(string query)

    implicitWidth: 320
    implicitHeight: 34
    radius: 6
    color: field.activeFocus ? Theme.fieldFocus : Theme.field
    border.width: 1
    border.color: field.activeFocus ? Theme.accent : Theme.border

    Behavior on border.color {
        ColorAnimation { duration: 120 }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 4
        spacing: 6

        TextField {
            font.family: Theme.fontFamily
            id: field
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            padding: 0
            background: Item { }
            placeholderText: qsTr("Search songs…")
            placeholderTextColor: Theme.textMuted
            color: Theme.textPrimary
            selectionColor: Theme.fieldSelection
            selectedTextColor: Theme.fieldSelectedText
            font.pixelSize: 13 * Theme.fontScale
            verticalAlignment: TextInput.AlignVCenter
            onTextChanged: searchBar.queryChanged(text)
            Keys.onEscapePressed: function (event) {
                text = ""
                event.accepted = true
            }
        }

        ToolButton {
            font.family: Theme.fontFamily
            id: clearButton
            visible: field.text.length > 0
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            padding: 4

            contentItem: Text {
                text: "\u2715"
                color: clearButton.hovered ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: clearButton.hovered ? Theme.border : "transparent"
            }
            onClicked: {
                field.text = ""
                field.forceActiveFocus()
            }
        }
    }
}
