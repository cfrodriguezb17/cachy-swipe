// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kwin as KWin
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as Components
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg

KWin.TabBoxSwitcher {
    id: switcher

    Instantiator {
        active: switcher.visible
        delegate: PlasmaCore.Dialog {
            visible: true
            location: PlasmaCore.Types.Floating
            flags: Qt.ToolTip | Qt.X11BypassWindowManagerHint
            x: switcher.screenGeometry.x + (switcher.screenGeometry.width - width) / 2
            y: switcher.screenGeometry.y + (switcher.screenGeometry.height - height) / 2

            mainItem: FocusScope {
                id: panel
                objectName: "cachy-swipe-grid-panel"
                focus: true
                readonly property int columns: Math.max(1, Math.min(4, grid.count))
                readonly property int rows: Math.ceil(grid.count / columns)
                readonly property real iconSize: Kirigami.Units.iconSizes.enormous
                readonly property real tileWidth: Math.max(1, Math.floor(Math.min(iconSize + Kirigami.Units.largeSpacing * 2, switcher.screenGeometry.width * 0.84 / columns)))
                readonly property real tileHeight: tileWidth
                readonly property real gridHeight: Math.min(Math.max(1, rows) * tileHeight, switcher.screenGeometry.height * 0.76 - selectedCaption.height - Kirigami.Units.smallSpacing * 2)
                width: columns * tileWidth
                height: gridHeight + selectedCaption.height + Kirigami.Units.smallSpacing * 2

                function rowOffset(index) {
                    if (index < 0) return 0;
                    const rowStart = Math.floor(index / columns) * columns;
                    const itemsInRow = Math.min(columns, grid.count - rowStart);
                    return (columns - itemsInRow) * tileWidth / 2;
                }

                GridView {
                    id: grid
                    objectName: "cachy-swipe-grid-view"
                    anchors { top: parent.top; left: parent.left; right: parent.right }
                    height: panel.gridHeight
                    clip: true
                    focus: true
                    model: switcher.model
                    currentIndex: switcher.currentIndex
                    cellWidth: panel.tileWidth
                    cellHeight: panel.tileHeight
                    flow: GridView.FlowLeftToRight
                    layoutDirection: Qt.LeftToRight
                    keyNavigationEnabled: false
                    highlightMoveDuration: 0
                    highlightFollowsCurrentItem: true
                    boundsBehavior: Flickable.StopAtBounds
                    highlight: KSvg.FrameSvgItem {
                        imagePath: "widgets/viewitem"
                        prefix: "hover"
                        transform: Translate { x: panel.rowOffset(grid.currentIndex) }
                    }

                    delegate: Kirigami.Icon {
                        id: tile
                        objectName: "cachy-swipe-grid-icon"
                        width: grid.cellWidth
                        height: grid.cellHeight
                        transform: Translate { x: panel.rowOffset(index) }
                        property string caption: model.caption
                        Accessible.name: model.caption
                        Accessible.role: Accessible.ListItem
                        source: model.icon
                        fallback: "application-x-executable"
                        active: GridView.isCurrentItem
                        TapHandler {
                            onTapped: switcher.model.activate(index)
                        }
                    }
                    onCurrentIndexChanged: {
                        switcher.currentIndex = currentIndex;
                        positionViewAtIndex(currentIndex, GridView.Contain);
                    }
                }

                Components.Label {
                    id: selectedCaption
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: implicitHeight + Kirigami.Units.smallSpacing
                    text: grid.currentItem ? grid.currentItem.caption : ""
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                    font.weight: Font.Bold
                }

                Keys.onPressed: function(event) {
                    const row = Math.floor(grid.currentIndex / panel.columns);
                    const col = grid.currentIndex % panel.columns;
                    let target = grid.currentIndex;
                    if (event.key === Qt.Key_Left)
                        target = Math.max(row * panel.columns, target - 1);
                    else if (event.key === Qt.Key_Right)
                        target = Math.min(grid.count - 1, row * panel.columns + panel.columns - 1, target + 1);
                    else if (event.key === Qt.Key_Up && row > 0)
                        target -= panel.columns;
                    else if (event.key === Qt.Key_Down && row + 1 < panel.rows)
                        target = Math.min(grid.count - 1, target + panel.columns);
                    else {
                        event.accepted = false;
                        return;
                    }
                    grid.currentIndex = target;
                    event.accepted = true;
                }
            }
        }
    }
}
