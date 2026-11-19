import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Controllers of player 1. Lists what SDL sees, assigns one to the emulated controller and tests the mapping.
// Changes are saved right away to controllerProfiles/controller0.xml (shared with the desktop interface).
Item {
    id: page

    property string title: qsTr("Controllers")

    readonly property int player: 0
    property var player0: ({"enabled": false, "type": "", "devices": []})
    property var devices: []
    property var pressed: []

    function reload() {
        player0 = cemu.inputPlayer(player)
        devices = cemu.availableInputDevices()
    }

    function isAssigned(uuid) {
        for (var i = 0; i < player0.devices.length; i++) {
            if (player0.devices[i].uuid === uuid)
                return true
        }
        return false
    }

    Component.onCompleted: reload()

    Connections {
        target: cemu
        function onInputChanged() { page.reload() }
    }

    // plugged / unplugged controllers show up without leaving the page
    Timer {
        interval: 1500
        running: page.visible
        repeat: true
        onTriggered: page.devices = cemu.availableInputDevices()
    }

    // live view of the held buttons
    Timer {
        interval: 60
        running: page.visible && page.player0.enabled
        repeat: true
        onTriggered: page.pressed = cemu.pressedInputButtons(page.player)
    }

    component SectionHeader: Label {
        Layout.fillWidth: true
        Layout.topMargin: 20
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        font.pixelSize: 14
        font.bold: true
        opacity: 0.7
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 0

            SectionHeader { text: qsTr("PLAYER 1") }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 6
                spacing: 4

                Label { text: qsTr("Emulated controller") }
                ComboBox {
                    id: typeCombo
                    Layout.fillWidth: true
                    model: [qsTr("Disabled")].concat(cemu.inputTypes())
                    currentIndex: {
                        if (!page.player0.enabled)
                            return 0
                        const i = cemu.inputTypes().indexOf(page.player0.type)
                        return i >= 0 ? i + 1 : 0
                    }
                    onActivated: function(index) {
                        cemu.setInputPlayerType(page.player, index === 0 ? "" : cemu.inputTypes()[index - 1])
                    }
                }
            }

            SectionHeader { text: qsTr("ASSIGNED CONTROLLER") }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 6
                visible: page.player0.devices.length === 0
                wrapMode: Text.Wrap
                opacity: 0.7
                text: qsTr("None. Pick one in the list below.")
            }

            Repeater {
                model: page.player0.devices

                delegate: ItemDelegate {
                    id: assignedRow
                    required property var modelData
                    Layout.fillWidth: true
                    contentItem: RowLayout {
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: assignedRow.modelData.name
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: (assignedRow.modelData.connected ? qsTr("Connected") : qsTr("Not connected"))
                                      + " · " + assignedRow.modelData.api
                                font.pixelSize: 12
                                opacity: 0.6
                            }
                        }
                        ToolButton {
                            text: "✕"
                            font.pixelSize: 18
                            onClicked: cemu.removeInputDevice(page.player, assignedRow.modelData.uuid)
                            Accessible.name: qsTr("Remove")
                        }
                    }
                }
            }

            Button {
                Layout.leftMargin: 16
                Layout.topMargin: 8
                visible: page.player0.devices.length > 0
                text: qsTr("Reset default mapping")
                onClicked: cemu.resetInputMapping(page.player)
            }

            SectionHeader { text: qsTr("DETECTED CONTROLLERS") }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 6
                visible: page.devices.length === 0
                wrapMode: Text.Wrap
                opacity: 0.7
                text: qsTr("No controller detected. Connect one (Bluetooth or USB), it will show up here.")
            }

            Repeater {
                model: page.devices

                delegate: ItemDelegate {
                    id: deviceRow
                    required property var modelData
                    readonly property bool assigned: page.isAssigned(modelData.uuid)
                    Layout.fillWidth: true
                    contentItem: RowLayout {
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: deviceRow.modelData.name
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: deviceRow.modelData.api
                                font.pixelSize: 12
                                opacity: 0.6
                            }
                        }
                        Button {
                            text: deviceRow.assigned ? qsTr("In use") : qsTr("Use")
                            enabled: !deviceRow.assigned
                            onClicked: cemu.assignInputDevice(page.player, deviceRow.modelData.uuid)
                        }
                    }
                }
            }

            SectionHeader {
                text: qsTr("TEST")
                visible: page.player0.enabled && page.player0.devices.length > 0
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 6
                visible: page.player0.enabled && page.player0.devices.length > 0
                wrapMode: Text.Wrap
                font.pixelSize: 16
                text: {
                    if (page.pressed.length === 0)
                        return qsTr("Press buttons on the controller...")
                    var parts = []
                    for (var i = 0; i < page.pressed.length; i++)
                        parts.push(page.pressed[i].name + "  ←  " + page.pressed[i].device)
                    return parts.join("\n")
                }
            }
        }
    }
}
