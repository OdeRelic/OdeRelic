import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: homeScreen
    color: "#090A0F"
    
    signal openManager(string consoleType)
    
    property string searchQuery: ""

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
            
            TextField {
                id: searchInput
                objectName: "searchInput"
                placeholderText: qsTr("Search consoles...")
                implicitWidth: 200
                implicitHeight: 40
                color: "white"
                font.pixelSize: 14
                
                background: Rectangle {
                    color: "#1E2230"
                    radius: 8
                    border.color: parent.activeFocus ? "#FF4D4D" : "transparent"
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }
                
                onTextChanged: homeScreen.searchQuery = text.toLowerCase().trim()
            }
            
            ComboBox {
                id: langCombo
                objectName: "langCombo"
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
        
        Component {
            id: reusableConsoleCard
            Item {
                property bool isMatch: {
                    if (homeScreen.searchQuery === "") return true;
                    return model.name.toLowerCase().includes(homeScreen.searchQuery) ||
                           model.ode.toLowerCase().includes(homeScreen.searchQuery) ||
                           model.key.toLowerCase().includes(homeScreen.searchQuery);
                }
                width: isMatch ? 320 : 0
                height: isMatch ? 220 : 0
                visible: isMatch
                clip: true
                
                Rectangle {
                    id: cardRect
                    objectName: "card_" + model.key
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
        
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            
            ColumnLayout {
                width: parent.width
                spacing: 20
                
                property bool showSony: homeScreen.searchQuery === "" || "sony playstation 1 ps1 playstation 2 ps2 xstation opl".toLowerCase().includes(homeScreen.searchQuery)
                property bool showNintendo: homeScreen.searchQuery === "" || "nintendo gamecube gc swiss".toLowerCase().includes(homeScreen.searchQuery)
                property bool showSega: homeScreen.searchQuery === "" || "sega dreamcast dc gdemu saturn saroo".toLowerCase().includes(homeScreen.searchQuery)
                
                // --------- Sony ---------
                Text { visible: parent.showSony; text: "Sony"; color: "white"; font.pixelSize: 20; font.bold: true; font.family: "Inter"; font.letterSpacing: 2; Layout.topMargin: 10 }
                Flow {
                    visible: parent.showSony
                    Layout.fillWidth: true
                    spacing: 30
                    Repeater {
                        model: ListModel {
                            ListElement { name: "PlayStation 1"; ode: "XStation"; image: "qrc:/assets/consoles/ps1.png"; active: true; key: "ps1" }
                            ListElement { name: "PlayStation 2"; ode: "OPL"; image: "qrc:/assets/consoles/ps2.png"; active: true; key: "ps2" }
                        }
                        delegate: reusableConsoleCard
                    }
                }
                
                Rectangle { visible: parent.showSony && (parent.showNintendo || parent.showSega); height: 1; Layout.fillWidth: true; color: "#1E2230"; Layout.topMargin: 15; Layout.bottomMargin: 15; opacity: 0.5 }
                
                // --------- Nintendo ---------
                Text { visible: parent.showNintendo; text: "Nintendo"; color: "white"; font.pixelSize: 20; font.bold: true; font.family: "Inter"; font.letterSpacing: 2 }
                Flow {
                    visible: parent.showNintendo
                    Layout.fillWidth: true
                    spacing: 30
                    Repeater {
                        model: ListModel {
                            ListElement { name: "Gamecube"; ode: "Swiss"; image: "qrc:/assets/consoles/gamecube.png"; active: true; key: "gc" }
                        }
                        delegate: reusableConsoleCard
                    }
                }
                
                Rectangle { visible: parent.showNintendo && parent.showSega; height: 1; Layout.fillWidth: true; color: "#1E2230"; Layout.topMargin: 15; Layout.bottomMargin: 15; opacity: 0.5 }
                
                // --------- Sega ---------
                Text { visible: parent.showSega; text: "Sega"; color: "white"; font.pixelSize: 20; font.bold: true; font.family: "Inter"; font.letterSpacing: 2 }
                Flow {
                    visible: parent.showSega
                    Layout.fillWidth: true
                    spacing: 30
                    Repeater {
                        model: ListModel {
                            ListElement { name: "Dreamcast"; ode: "GDEMU"; image: "qrc:/assets/consoles/dreamcast.png"; active: false; key: "dc" }
                            ListElement { name: "Sega Saturn"; ode: "Saroo"; image: "qrc:/assets/consoles/saturn.png"; active: false; key: "saturn" }
                        }
                        delegate: reusableConsoleCard
                    }
                }
                
                Item { height: 40 } // Bottom spacer
            }
        }
    }
}
