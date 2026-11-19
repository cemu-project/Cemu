import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root

    // the list is hidden while a game runs in its own full screen window
    visible: !cemu.gameRunning
    width: 420
    height: 760
    title: "Cemu"

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 0

            ToolButton {
                visible: stack.depth > 1
                text: "‹"
                font.pixelSize: 28
                onClicked: stack.pop()
                Accessible.name: qsTr("Back")
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: stack.depth > 1 ? 0 : 16
                text: stack.currentItem && stack.currentItem.title ? stack.currentItem.title : "Cemu"
                font.pixelSize: 20
                font.bold: true
                elide: Text.ElideRight
            }
            BusyIndicator {
                visible: cemu.scanning
                running: visible
                implicitWidth: 32
                implicitHeight: 32
            }
            ToolButton {
                visible: stack.depth === 1
                text: "↻"
                font.pixelSize: 22
                enabled: !cemu.scanning
                onClicked: cemu.refresh()
                Accessible.name: qsTr("Refresh game list")
            }
            ToolButton {
                visible: stack.depth === 1
                text: "⋮"
                font.pixelSize: 22
                onClicked: mainMenu.open()
                Accessible.name: qsTr("Menu")

                Menu {
                    id: mainMenu
                    y: parent.height
                    MenuItem {
                        text: qsTr("Game folders")
                        onTriggered: stack.push(foldersPage)
                    }
                    MenuItem {
                        text: qsTr("Controllers")
                        onTriggered: stack.push(controllersPage)
                    }
                    MenuItem {
                        text: qsTr("Settings")
                        onTriggered: stack.push(settingsPage)
                    }
                    MenuItem {
                        text: qsTr("Quit")
                        onTriggered: Qt.quit()
                    }
                }
            }
        }
    }

    // moves the keyboard focus to the next/previous control (used by the gamepad D-pad)
    function moveFocus(forward) {
        const current = root.activeFocusItem
        if (!current)
            return
        const next = current.nextItemInFocusChain(forward)
        if (next && next !== current)
            next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: gamesPage
        focus: true

        // arrows / D-pad: lists, grids, text fields and the left/right of sliders keep their own behaviour,
        // everything else moves the focus from one control to the next
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: function(event) {
            const k = event.key
            const horizontal = k === Qt.Key_Left || k === Qt.Key_Right
            if (!horizontal && k !== Qt.Key_Up && k !== Qt.Key_Down)
                return
            const item = root.activeFocusItem
            if (item) {
                if (item.keyNavigationEnabled !== undefined) // ListView, GridView
                    return
                if (item.cursorPosition !== undefined) // text fields
                    return
                if (horizontal && item.stepSize !== undefined) // sliders, spin boxes
                    return
            }
            root.moveFocus(k === Qt.Key_Down || k === Qt.Key_Right)
            event.accepted = true
        }
        // B / circle: back (popups close by themselves on Escape)
        Keys.onEscapePressed: {
            if (stack.depth > 1)
                stack.pop()
        }
    }

    Component {
        id: gamesPage
        GamesPage {
            onOpenFolders: stack.push(foldersPage)
            onOpenGraphicPacks: function(titleId, name) {
                stack.push(graphicPacksPage, {"titleId": titleId, "gameName": name})
            }
        }
    }

    Component {
        id: foldersPage
        FoldersPage {}
    }

    Component {
        id: controllersPage
        ControllersPage {}
    }

    Component {
        id: settingsPage
        SettingsPage {}
    }

    Component {
        id: graphicPacksPage
        GraphicPacksPage {}
    }

    // shown while the game window is being created and the title is mounted
    Rectangle {
        anchors.fill: parent
        visible: cemu.launching
        color: Qt.rgba(0, 0, 0, 0.6)
        z: 10

        MouseArea {
            anchors.fill: parent // swallow touches
        }
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 12
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: cemu.launching
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Loading...")
                color: "white"
            }
        }
    }

    Dialog {
        id: errorDialog
        property alias message: errorLabel.text

        title: qsTr("Error")
        modal: true
        standardButtons: Dialog.Ok
        anchors.centerIn: parent
        width: Math.min(root.width - 32, 480)

        Label {
            id: errorLabel
            width: errorDialog.availableWidth
            wrapMode: Text.Wrap
        }
    }

    Connections {
        target: cemu
        function onErrorOccurred(message) {
            errorDialog.message = message
            errorDialog.open()
        }
    }
}
