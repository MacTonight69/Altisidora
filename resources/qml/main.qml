import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 800
    title: qsTr("MusicRep")
    minimumWidth: 800
    minimumHeight: 600

    // Main layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Content area
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Sidebar
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: "#181825"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5

                    Text {
                        text: qsTr("MusicRep")
                        color: "#cdd6f4"
                        font.pixelSize: 20
                        font.bold: true
                    }

                    Button {
                        text: qsTr("Songs")
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Albums")
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Artists")
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Playlists")
                        Layout.fillWidth: true
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // Main content
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#1e1e2e"

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Library")
                    color: "#cdd6f4"
                    font.pixelSize: 24
                }
            }
        }

        // Player controls
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            color: "#181825"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10

                Button {
                    text: "⏮"
                }

                Button {
                    text: "▶"
                }

                Button {
                    text: "⏸"
                }

                Button {
                    text: "⏹"
                }

                Button {
                    text: "⏭"
                }

                Item {
                    Layout.fillWidth: true
                }

                Slider {
                    from: 0
                    to: 1
                    value: 0.8
                    Layout.preferredWidth: 150
                }
            }
        }
    }
}
