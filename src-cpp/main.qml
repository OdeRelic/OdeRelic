import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: mainWindow
    width: 1200
    height: 800
    visible: true
    title: qsTr("OdeRelic (C++ Native)")
    color: "#090A0F"
    
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: HomeScreen {
            onOpenManager: function(consoleType) {
                if (consoleType === "ps2") {
                    stackView.push(oplManagerComponent)
                } else if (consoleType === "gc") {
                    stackView.push(swissManagerComponent)
                }
            }
        }
    }
    
    Component {
        id: oplManagerComponent
        OplManager {
            onRequestBack: stackView.pop()
        }
    }
    
    Component {
        id: swissManagerComponent
        SwissManager {
            onRequestBack: stackView.pop()
        }
    }
}
