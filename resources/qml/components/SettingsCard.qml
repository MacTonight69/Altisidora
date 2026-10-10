import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Bordered card used by the Settings page. Its default property is the
// content slot: declared children are placed inside the padded inner column.
// An optional `title` renders the shared section heading above the content
// (used by the cards without an inline header).
Rectangle {
    id: card

    Layout.fillWidth: true
    Layout.maximumWidth: 640
    implicitHeight: contentColumn.implicitHeight + 32
    radius: 6
    color: Theme.bgAlt
    border.width: 1
    border.color: Theme.border

    property string title: ""

    default property alias content: contentColumn.data

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Text {
            visible: card.title.length > 0
            text: card.title
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 14 * Theme.fontScale
            font.bold: true
        }
    }
}
