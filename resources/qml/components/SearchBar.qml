import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: searchBar

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    signal queryChanged(string query)

    implicitWidth: 320
    implicitHeight: 34
    radius: 6
    color: field.activeFocus ? "#232435" : "#181825"
    border.width: 1
    border.color: field.activeFocus ? "#89b4fa" : "#313244"

    Behavior on border.color {
        ColorAnimation { duration: 120 }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 4
        spacing: 6

        TextField {
            id: field
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            padding: 0
            background: Item { }
            placeholderText: qsTr("Search songs…")
            placeholderTextColor: "#6c7086"
            color: "#cdd6f4"
            selectionColor: "#45475a"
            selectedTextColor: "#11111b"
            font.pixelSize: 13
            verticalAlignment: TextInput.AlignVCenter
            onTextChanged: searchBar.queryChanged(text)
            Keys.onEscapePressed: function (event) {
                text = ""
                event.accepted = true
            }
        }

        ToolButton {
            id: clearButton
            visible: field.text.length > 0
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            padding: 4

            contentItem: Text {
                text: "\u2715"
                color: clearButton.hovered ? "#cdd6f4" : "#a6adc8"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: clearButton.hovered ? "#313244" : "transparent"
            }
            onClicked: {
                field.text = ""
                field.forceActiveFocus()
            }
        }
    }
}
