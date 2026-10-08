import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: controls

    implicitHeight: 80
    color: Theme.bgAlt

    property string errorText: ""

    readonly property string songTitle: (Player.currentSong && Player.currentSong.title)
                                        ? Player.currentSong.title : qsTr("No song playing")
    readonly property string songArtist: (Player.currentSong && Player.currentSong.artist)
                                         ? Player.currentSong.artist : ""

    function formatTime(ms) {
        var total = Math.floor(Number(ms || 0) / 1000)
        if (!isFinite(total) || total < 0)
            total = 0
        var minutes = Math.floor(total / 60)
        var seconds = total % 60
        return minutes + ":" + (seconds < 10 ? "0" + seconds : "" + seconds)
    }

    component TransportButton: ToolButton {
        id: tButton
        property string iconText: ""
        property bool primary: false
        property bool circular: false
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        padding: 8

        contentItem: Text {
            text: tButton.iconText
            color: tButton.primary ? Theme.accentText : Theme.textPrimary
            font.pixelSize: tButton.primary ? 17 : 14
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            // circular => a perfect circle (radius follows the height);
            // otherwise keep the rounded look.
            radius: tButton.circular ? height / 2
                   : tButton.primary ? 17 : 6
            color: tButton.primary
                   ? (tButton.down ? Theme.accentPressed : tButton.hovered ? Theme.accentHover : Theme.accent)
                   : (tButton.down ? Theme.borderPressed : tButton.hovered ? Theme.border : "transparent")
            Behavior on color {
                ColorAnimation { duration: 100 }
            }
        }
    }

    // Play/pause: both states are drawn on a canvas so they share the exact
    // same geometry and stay perfectly centered in the circle. The text
    // glyphs (▶/⏸) come from different fonts with different metrics, which
    // made the icon jump sideways when toggling.
    component PlayPauseButton: ToolButton {
        id: ppButton
        property bool playing: false
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        padding: 0
        implicitWidth: 44
        implicitHeight: 44

        Canvas {
            anchors.centerIn: parent
            width: 30
            height: 30

            property color fillColor: Theme.accentText
            property bool isPause: ppButton.playing
            onFillColorChanged: requestPaint()
            onIsPauseChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = fillColor
                if (isPause) {
                    // two symmetric bars centred on (15,15)
                    ctx.fillRect(7, 8, 5, 14)
                    ctx.fillRect(18, 8, 5, 14)
                } else {
                    // right-pointing triangle, centroid exactly at (15,15)
                    ctx.beginPath()
                    ctx.moveTo(11, 8)
                    ctx.lineTo(11, 22)
                    ctx.lineTo(24, 15)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }

        background: Rectangle {
            radius: height / 2
            color: ppButton.down ? Theme.accentPressed
                   : ppButton.hovered ? Theme.accentHover : Theme.accent
            Behavior on color {
                ColorAnimation { duration: 100 }
            }
        }

        onClicked: {
            if (ppButton.playing)
                Player.pause()
            else
                Player.play()
        }
    }

    // transient error banner (Player.errorOccurred)
    Rectangle {
        id: errorBanner
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? 24 : 0
        visible: controls.errorText !== ""
        color: Theme.dangerBg
        z: 1

        Text {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            verticalAlignment: Text.AlignVCenter
            text: controls.errorText
            color: Theme.danger
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    Timer {
        id: errorTimer
        interval: 5000
        onTriggered: controls.errorText = ""
    }

    Connections {
        target: Player
        function onErrorOccurred(message) {
            controls.errorText = message
            errorTimer.restart()
        }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 14 + errorBanner.height
        anchors.bottomMargin: 12
        spacing: 20

        // ---- now playing ----
        ColumnLayout {
            Layout.preferredWidth: 240
            Layout.maximumWidth: 320
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: controls.songTitle
                color: Theme.textPrimary
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: controls.songArtist !== ""
                text: controls.songArtist
                color: Theme.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        // ---- transport + seek ----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                TransportButton {
                    iconText: "\u23EE" // ⏮
                    onClicked: Player.previous()
                }
                PlayPauseButton {
                    playing: Player.isPlaying
                }
                TransportButton {
                    iconText: "\u23F9" // ⏹
                    onClicked: Player.stop()
                }
                TransportButton {
                    iconText: "\u23ED" // ⏭
                    onClicked: Player.next()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignRight
                    text: controls.formatTime(Player.position)
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }

                Slider {
                    id: seekSlider
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    focusPolicy: Qt.NoFocus
                    from: 0
                    to: Math.max(Player.duration, 1)
                    value: Player.position
                    onMoved: Player.seek(Math.round(value))
                    onPressedChanged: {
                        if (pressed) {
                            // Drop the position binding so playback updates
                            // cannot fight the user while dragging.
                            value = value
                        } else {
                            value = Qt.binding(function () { return Player.position })
                        }
                    }
                }

                Text {
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignLeft
                    text: controls.formatTime(Player.duration)
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }
            }
        }

        // ---- volume ----
        RowLayout {
            spacing: 8

            Text {
                Layout.preferredWidth: 34
                horizontalAlignment: Text.AlignRight
                text: Math.round(Player.volume * 100) + "%"
                color: Theme.textSecondary
                font.pixelSize: 11
            }

            Slider {
                id: volumeSlider
                Layout.preferredWidth: 120
                Layout.preferredHeight: 22
                focusPolicy: Qt.NoFocus
                from: 0
                to: 1
                value: Player.volume
                onMoved: Player.volume = value
                onPressedChanged: {
                    if (pressed)
                        value = value
                    else
                        value = Qt.binding(function () { return Player.volume })
                }
            }
        }
    }
}
