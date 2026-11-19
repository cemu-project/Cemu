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

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: gamesPage
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
