import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: page

    property string title: qsTr("Game folders")

    FolderDialog {
        id: folderDialog
        title: qsTr("Select a folder containing games")
        onAccepted: cemu.addGamePath(selectedFolder)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: cemu.gamePaths

            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row

                required property string modelData

                width: list.width
                contentItem: RowLayout {
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData
                        wrapMode: Text.WrapAnywhere
                        maximumLineCount: 3
                        elide: Text.ElideMiddle
                    }
                    ToolButton {
                        text: "✕"
                        font.pixelSize: 18
                        onClicked: cemu.removeGamePath(row.modelData)
                        Accessible.name: qsTr("Remove")
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                opacity: 0.7
                text: qsTr("Add the folders where your Wii U games are stored.")
            }
        }

        ToolSeparator {
            Layout.fillWidth: true
            orientation: Qt.Horizontal
        }

        // typing a path is a fallback for systems where the folder picker is awkward to use
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 8

            TextField {
                id: pathField
                Layout.fillWidth: true
                placeholderText: qsTr("/path/to/games")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onAccepted: addButton.clicked()
            }
            Button {
                id: addButton
                text: qsTr("Add")
                enabled: pathField.text.trim().length > 0
                onClicked: {
                    cemu.addLocalGamePath(pathField.text)
                    pathField.clear()
                }
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.margins: 12
            text: qsTr("Browse...")
            onClicked: folderDialog.open()
        }

        Label {
            Layout.fillWidth: true
            Layout.bottomMargin: 8
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 11
            opacity: 0.6
            text: cemu.version
        }
    }
}
