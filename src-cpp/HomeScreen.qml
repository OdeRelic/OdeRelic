import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: homeScreen
    color: "#090A0F"
    
    signal openManager(string consoleType)
    
    // Aesthetic properties
    property color cardBorderNormal: "#2A2F40"
    property color cardBorderHover: "#FF4D4D"
    
    // Header
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 30
        
        RowLayout {
            spacing: 20
            Layout.fillWidth: true
            
            Item {
                width: 60; height: 60
                
                Image {
                    id: mainLogo
                    anchors.fill: parent
                    source: "qrc:/app_icon.png"
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }
                
                MultiEffect {
                    source: mainLogo
                    anchors.fill: mainLogo
                    shadowEnabled: true
                    shadowColor: "#FF4D4D"
                    shadowOpacity: 0.3
                    shadowBlur: 2.0
                }
            }
            
            ColumnLayout {
                spacing: 6
                Text {
                    text: qsTr("OdeRelic")
                    color: "white"
                    font.pixelSize: 34
                    font.bold: true
                    font.family: "Inter"
                    font.letterSpacing: 1
                }
                
                Text {
                    text: qsTr("Select a console environment below to bridge and manage its Optical Drive storage.")
                    color: "#8B95A5"
                    font.pixelSize: 15
                }
            }
            
            Item { Layout.fillWidth: true } // Spatial wedge
            
            ComboBox {
                id: langCombo
                implicitWidth: 140
                implicitHeight: 40
                
                model: [
                    { text: "English", code: "en" },
                    { text: "Português", code: "pt_BR" },
                    { text: "Deutsch", code: "de" },
                    { text: "日本語", code: "ja" }
                ]
                textRole: "text"
                valueRole: "code"
                
                background: Rectangle {
                    color: "#1E2230"
                    radius: 8
                    border.color: parent.hovered ? "#FF4D4D" : "transparent"
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }
                contentItem: Text {
                    text: parent.currentText
                    color: "white"
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 16
                }
                indicator: Text {
                    text: "▼"
                    color: "#8B95A5"
                    font.pixelSize: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
                
                Component.onCompleted: {
                    let code = translationManager.currentLanguage;
                    for (let i = 0; i < count; i++) {
                        if (model[i].code === code) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                
                onActivated: {
                    translationManager.setLanguage(currentValue)
                }
            }
        }
        
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#1E2230"
        }
        
        Item { height: 10 }
        
        GridView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: 350
            cellHeight: 250
            clip: true
            
            model: ListModel {
                ListElement { name: "PlayStation 2"; ode: "OPL"; image: "qrc:/assets/consoles/ps2.png"; active: true; key: "ps2" }
                ListElement { name: "Gamecube"; ode: "Swiss"; image: "qrc:/assets/consoles/gamecube.png"; active: false; key: "gc" }
                ListElement { name: "Dreamcast"; ode: "GDEMU"; image: "qrc:/assets/consoles/dreamcast.png"; active: false; key: "dc" }
                ListElement { name: "Sega Saturn"; ode: "Saroo"; image: "qrc:/assets/consoles/saturn.png"; active: false; key: "saturn" }
                ListElement { name: "PlayStation 1"; ode: "PSIO"; image: "qrc:/assets/consoles/ps1.png"; active: false; key: "ps1" }
            }
            
            delegate: Item {
                width: 320
                height: 220
                
                Rectangle {
                    id: cardRect
                    anchors.fill: parent
                    color: "#181A20"
                    radius: 16
                    border.color: mouseArea.containsMouse ? (model.active ? cardBorderHover : "#88FFFFFF") : cardBorderNormal
                    border.width: mouseArea.containsMouse ? 2 : 1
                    clip: true
                    
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                    
                    Image {
                        anchors.fill: parent
                        source: model.image
                        fillMode: Image.PreserveAspectCrop
                        opacity: model.active ? (mouseArea.containsMouse ? 1.0 : 0.8) : 0.4
                        Behavior on opacity { NumberAnimation { duration: 200 } }
                    }
                    
                    // Gradient overlay to make text readable
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.6; color: "#44000000" }
                            GradientStop { position: 1.0; color: "#CC000000" }
                        }
                    }
                    
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 20
                        spacing: 2
                        
                        Text {
                            text: qsTr(model.name)
                            color: "white"
                            font.bold: true
                            font.pixelSize: 22
                        }
                        
                        RowLayout {
                            spacing: 8
                            Text {
                                text: qsTr(model.ode)
                                color: "#FF4D4D"
                                font.bold: true
                                font.pixelSize: 14
                            }
                            Text {
                                text: model.active ? "" : qsTr("(Coming Soon)")
                                color: "#8B95A5"
                                font.pixelSize: 12
                                visible: !model.active
                            }
                        }
                    }
                    
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: model.active ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (model.active) {
                                homeScreen.openManager(model.key)
                            }
                        }
                    }
                }
            }
        }
    }
}
