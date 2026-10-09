import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: settingsView
    color: "transparent"

    property int hiddenCount: Library.ignoredPaths.length

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

    FolderDialog {
        id: hideDirDialog
        title: qsTr("Hide folder")
        onAccepted: Library.ignorePath(settingsView.urlToPath(selectedFolder))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            text: qsTr("Settings")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 20 * Theme.fontScale
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
                    font.family: Theme.fontFamily
                    font.pixelSize: 14 * Theme.fontScale
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Theme")
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 13 * Theme.fontScale
                        elide: Text.ElideRight
                    }

                    ComboBox {
                        font.family: Theme.fontFamily
                        id: themeCombo
                        Layout.preferredWidth: 210
                        model: Theme.themeNames
                        currentIndex: Math.max(0, Theme.themeNames.indexOf(AppSettings.theme))
                        onActivated: {
                            // main.qml reacts to AppSettings.theme and re-applies
                            // the theme (palette + fontScale) in one place.
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
                    font.family: Theme.fontFamily
                    font.pixelSize: 14 * Theme.fontScale
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: settingsView.prettyPath(AppSettings.musicDirectory)
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 13 * Theme.fontScale
                        elide: Text.ElideMiddle
                    }

                    Button {
                        font.family: Theme.fontFamily
                        id: chooseButton
                        Layout.preferredHeight: 32
                        focusPolicy: Qt.NoFocus
                        text: qsTr("Choose…")
                        onClicked: dirDialog.open()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("Pick the folder that contains your music, then press “Scan library” in the sidebar to import it.")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 11 * Theme.fontScale
                }
            }
        }

        // ---- hidden items ----
        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 640
            implicitHeight: hiddenColumn.implicitHeight + 32
            radius: 6
            color: Theme.bgAlt
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: hiddenColumn
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Hidden items")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 14 * Theme.fontScale
                        font.bold: true
                    }

                    Button {
                        font.family: Theme.fontFamily
                        id: hideFolderButton
                        Layout.preferredHeight: 28
                        focusPolicy: Qt.NoFocus
                        text: qsTr("Hide folder…")
                        font.pixelSize: 11 * Theme.fontScale
                        onClicked: hideDirDialog.open()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("Songs and folders you hide stay on disk; they are simply not shown in the lists. Restore them here at any time.")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 11 * Theme.fontScale
                }

                Text {
                    Layout.fillWidth: true
                    visible: settingsView.hiddenCount === 0
                    wrapMode: Text.Wrap
                    text: qsTr("Nothing hidden yet. Use “Hide” / “Hide folder” on the song list, or “Hide folder…” above.")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 11 * Theme.fontScale
                }

                ListView {
                    Layout.fillWidth: true
                    implicitHeight: Math.min(settingsView.hiddenCount * 34, 170)
                    visible: settingsView.hiddenCount > 0
                    model: Library.ignoredPaths
                    clip: true
                    ScrollBar.vertical: ScrollBar { }

                    delegate: RowLayout {
                        required property string modelData
                        width: ListView.view.width
                        height: 34
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 12 * Theme.fontScale
                            elide: Text.ElideMiddle
                        }

                        Button {
                            font.family: Theme.fontFamily
                            text: qsTr("Restore")
                            Layout.preferredHeight: 26
                            focusPolicy: Qt.NoFocus
                            font.pixelSize: 11 * Theme.fontScale
                            onClicked: Library.unignorePath(modelData)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: settingsView.hiddenCount > 0

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        font.family: Theme.fontFamily
                        id: restoreAllButton
                        Layout.preferredHeight: 28
                        focusPolicy: Qt.NoFocus
                        text: qsTr("Restore all")
                        font.pixelSize: 11 * Theme.fontScale
                        onClicked: {
                            // Unhide each entry; iterate backwards because
                            // every call refreshes the library.
                            var paths = Library.ignoredPaths
                            for (var i = paths.length - 1; i >= 0; --i)
                                Library.unignorePath(paths[i])
                        }
                    }
                }
            }
        }

        // ---- save ----
        RowLayout {
            spacing: 12

            Button {
                font.family: Theme.fontFamily
                id: saveButton
                Layout.preferredHeight: 34
                focusPolicy: Qt.NoFocus
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
                font.family: Theme.fontFamily
                font.pixelSize: 12 * Theme.fontScale
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
