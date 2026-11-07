import QtQuick

// The enclosing native thumbnail retains its border, tags and touch handling.
Image {
    objectName: "wereadOriginalNativeCover"
    fillMode: Image.PreserveAspectCrop
    horizontalAlignment: Image.AlignHCenter
    verticalAlignment: Image.AlignVCenter
    clip: true
    asynchronous: true
    cache: true
    visible: status === Image.Ready
}
