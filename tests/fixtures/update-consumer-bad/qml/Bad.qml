// Gegenprobe für tests/CheckUpdateConsumer.cmake. Wird nie gebaut.
import QtQuick
import QtCore
Item {
    Settings { category: "update"; property bool autoCheck: true }
    visible: Updates.flow.state === 3
}
