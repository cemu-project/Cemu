import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Global settings. Every change is written to settings.xml right away (shared with the desktop interface).
Item {
    id: page

    property string title: qsTr("Settings")

    // ---- reusable rows ----

    component SectionHeader: Label {
        Layout.fillWidth: true
        Layout.topMargin: 20
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        font.pixelSize: 14
        font.bold: true
        opacity: 0.7
    }

    component SwitchRow: SwitchDelegate {
        id: switchRow
        required property string key
        property string hint: ""

        Layout.fillWidth: true
        leftPadding: 16
        rightPadding: 16
        checked: cemu.setting(switchRow.key) !== 0
        onToggled: cemu.setSetting(switchRow.key, checked ? 1 : 0)

        contentItem: ColumnLayout {
            spacing: 2
            Label {
                Layout.fillWidth: true
                Layout.rightMargin: 56 // room for the switch indicator
                text: switchRow.text
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                Layout.rightMargin: 56
                visible: text.length > 0
                text: switchRow.hint
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.6
            }
        }
    }

    component ComboRow: ColumnLayout {
        id: comboRow
        required property string key
        required property string label
        required property var options
        // maps between the stored value and the index in options (identity by default)
        property var toIndex: function(v) { return v }
        property var fromIndex: function(i) { return i }

        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        spacing: 4

        Label {
            text: comboRow.label
        }
        ComboBox {
            Layout.fillWidth: true
            model: comboRow.options
            currentIndex: comboRow.toIndex(cemu.setting(comboRow.key))
            onActivated: function(index) { cemu.setSetting(comboRow.key, comboRow.fromIndex(index)) }
        }
    }

    component SliderRow: ColumnLayout {
        id: sliderRow
        required property string key
        required property string label
        property int from: 0
        property int to: 100
        property var format: function(v) { return v + " %" }

        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        spacing: 0

        RowLayout {
            Label {
                Layout.fillWidth: true
                text: sliderRow.label
            }
            Label {
                text: sliderRow.format(slider.value)
                opacity: 0.7
            }
        }
        Slider {
            id: slider
            Layout.fillWidth: true
            from: sliderRow.from
            to: sliderRow.to
            stepSize: 1
            snapMode: Slider.SnapAlways
            value: cemu.setting(sliderRow.key)
            // save once the finger is lifted, not for every intermediate value
            onPressedChanged: if (!pressed) cemu.setSetting(sliderRow.key, value)
        }
    }

    readonly property var positions: [qsTr("Disabled"), qsTr("Top left"), qsTr("Top center"), qsTr("Top right"),
                                      qsTr("Bottom left"), qsTr("Bottom center"), qsTr("Bottom right")]
    readonly property var textScales: ["50 %", "75 %", "100 %", "125 %", "150 %", "175 %", "200 %", "225 %", "250 %", "275 %", "300 %"]
    readonly property var filters: [qsTr("Bilinear"), qsTr("Bicubic"), qsTr("Hermite"), qsTr("Nearest neighbor")]

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.implicitHeight + 32
        clip: true
        boundsBehavior: Flickable.DragAndOvershootBounds

        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: flick.width
            spacing: 0

            // ---------------- general
            SectionHeader { text: qsTr("General") }

            ComboRow {
                key: "console_language"
                label: qsTr("Console language")
                options: [qsTr("Japanese"), qsTr("English"), qsTr("French"), qsTr("German"), qsTr("Italian"), qsTr("Spanish"),
                          qsTr("Chinese"), qsTr("Korean"), qsTr("Dutch"), qsTr("Portuguese"), qsTr("Russian"), qsTr("Taiwanese")]
            }
            SwitchRow {
                key: "disable_screensaver"
                text: qsTr("Keep the screen on while playing")
            }
            SwitchRow {
                key: "play_boot_sound"
                text: qsTr("Intro sound")
                hint: qsTr("Plays the boot sound while shaders are being compiled")
            }

            // ---------------- graphics
            SectionHeader { text: qsTr("Graphics") }

            ComboRow {
                key: "vsync"
                label: qsTr("VSync")
                options: [qsTr("Off"), qsTr("Double buffering"), qsTr("Triple buffering")]
            }
            SwitchRow {
                key: "async_compile"
                text: qsTr("Asynchronous shader compilation")
                hint: qsTr("Less stutter, but objects may be missing for a moment")
            }
            ComboRow {
                key: "fullscreen_scaling"
                label: qsTr("Scaling")
                options: [qsTr("Keep aspect ratio"), qsTr("Stretch")]
            }
            ComboRow {
                key: "upscale_filter"
                label: qsTr("Upscale filter")
                options: page.filters
            }
            ComboRow {
                key: "downscale_filter"
                label: qsTr("Downscale filter")
                options: page.filters
            }

            // ---------------- audio
            SectionHeader { text: qsTr("Audio") }

            ColumnLayout {
                id: deviceRow
                property var devices: cemu.audioDevices()

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 6
                Layout.bottomMargin: 6
                spacing: 4

                Label {
                    text: qsTr("Output device")
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: deviceRow.devices
                    textRole: "name"
                    valueRole: "id"
                    Component.onCompleted: {
                        var current = cemu.tvAudioDevice()
                        var idx = indexOfValue(current)
                        // a saved device that no longer exists is replaced by the default one at launch
                        currentIndex = idx >= 0 ? idx : (deviceRow.devices.length > 1 ? 1 : 0)
                    }
                    onActivated: cemu.setTvAudioDevice(currentValue)
                }
            }
            SliderRow {
                key: "tv_volume"
                label: qsTr("Volume")
            }
            ComboRow {
                key: "tv_channels"
                label: qsTr("Channels")
                options: [qsTr("Mono"), qsTr("Stereo"), qsTr("Surround")]
            }
            SliderRow {
                key: "audio_delay"
                label: qsTr("Latency")
                to: 23
                // one block = 12 ms
                format: function(v) { return (v * 12) + " ms" }
            }

            // ---------------- overlay
            SectionHeader { text: qsTr("Overlay") }

            ComboRow {
                key: "overlay_position"
                label: qsTr("Position")
                options: page.positions
            }
            ComboRow {
                key: "overlay_scale"
                label: qsTr("Text size")
                options: page.textScales
                toIndex: function(v) { return Math.round((v - 50) / 25) }
                fromIndex: function(i) { return i * 25 + 50 }
            }
            SwitchRow { key: "overlay_fps"; text: qsTr("FPS") }
            SwitchRow { key: "overlay_cpu"; text: qsTr("CPU usage") }
            SwitchRow { key: "overlay_ram"; text: qsTr("RAM usage") }
            SwitchRow { key: "overlay_vram"; text: qsTr("VRAM usage") }

            // ---------------- notifications
            SectionHeader { text: qsTr("Notifications") }

            ComboRow {
                key: "notification_position"
                label: qsTr("Position")
                options: page.positions
            }
            ComboRow {
                key: "notification_scale"
                label: qsTr("Text size")
                options: page.textScales
                toIndex: function(v) { return Math.round((v - 50) / 25) }
                fromIndex: function(i) { return i * 25 + 50 }
            }
            SwitchRow {
                key: "notification_shaders"
                text: qsTr("Shader compilation")
            }

            Label {
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.topMargin: 24
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.6
                text: qsTr("Controllers, accounts and the other options are still set up in the desktop interface (Cemu launcher). Both interfaces share the same settings.")
            }
        }
    }
}
