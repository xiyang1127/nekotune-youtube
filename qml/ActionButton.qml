import QtQuick
import QtQuick.Controls

Button {
    id: control
    property var paletteTokens: ({})
    property bool quiet: false
    property int textAlignment: Text.AlignHCenter
    readonly property color ink: paletteTokens.textPrimary || "#f2f2f4"
    readonly property color accent: paletteTokens.accentPrimary || "#e9424f"
    padding: 10
    horizontalPadding: 13
    implicitHeight: 38
    implicitWidth: Math.max(64, contentItem.implicitWidth + leftPadding + rightPadding)
    opacity: enabled ? 1 : 0.45
    font.pixelSize: 13
    contentItem: Text {
        text: control.text; font: control.font; textFormat: Text.PlainText
        color: control.highlighted ? "white" : control.ink
        horizontalAlignment: control.textAlignment
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 8
        color: control.highlighted ? control.accent : control.down || control.hovered
            ? (control.paletteTokens.bgHover || "#30323a") : control.quiet ? "transparent"
            : (control.paletteTokens.bgRaised || "#24262d")
        border.width: control.visualFocus ? 2 : 0
        border.color: control.accent
    }
}
