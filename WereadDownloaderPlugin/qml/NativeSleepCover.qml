import QtQuick

// Only the opaque native sleep window hosts this item. No power/timer hooks.
Rectangle {
    objectName: "wereadNativeSleepCover"
    property url coverSource: ""
    readonly property bool coverReady: cover.status === Image.Ready
    color: "white"
    visible: coverReady
    Image {
        id: cover
        anchors.fill: parent
        source: parent.coverSource
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        cache: true
        // Local image: load before the system captures the sleep framebuffer.
        asynchronous: false
    }
}
