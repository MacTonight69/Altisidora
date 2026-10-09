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
        font.family: Theme.fontFamily
        property string iconText: ""
        property bool primary: false
        property bool circular: false
        // Toggle-style "on" state (e.g. repeat-one): tints the icon and
        // fills a soft highlight so the active mode is obvious at a glance.
        property bool active: false
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        padding: 8

        contentItem: Text {
            text: tButton.iconText
            color: tButton.primary ? Theme.accentText
                 : tButton.active ? Theme.accent : Theme.textPrimary
            font.pixelSize: (tButton.primary ? 17 : 14) * Theme.fontScale
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
                   : tButton.active
                     ? (tButton.down ? Theme.borderPressed : tButton.hovered ? Theme.border : Theme.selection)
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
        font.family: Theme.fontFamily
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

            property color fillColor: Theme.textPrimary
            property bool isPause: ppButton.playing
            onFillColorChanged: requestPaint()
            onIsPauseChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = fillColor
                // Optical nudge: the drawn icons sit slightly right-heavy
                // inside the circle, move both states a couple of px left.
                var ox = -2
                if (isPause) {
                    // two symmetric bars centred on (15,15) + ox
                    ctx.fillRect(7 + ox, 8, 5, 14)
                    ctx.fillRect(18 + ox, 8, 5, 14)
                } else {
                    // right-pointing triangle, centroid at (15,15) + ox
                    ctx.beginPath()
                    ctx.moveTo(11 + ox, 8)
                    ctx.lineTo(11 + ox, 22)
                    ctx.lineTo(24 + ox, 15)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }

        background: Rectangle {
            radius: height / 2
            color: ppButton.down ? Theme.borderPressed
                   : ppButton.hovered ? Theme.border : "transparent"
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
            font.family: Theme.fontFamily
            font.pixelSize: 12 * Theme.fontScale
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
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: controls.songArtist !== ""
                text: controls.songArtist
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
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

                // Repeat-one: loops the current song instead of advancing.
                TransportButton {
                    iconText: "\u21BB" // ↻
                    active: Player.repeatOne
                    onClicked: Player.repeatOne = !Player.repeatOne
                    ToolTip.visible: hovered
                    ToolTip.text: Player.repeatOne ? qsTr("Repeat one — on") : qsTr("Repeat one")
                    ToolTip.delay: 400
                }
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
                    font.family: Theme.fontFamily
                    font.pixelSize: 11 * Theme.fontScale
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
                    font.family: Theme.fontFamily
                    font.pixelSize: 11 * Theme.fontScale
                }
            }
        }

        // ---- volume ----
        RowLayout {
            spacing: 8

            TransportButton {
                iconText: "EQ"
                onClicked: eqPopup.open()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Ecualizador")
                ToolTip.delay: 400
            }

            Text {
                Layout.preferredWidth: 34
                horizontalAlignment: Text.AlignRight
                text: Math.round(Player.volume * 100) + "%"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
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

    // ---- ecualizador (10 bandas vía Player eq API) ----
    Popup {
        id: eqPopup
        popupType: Popup.Item
        parent: controls
        x: controls.width - width - 16
        // The popup lives inside the bottom bar; y is relative to it, so a
        // positive offset pushed the panel below the window. Grow upward
        // instead so the whole panel stays visible above the transport bar.
        y: -(height + 14)
        width: 520
        padding: 14
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        background: Rectangle {
            color: Theme.bgAlt
            border.color: Theme.border
            radius: 10
        }

        ColumnLayout {
            width: parent.width
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: qsTr("Ecualizador")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 14 * Theme.fontScale
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: Player.eqEnabled ? qsTr("Activo") : qsTr("Desactivado")
                    color: Player.eqEnabled ? Theme.success : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 11 * Theme.fontScale
                }

                Switch {
                    font.family: Theme.fontFamily
                    checked: Player.eqEnabled
                    onToggled: Player.eqEnabled = checked
                }

                TransportButton {
                    iconText: "\u2715"
                    onClicked: eqPopup.close()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Ocultar ecualizador")
                    ToolTip.delay: 400
                }
            }

            Row {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                spacing: 6

                Repeater {
                    model: 10

                    Column {
                        width: 42
                        spacing: 2

                        Slider {
                            id: bandSlider
                            width: 26
                            height: 120
                            anchors.horizontalCenter: parent.horizontalCenter
                            orientation: Qt.Vertical
                            focusPolicy: Qt.NoFocus
                            from: -12
                            to: 12
                            stepSize: 0.5
                            // Bind to the eqGains property (NOTIFY eqChanged)
                            // instead of the eqGain() method: QML tracks
                            // property dependencies only, so a method call
                            // would never re-evaluate when a preset applies
                            // and the sliders would keep stale values.
                            value: Player.eqGains[index]
                            onMoved: Player.setEqGain(index, value)
                            onPressedChanged: {
                                if (pressed)
                                    value = value
                                else
                                    value = Qt.binding(function () { return Player.eqGains[index] })
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: ["31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"][index]
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 9 * Theme.fontScale
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("Preset:")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 11 * Theme.fontScale
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    spacing: 6

                    Repeater {
                        model: Player.eqPresets

                        Row {
                            spacing: 2

                            Button {
                                font.family: Theme.fontFamily
                                text: modelData
                                checkable: true
                                checked: Player.eqPreset === modelData
                                onClicked: Player.eqPreset = modelData
                                font.pixelSize: 11 * Theme.fontScale
                                padding: 6

                                background: Rectangle {
                                    radius: 5
                                    color: parent.checked ? Theme.accent
                                         : (parent.hovered ? Theme.rowHover : "transparent")
                                    border.color: Theme.border
                                }

                                contentItem: Text {
                                    text: parent.text
                                    color: parent.checked ? Theme.accentText : Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11 * Theme.fontScale
                                }
                            }

                            Button {
                                font.family: Theme.fontFamily
                                visible: !Player.isBuiltInEqPreset(modelData)
                                text: "\u2715"
                                implicitWidth: 20
                                implicitHeight: 22
                                focusPolicy: Qt.NoFocus
                                onClicked: Player.deleteEqPreset(modelData)
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Eliminar preset")
                                ToolTip.delay: 400

                                background: Rectangle {
                                    radius: 4
                                    color: parent.hovered ? Theme.dangerBg : "transparent"
                                    border.color: parent.hovered ? Theme.danger : "transparent"
                                }

                                contentItem: Text {
                                    text: parent.text
                                    color: parent.hovered ? Theme.danger : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10 * Theme.fontScale
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    font.family: Theme.fontFamily
                    text: qsTr("Reset")
                    font.pixelSize: 11 * Theme.fontScale
                    padding: 6

                    background: Rectangle {
                        radius: 5
                        color: Theme.rowHover
                        border.color: Theme.border
                    }

                    contentItem: Text {
                        text: parent.text
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11 * Theme.fontScale
                    }

                    onClicked: Player.resetEq()
                }
            }

            // ---- guardar una curva como preset de usuario ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                TextField {
                    font.family: Theme.fontFamily
                    id: eqPresetNameField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: qsTr("Nombre del nuevo preset")
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textPrimary
                    selectionColor: Theme.fieldSelection
                    selectedTextColor: Theme.fieldSelectedText
                    font.pixelSize: 11 * Theme.fontScale
                    leftPadding: 10
                    rightPadding: 10
                    background: Rectangle {
                        radius: 5
                        color: Theme.field
                        border.width: 1
                        border.color: eqPresetNameField.activeFocus ? Theme.accent : Theme.border
                    }
                    onAccepted: saveEqPresetButton.clicked()
                }

                Button {
                    font.family: Theme.fontFamily
                    id: saveEqPresetButton
                    Layout.preferredHeight: 32
                    focusPolicy: Qt.NoFocus
                    text: qsTr("Guardar")
                    font.pixelSize: 11 * Theme.fontScale
                    padding: 6
                    enabled: eqPresetNameField.text.trim().length > 0

                    background: Rectangle {
                        radius: 5
                        color: parent.enabled
                               ? (parent.down ? Theme.accentPressed
                                  : parent.hovered ? Theme.accentHover : Theme.accent)
                               : Theme.rowHover
                        border.color: Theme.border
                    }

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? Theme.accentText : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 11 * Theme.fontScale
                    }

                    onClicked: {
                        if (Player.saveEqPreset(eqPresetNameField.text))
                            eqPresetNameField.text = ""
                    }
                }
            }
        }
    }
}
