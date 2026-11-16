import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property string title: "Cemu"
    signal openFolders()

    GridView {
        id: grid

        // as many ~150px wide columns as fit, never fewer than two
        readonly property int columns: Math.max(2, Math.floor(width / 150))

        anchors.fill: parent
        anchors.margins: 4
        clip: true
        cellWidth: Math.floor(width / columns)
        cellHeight: cellWidth + 44
        model: cemu.games
        boundsBehavior: Flickable.DragAndOvershootBounds

        ScrollBar.vertical: ScrollBar {}

        delegate: ItemDelegate {
            id: cell

            required property string titleId
            required property string name
            required property string region
            required property string iconSource

            width: grid.cellWidth
            height: grid.cellHeight
            padding: 6
            onClicked: cemu.launch(cell.titleId)

            contentItem: ColumnLayout {
                spacing: 4

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: width
                    radius: 8
                    color: Qt.rgba(0.5, 0.5, 0.5, 0.18)
                    clip: true

                    Image {
                        id: iconImage
                        anchors.fill: parent
                        source: cell.iconSource
                        asynchronous: true
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: 256
                        sourceSize.height: 256
                        smooth: true
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: iconImage.status !== Image.Ready
                        text: cell.name.length > 0 ? cell.name.charAt(0).toUpperCase() : "?"
                        font.pixelSize: parent.width * 0.4
                        opacity: 0.5
                    }
                    Label {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 4
                        visible: cell.region.length > 0
                        text: cell.region
                        font.pixelSize: 10
                        color: "white"
                        padding: 3
                        background: Rectangle {
                            radius: 3
                            color: Qt.rgba(0, 0, 0, 0.55)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: cell.name
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                }
            }
        }
    }

    // empty state
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 360)
        spacing: 16
        visible: grid.count === 0

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: 16
            text: cemu.scanning ? qsTr("Searching for games...")
                                : (cemu.gamePaths.length === 0
                                   ? qsTr("No game folder is configured yet.")
                                   : qsTr("No games found in the configured folders."))
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: !cemu.scanning
            text: qsTr("Game folders")
            onClicked: page.openFolders()
        }
    }
}
