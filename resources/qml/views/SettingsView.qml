import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: settingsView
    color: "transparent"

    function prettyPath(path) {
        if (path === undefined || path === null || path === "")
            return qsTr("Not set")
        var s = String(path)
        if (s.indexOf("file://") === 0)
            s = s.substring("file://".length)
        return s
    }

    // QUrl -> plain local path
    function urlToPath(url) {
        var s = url.toString()
        if (s.indexOf("file://") === 0)
            s = s.substring(7)
        return decodeURIComponent(s)
    }

    FolderDialog {
        id: dirDialog
        title: qsTr("Select music folder")
        onAccepted: {
            AppSettings.musicDirectory = urlToPath(selectedFolder)
            AppSettings.save()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            text: qsTr("Settings")
            color: Theme.textPrimary
            font.pixelSize: 20
            font.bold: true
        }

        // ---- appearance (theme) ----
        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 640
            implicitHeight: appearanceColumn.implicitHeight + 32
            radius: 6
            color: Theme.bgAlt
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: appearanceColumn
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    text: qsTr("Appearance")
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Theme")
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    ComboBox {
                        id: themeCombo
                        Layout.preferredWidth: 210
                        model: Theme.themeNames
                        currentIndex: Math.max(0, Theme.themeNames.indexOf(AppSettings.theme))
                        onActivated: {
                            Theme.apply(currentText)
                            AppSettings.theme = currentText
                        }
                    }
                }
            }
        }

        // ---- music folder ----
        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 640
            implicitHeight: folderColumn.implicitHeight + 32
            radius: 6
            color: Theme.bgAlt
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: folderColumn
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    text: qsTr("Music folder")
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: settingsView.prettyPath(AppSettings.musicDirectory)
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        elide: Text.ElideMiddle
                    }

                    Button {
                        id: chooseButton
                        Layout.preferredHeight: 32
                        text: qsTr("Choose…")
                        onClicked: dirDialog.open()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("Pick the folder that contains your music, then press “Scan library” in the sidebar to import it.")
                    color: Theme.textMuted
                    font.pixelSize: 11
                }
            }
        }

        // ---- playback ----
        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 640
            implicitHeight: playbackColumn.implicitHeight + 32
            radius: 6
            color: Theme.bgAlt
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: playbackColumn
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    text: qsTr("Playback")
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                }

                CheckBox {
                    id: shuffleBox
                    text: qsTr("Shuffle")
                    font.pixelSize: 13
                    checked: AppSettings.shuffle
                    onToggled: AppSettings.shuffle = checked
                }

                CheckBox {
                    id: repeatBox
                    text: qsTr("Repeat")
                    font.pixelSize: 13
                    checked: AppSettings.repeat
                    onToggled: AppSettings.repeat = checked
                }
            }
        }

        // ---- save ----
        RowLayout {
            spacing: 12

            Button {
                id: saveButton
                Layout.preferredHeight: 34
                text: qsTr("Save settings")
                onClicked: {
                    AppSettings.save()
                    savedHint.visible = true
                    savedTimer.restart()
                }
            }

            Text {
                id: savedHint
                visible: false
                text: qsTr("Settings saved")
                color: Theme.success
                font.pixelSize: 12
            }

            Timer {
                id: savedTimer
                interval: 2500
                onTriggered: savedHint.visible = false
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
