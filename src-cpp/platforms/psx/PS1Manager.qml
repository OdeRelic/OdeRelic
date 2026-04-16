import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Effects
import Qt.labs.folderlistmodel

Rectangle {
    id: mainWindow
    signal requestBack()
    
    // Modern Dark Theme Colors & Transparent Glass Aesthetics
    property color bgMain: "#D9090A0F" // Highly translucent dark slate
    property color bgSidebar: "#4D131620" // Frosted sidebar tint
    property color bgCard: "transparent" // Fully transparent cards matching mock
    property color cardBorderNormal: "#88FF4D4D" // Soft red border
    property color cardBorderHover: "#FFFF4D4D" // Hard glowing red border
    property color accentPrimary: "#FF7A00" // Classic Gamecube Spice Orange
    property color accentHover: "#FF9233"

    property color textPrimary: "#FFFFFF"
    property color textSecondary: "#8B95A5"
    property color borderGlass: "#2A2F40"
    
    color: bgMain

    function extractGameId(filename) {
        let match = filename.match(/(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|SCAJ|SLKA|SCKA|SCED|SCCS)[_-]?\d{3}\.?\d{2}/i);
        if (!match) return "";
        let str = match[0].toUpperCase();
        str = str.replace("-", "_");
        if (str.length === 10 && str[4] === '_' && str[8] !== '.') {
            str = str.slice(0, 8) + "." + str.slice(8);
        }
        return str;
    }

    property string currentLibraryPath: ""
    property var gameFiles: []
    property var libraryGames: {
        let q = searchQuery.toLowerCase()
        return gameFiles.filter(function(g) { 
            if (!g.isRenamed) return false;
            if (q === "") return true;
            return g.name.toLowerCase().indexOf(q) !== -1; 
        })
    }
    
    property var importGames: {
        let q = searchQuery.toLowerCase()
        return gameFiles.filter(function(g) { 
            if (g.isRenamed) return false;
            if (q === "") return true;
            return g.name.toLowerCase().indexOf(q) !== -1; 
        })
    }
    
    property int currentTabIndex: 0
    property var selectionMap: ({})
    property var librarySelectionMap: ({})
    property string searchQuery: ""
    
    property bool isGridView: true
    
    property int scanCurrentFolder: 0
    property int scanTotalFolders: 0
    
    property int libraryScanCurrent: 0
    property int libraryScanTotal: 0
    property string swissSetupStatusStr: "Initializing..."
    property int swissSetupPercent: 0
    property bool swissUpdateAvailable: false
    property string swissLocalVersion: ""
    property string swissRemoteVersion: ""
    property string savedOdeType: "Standalone"
    
    property string sortCriteria: "name"
    property bool sortAscending: true
    
    property bool isScrapingIO: false

    // ── PS1 batch state ───────────────────────────────────────────────────────
    property var batchConvertingMap: ({})
    property var batchConvertingNames: ({})
    property int batchActiveJobs: 0
    property int batchMaxJobs: 1
    property bool isBatchExtracting: false
    property var extractQueue: []
    property int extractIndex: 0

    property double targetDriveFreeSpace: 0
    property double targetDriveTotalSpace: 0

    function updateStorageBars() {
        if (mainWindow.currentLibraryPath !== "") {
            let spaceObj = systemUtils.getStorageSpace(mainWindow.currentLibraryPath)
            targetDriveFreeSpace = spaceObj.free
            targetDriveTotalSpace = spaceObj.total
        }
    }
    
    onCurrentLibraryPathChanged: updateStorageBars()

    property int activeTargetPercent: 0
    property double activeTargetMBps: 0.0
    Component.onCompleted: {
        if (mainWindow.currentLibraryPath === "") {
            targetSetupDialog.open()
        }
    }


    Connections {
        target: ps1XstationLibraryService
        function onImportIsoProgress(sourcePath, percent, MBps) {
            mainWindow.activeTargetPercent = percent
            mainWindow.activeTargetMBps = MBps
        }
        function onArtDownloadProgress(sourcePath, percent) {}
        
        function onGamesFilesLoaded(dirPath, data) {
            if (dirPath === mainWindow.currentLibraryPath) {
                isBatchExtracting = false
                isScrapingIO = false
                mainWindow.gameFiles = []
                mainWindow.gameFiles = data.data
                mainWindow.librarySelectionMap = ({})
                mainWindow.selectionMap = ({})
                updateSort()
            }
        }
        
        function onArtDownloadFinished(sourcePath, success, message) {
            if (sourcePath.toString().startsWith("FETCH_")) {
                fetchArtBtn.activeJobs--
                if (fetchArtBtn.currIndex >= mainWindow.libraryGames.length && fetchArtBtn.activeJobs <= 0) {
                    fetchArtBtn.fetching = false
                    Qt.callLater(refreshGames)
                }
                return;
            }
            if (mainWindow.batchConvertingMap[sourcePath] === true) {
                let newMap = Object.assign({}, mainWindow.batchConvertingMap)
                delete newMap[sourcePath]
                mainWindow.batchConvertingMap = newMap
                
                mainWindow.batchActiveJobs--
                extractProcessor()
            }
        }
        
        function onImportIsoFinished(sourcePath, success, destIsoPath, message) {
            if (mainWindow.batchConvertingMap[sourcePath] === true) {
                if (success) {
                    let extractedId = extractGameId(mainWindow.batchConvertingNames[sourcePath]);
                    if (extractedId === "") {
                        let hexScan = ps1XstationLibraryService.tryDetermineGameIdFromHex(sourcePath);
                        if (hexScan.success) extractedId = hexScan.gameId;
                    }
                    if (extractedId !== "") {
                        let artFolder = destIsoPath
                        ps1XstationLibraryService.startDownloadArtAsync(artFolder, extractedId, sourcePath)
                        return; // Await async net dispatch
                    } else {
                        console.log("No valid PS1 GameID found in filename string intuitively naturally safely.");
                    }
                } else {
                    console.error("Batch ISO Import Failed: " + message)
                    ps1XstationLibraryService.cancelAllImports()
                    isBatchExtracting = false
                    extractQueue = []
                    mainWindow.librarySelectionMap = ({})
                    mainWindow.selectionMap = ({})
                    Qt.callLater(refreshGames)
                    
                    formatErrorPopup.errorDetails = message
                    formatErrorPopup.open()
                }
                
                // Fallback completion if network fails to dispatch
                let newMap = Object.assign({}, mainWindow.batchConvertingMap)
                delete newMap[sourcePath]
                mainWindow.batchConvertingMap = newMap
                mainWindow.batchActiveJobs--
                if (isBatchExtracting) extractProcessor()
            }
        }
        
        function onExternalFilesScanProgress(currentFolder, totalFolders) {
            mainWindow.scanCurrentFolder = currentFolder
            mainWindow.scanTotalFolders = totalFolders
        }
        
        function onLibraryScanProgress(current, total) {
            mainWindow.libraryScanCurrent = current
            mainWindow.libraryScanTotal = total
        }
        
        function onExternalFilesScanFinished(isPs1, files) {
            folderScanOverlay.close()
            let arr = [].concat(mainWindow.gameFiles)
            for (let i = 0; i < files.length; i++) {
                let exists = false;
                for (let j = 0; j < arr.length; j++) { if (arr[j].path === files[i].path) { exists = true; break; } }
                if (!exists) arr.push(files[i]);
            }
            mainWindow.gameFiles = arr
            updateSort()
            mainWindow.updateStorageBars()
        }
        
        function onSetupXStationProgress(percent, statusText) {
            mainWindow.swissSetupStatusStr = statusText
            mainWindow.swissSetupPercent = percent
        }
        
        function onSetupXStationFinished(success, message) {
            swissSetupProgressOverlay.close()
            mainWindow.isScrapingIO = true
            refreshGames()
            swissUpdateAvailable = false // Clear banner after setup
            currentTabIndex = 0
            console.log("XStation Setup result:", success, message)
        }
        
        function onXstationUpdateCheckFinished(updateAvailable, localVersion, remoteVersion, savedOde) {
            mainWindow.swissUpdateAvailable = updateAvailable
            mainWindow.swissLocalVersion = localVersion
            mainWindow.swissRemoteVersion = remoteVersion
            mainWindow.savedOdeType = savedOde
            
            if (updateAvailable) console.log("XStation Update Available:", remoteVersion)
        }
        

    }

    
    Popup {
        id: targetSetupDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 380
        height: 220
        modal: true
        closePolicy: Popup.NoAutoClose
        
        background: Rectangle {
            color: "#EA0A1929"
            radius: 12
            border.color: borderGlass
            border.width: 1
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15
            Text {
                text: qsTr("Setup PS1 ODE")
                color: accentPrimary
                font.bold: true
                font.pixelSize: 18
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: qsTr("Please select the root directory of your SD Card or USB drive to mount the PS1 ecosystem natively.")
                color: textSecondary
                font.pixelSize: 14
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Button {
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    onClicked: {
                        targetSetupDialog.close()
                        requestBack()
                    }
                    contentItem: Text {
                        text: parent.text; color: textPrimary; font.bold: true; font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.down ? "#111" : (parent.hovered ? borderGlass : "transparent")
                        radius: 6; implicitHeight: 36; border.color: borderGlass; border.width: 1
                    }
                }
                Button {
                    text: qsTr("Select Root Media")
                    Layout.fillWidth: true
                    onClicked: {
                        targetSetupDialog.close()
                        folderDialog.open()
                    }
                    contentItem: Text {
                        text: parent.text; color: "#090A0F"; font.bold: true; font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.down ? accentHover : accentPrimary
                        radius: 6; implicitHeight: 36
                    }
                }
            }
        }
    }

    Popup {
        id: folderScanOverlay
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 300
        height: 140
        modal: true
        closePolicy: Popup.NoAutoClose
        onOpened: {
            mainWindow.scanCurrentFolder = 0
            mainWindow.scanTotalFolders = 0
        }
        background: Rectangle {
            color: "#EA0A1929"
            radius: 12
            border.color: borderGlass
            border.width: 1
        }
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 12
            Text {
                text: qsTr("Loading Game Library...")
                color: "#18A0FB"
                font.bold: true
                font.pixelSize: 15
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: mainWindow.scanTotalFolders > 0 ? (mainWindow.scanCurrentFolder + " / " + mainWindow.scanTotalFolders + " Folders") : "Locating targets..."
                color: textSecondary
                font.pixelSize: 12
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                width: 160; height: 5; radius: 2; color: borderGlass
                Layout.alignment: Qt.AlignHCenter
                Rectangle {
                    width: 50; height: parent.height; radius: 2; color: "#18A0FB"
                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation { from: 0; to: 110; duration: 600; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 110; to: 0; duration: 600; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }
    }

    Popup {
        id: importProgressOverlay
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 320
        height: 180
        modal: true
        closePolicy: Popup.NoAutoClose
        
        property bool isExtracting: mainWindow.isBatchExtracting
        onIsExtractingChanged: {
            if (isExtracting) open(); else close();
        }

        property int currentIndex: mainWindow.extractIndex
        property int totalItems: mainWindow.extractQueue.length
        property string actionName: "Importing PS1 games..."
        property color activeColor: accentPrimary
        
        onOpened: {
            mainWindow.activeTargetPercent = 0
            mainWindow.activeTargetMBps = 0.0
        }

        background: Rectangle {
            color: "#EA0A1929"
            radius: 12
            border.color: borderGlass
            border.width: 1
        }
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 12
            Text {
                text: importProgressOverlay.actionName
                color: importProgressOverlay.activeColor
                font.bold: true
                font.pixelSize: 15
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: importProgressOverlay.totalItems > 0 ? (importProgressOverlay.currentIndex + " / " + importProgressOverlay.totalItems + " Processed") : "Initializing..."
                color: textSecondary
                font.pixelSize: 12
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                width: 220; height: 8; radius: 4; color: borderGlass
                Layout.alignment: Qt.AlignHCenter
                Rectangle {
                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    width: importProgressOverlay.totalItems > 0 ? (importProgressOverlay.currentIndex / Math.max(1, importProgressOverlay.totalItems)) * parent.width : 0
                    color: importProgressOverlay.activeColor
                    radius: 4
                    Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.InOutQuad } }
                }
            }
            Text {
                text: Math.round((importProgressOverlay.currentIndex / Math.max(1, importProgressOverlay.totalItems)) * 100) + "%"
                color: "white"; font.pixelSize: 10; font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }
            
            // --- Secondary Active Item IO Tracker ---
            Rectangle {
                width: 200; height: 4; radius: 2; color: "#222"
                Layout.alignment: Qt.AlignHCenter
                Rectangle {
                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    width: (mainWindow.activeTargetPercent / 100.0) * parent.width
                    color: "#00E676" // Light green
                    radius: 2
                    Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                }
            }
            
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Text {
                    text: "File: " + mainWindow.activeTargetPercent + "%"
                    color: "#A0A0A0"; font.pixelSize: 10
                }
                Text {
                    text: mainWindow.activeTargetMBps.toFixed(2) + " MB/s"
                    color: "#00E676"; font.pixelSize: 10; font.bold: true
                }
            }
            
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                Layout.preferredHeight: 28
                Layout.topMargin: 5
                text: "Cancel"
                contentItem: Text {
                    text: parent.text; color: textSecondary; font.pixelSize: 11; font.bold: true
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.hovered ? "#33FFFFFF" : "transparent"
                    border.color: borderGlass; border.width: 1; radius: 4
                }
                onClicked: {
                    ps1XstationLibraryService.cancelAllImports()
                    mainWindow.isBatchExtracting = false
                    mainWindow.batchActiveJobs = 0
                    mainWindow.extractQueue = []
                    mainWindow.extractIndex = 0
                    mainWindow.batchConvertingMap = ({})
                    mainWindow.librarySelectionMap = ({})
                    mainWindow.selectionMap = ({})
                    importProgressOverlay.close()
                    refreshGames()
                }
            }
        }
    }
    
    Popup {
        id: swissSetupProgressOverlay
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 320
        height: 160
        modal: true
        closePolicy: Popup.NoAutoClose
        
        background: Rectangle {
            color: "#EA0A1929"
            radius: 12
            border.color: borderGlass
            border.width: 1
        }
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 12
            Text {
                text: "Standing up XStation Ecosystem"
                color: accentPrimary
                font.bold: true; font.pixelSize: 15
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: mainWindow.swissSetupStatusStr
                color: textSecondary; font.pixelSize: 12
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                width: 260; height: 8; radius: 4; color: borderGlass
                Layout.alignment: Qt.AlignHCenter
                Rectangle {
                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    width: (mainWindow.swissSetupPercent / 100.0) * parent.width
                    color: accentPrimary
                    radius: 4
                    Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.InOutQuad } }
                }
            }
            Text {
                text: mainWindow.swissSetupPercent + "%"
                color: "white"; font.pixelSize: 10; font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
    
    Popup {
        id: createStructurePopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        height: 280
        modal: true
        closePolicy: Popup.NoAutoClose
        
        property string selectedOde: "XStation System OS"
        
        background: Rectangle {
            color: "#EA0A1929"
            radius: 12
            border.color: borderGlass
            border.width: 1
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12
            
            Text {
                text: qsTr("PS1 ODE Auto Setup")
                color: accentPrimary
                font.bold: true
                font.pixelSize: 18
                Layout.alignment: Qt.AlignHCenter
            }
            
            Text {
                text: qsTr("We couldn't detect a valid XStation installation. Would you like OdeRelic to fetch the latest official release from GitHub and provision it for your drive automatically?")
                color: textPrimary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            
            Item { Layout.fillHeight: true }
            
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                
                Text {
                    text: "Hardware Mod:"
                    color: textSecondary
                    font.bold: true; font.pixelSize: 13
                }
                
                ComboBox {
                    id: odeTypeSelector
                    Layout.fillWidth: true
                    model: ["XStation", "PSIO"]
                    onCurrentTextChanged: createStructurePopup.selectedOde = currentText
                }
            }
            
            Item { Layout.fillHeight: true }
            
            RowLayout {
                Layout.fillWidth: true
                spacing: 15
                
                Button {
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    onClicked: {
                        createStructurePopup.close()
                        mainWindow.isScrapingIO = true
                        refreshGames()
                        currentTabIndex = 0
                    }
                    contentItem: Text {
                        text: parent.text; color: textSecondary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#33FFFFFF" : "transparent"
                        radius: 6; border.color: borderGlass; border.width: 1; implicitHeight: 36
                    }
                }
                
                Button {
                    text: qsTr("Setup Automatically")
                    Layout.fillWidth: true
                    onClicked: {
                        createStructurePopup.close()
                        swissSetupProgressOverlay.open()
                        ps1XstationLibraryService.startXStationSetupAsync(mainWindow.currentLibraryPath, createStructurePopup.selectedOde)
                    }
                    contentItem: Text {
                        text: parent.text; color: "white"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? accentHover : accentPrimary
                        radius: 6; implicitHeight: 36
                    }
                }
            }
        }
    }

    Popup {
        id: formatErrorPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 380
        height: 220
        modal: true
        closePolicy: Popup.NoAutoClose
        
        property string errorDetails: ""

        background: Rectangle {
            color: "#0F1626"
            radius: 12
            border.color: "#FF4D4D"
            border.width: 1
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16
            
            RowLayout {
                spacing: 12
                Layout.alignment: Qt.AlignHCenter
                Text {
                    text: "⚠️" 
                    font.pixelSize: 22
                }
                Text {
                    text: qsTr("Unsupported Drive")
                    color: "#FF4D4D"
                    font.bold: true
                    font.pixelSize: 18
                }
            }
            
            Text {
                text: qsTr("This drive cannot be used with XStation.")
                color: textPrimary
                font.pixelSize: 14
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Rectangle {
                color: "#1A2235"
                radius: 6
                Layout.fillWidth: true
                Layout.preferredHeight: 45
                
                Text {
                    anchors.centerIn: parent
                    text: formatErrorPopup.errorDetails
                    color: "#FFB3B3"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
            
            Item { Layout.fillHeight: true }
            
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                text: qsTr("Close")
                background: Rectangle {
                    color: parent.down ? "#222" : (parent.hovered ? "#333" : "#222")
                    radius: 6
                    border.color: borderGlass
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text; color: textPrimary; font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: formatErrorPopup.close()
            }
        }
        onClosed: {
            if (mainWindow.currentLibraryPath === "") {
                requestBack()
            }
        }
    }

    Popup {
        id: insufficientSpacePopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 380
        height: 220
        modal: true
        closePolicy: Popup.NoAutoClose
        
        property string requiredSpaceStr: ""
        property string availableSpaceStr: ""

        background: Rectangle {
            color: "#0F1626"
            radius: 12
            border.color: "#FF4D4D"
            border.width: 1
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16
            
            RowLayout {
                spacing: 12
                Layout.alignment: Qt.AlignHCenter
                Text { text: "⚠️"; font.pixelSize: 22 }
                Text {
                    text: qsTr("Insufficient Space")
                    color: "#FF4D4D"
                    font.bold: true; font.pixelSize: 18
                }
            }
            
            Text {
                text: qsTr("There is not enough free space on the destination drive to safely complete this import batch.")
                color: textPrimary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Rectangle {
                color: "#1A2235"
                radius: 6
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    Text { text: "Required: " + insufficientSpacePopup.requiredSpaceStr; color: "#FFB3B3"; font.pixelSize: 12; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                    Text { text: "Available: " + insufficientSpacePopup.availableSpaceStr; color: "#FF4D4D"; font.pixelSize: 12; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                }
            }
            
            Item { Layout.fillHeight: true }
            
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                text: qsTr("Close")
                background: Rectangle {
                    color: parent.down ? "#222" : (parent.hovered ? "#333" : "#222")
                    radius: 6; border.color: borderGlass; border.width: 1
                }
                contentItem: Text {
                    text: parent.text; color: textPrimary; font.bold: true
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: insufficientSpacePopup.close()
            }
        }
    }

    Popup {
        id: deleteConfirmationPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        height: 380
        modal: true
        closePolicy: Popup.NoAutoClose
        
        property var selectedPaths: []
        property var selectedNames: []
        
        background: Rectangle {
            color: "#0F1626"
            radius: 12
            border.color: "#FF4D4D"
            border.width: 1
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16
            
            RowLayout {
                spacing: 12
                Layout.alignment: Qt.AlignHCenter
                Text { text: "⚠️"; font.pixelSize: 22 }
                Text {
                    text: qsTr("Confirm Deletion")
                    color: "#FF4D4D"
                    font.bold: true; font.pixelSize: 18
                }
            }
            
            Text {
                text: qsTr("Are you sure you want to permanently remove the following games from your library?\nThis action cannot be undone.")
                color: textPrimary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Rectangle {
                color: "#1A2235"
                radius: 6
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    
                    ColumnLayout {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: deleteConfirmationPopup.selectedNames
                            Text {
                                text: "• " + modelData
                                color: "#FFB3B3"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
            
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 20
                
                Button {
                    text: qsTr("Cancel")
                    Layout.preferredWidth: 120
                    background: Rectangle {
                        color: parent.down ? "#222" : (parent.hovered ? "#333" : "#222")
                        radius: 6; border.color: borderGlass; border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text; color: textPrimary; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: deleteConfirmationPopup.close()
                }
                
                Button {
                    text: qsTr("Delete (" + deleteConfirmationPopup.selectedPaths.length + ")")
                    Layout.preferredWidth: 120
                    background: Rectangle {
                        color: parent.down ? "#111" : (parent.hovered ? "#CC3333" : "transparent")
                        radius: 6; border.color: "#FF4D4D"; border.width: 1
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                    contentItem: Text {
                        text: parent.text; color: parent.hovered ? "white" : "#FF4D4D"; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                    onClicked: {
                        for(let i = 0; i < deleteConfirmationPopup.selectedPaths.length; i++) {
                            systemUtils.deleteGame(deleteConfirmationPopup.selectedPaths[i], true);
                        }
                        mainWindow.librarySelectionMap = ({});
                        refreshGames();
                        
                        let spaceObj = systemUtils.getStorageSpace(mainWindow.currentLibraryPath);
                        mainWindow.targetDriveTotalSpace = spaceObj.total;
                        mainWindow.targetDriveFreeSpace = spaceObj.free;
                        deleteConfirmationPopup.close();
                    }
                }
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Select PS1 Root Folder")
        onAccepted: {
            let cleanPath = ps1XstationLibraryService.urlToLocalFile(selectedFolder.toString())
            let structureCheck = ps1XstationLibraryService.checkXStationFolder(cleanPath)

            mainWindow.currentLibraryPath = cleanPath
            
            if (!structureCheck.isSetup) {
                createStructurePopup.open()
            } else {
                mainWindow.isScrapingIO = true
                refreshGames()
                ps1XstationLibraryService.checkXStationUpdateAsync(cleanPath)
                currentTabIndex = 0
            }
        }
        onRejected: {
            if (mainWindow.currentLibraryPath === "") {
                requestBack()
            }
        }
    }


    
    FileDialog {
        id: addGamesDialog
        title: qsTr("Select Games to Import")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["PS1 CD Images (*.bin *.cue *.img *.iso *.chd)", "All files (*)"]
        onAccepted: {
            let urls = []
            for (let i = 0; i < selectedFiles.length; i++) urls.push(selectedFiles[i].toString())
            folderScanOverlay.open()
            ps1XstationLibraryService.scanExternalFilesAsync(urls, false)
        }
    }

    FolderDialog {
        id: addGamesFolderDialog
        title: qsTr("Select Folder to Import GC Games")
        onAccepted: {
            folderScanOverlay.open()
            ps1XstationLibraryService.scanExternalFilesAsync([selectedFolder.toString()], false)
        }
    }
    

    
    function refreshGames() {
        if (currentLibraryPath !== "") {
            ps1XstationLibraryService.startGetGamesFilesAsync(currentLibraryPath)
        }
    }


    
    function updateSort() {
        let sorted = []
        for (let i = 0; i < mainWindow.gameFiles.length; i++) {
            sorted.push(mainWindow.gameFiles[i])
        }
        
        sorted.sort(function(a, b) {
            let valA, valB;
            if (sortCriteria === "name") {
                let nameA = a.name; let gidA = extractGameId(a.name);
                if (gidA && nameA.startsWith(gidA)) nameA = nameA.substring(gidA.length + 1);
                
                let nameB = b.name; let gidB = extractGameId(b.name);
                if (gidB && nameB.startsWith(gidB)) nameB = nameB.substring(gidB.length + 1);

                valA = nameA.toLowerCase(); valB = nameB.toLowerCase();
            } else if (sortCriteria === "size") {
                valA = a.stats.size; valB = b.stats.size;
            } else if (sortCriteria === "slu") {
                valA = extractGameId(a.name); valB = extractGameId(b.name);
            } else if (sortCriteria === "media") {
                valA = a.path.includes("/CD/") ? 0 : 1; valB = b.path.includes("/CD/") ? 0 : 1;
            }
            if (valA < valB) return sortAscending ? -1 : 1;
            if (valA > valB) return sortAscending ? 1 : -1;
            return 0;
        })
        mainWindow.gameFiles = []
        mainWindow.gameFiles = sorted
    }
    

    
    function extractProcessor() {
        if (!isBatchExtracting) return;
        
        while (mainWindow.batchActiveJobs < mainWindow.batchMaxJobs && extractIndex < extractQueue.length) {
            let g = extractQueue[extractIndex]
            extractIndex++;
            let filePath = g.path
            
            let gameId = extractGameId(g.name)
            if (gameId === "") {
                let hexScan = ps1XstationLibraryService.tryDetermineGameIdFromHex(filePath);
                if (hexScan.success) gameId = hexScan.gameId;
            }
            
            let newMap = Object.assign({}, mainWindow.batchConvertingMap)
            newMap[filePath] = true
            mainWindow.batchConvertingMap = newMap
            
            let newNames = Object.assign({}, mainWindow.batchConvertingNames)
            newNames[filePath] = g.name
            mainWindow.batchConvertingNames = newNames
            
            mainWindow.batchActiveJobs++;
            mainWindow.activeTargetPercent = 0
            mainWindow.activeTargetMBps = 0.0
            ps1XstationLibraryService.startImportIsoAsync(filePath, mainWindow.currentLibraryPath, gameId, g.name)
            
            continue;
        }
        
        if (mainWindow.batchActiveJobs === 0 && extractIndex >= extractQueue.length) {
            isBatchExtracting = false
            mainWindow.selectionMap = ({})
            mainWindow.librarySelectionMap = ({})
            Qt.callLater(refreshGames)
            updateStorageBars()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        
        // --- Navigation Sidebar ---
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 260
            color: bgSidebar
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 20
                
                // Brand Header
                MouseArea {
                    Layout.fillWidth: true
                    height: 50
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mainWindow.requestBack()
                    
                    Rectangle {
                        anchors.fill: parent
                        color: parent.containsMouse ? "#1AFFFFFF" : "transparent"
                        radius: 8
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 15
                        
                        Text {
                            text: "←"
                            color: textSecondary
                            font.pixelSize: 24
                            font.bold: true
                            Layout.leftMargin: 5
                        }
                        
                        Item {
                            width: 32; height: 32
                            
                            Image {
                                id: logoRect
                                anchors.fill: parent
                                source: "qrc:/app_icon.png"
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                            }
                            
                            MultiEffect {
                                source: logoRect
                                anchors.fill: logoRect
                                shadowEnabled: true
                                shadowColor: accentPrimary
                                shadowOpacity: 0.2
                                shadowBlur: 1.0
                            }
                        }
                        
                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: qsTr("HOME")
                                font.pixelSize: 16
                                font.bold: true
                                font.family: "Inter"
                                color: textPrimary
                                font.letterSpacing: 1
                            }
                            Text {
                                text: qsTr("BACK TO DASHBOARD")
                                font.pixelSize: 10
                                font.bold: true
                                font.family: "Inter"
                                color: accentPrimary
                            }
                        }
                    }
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: borderGlass
                    Layout.topMargin: 10
                    Layout.bottomMargin: 10
                }
                
                // --- Storage Visualization ---
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: mainWindow.targetDriveTotalSpace > 0

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Storage Media")
                            color: textSecondary; font.pixelSize: 12; font.bold: true
                            Layout.alignment: Qt.AlignLeft
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            // Compute GB values dynamically
                            property double mBase: systemUtils.getStorageMultiplier()
                            property double freeGb: mainWindow.targetDriveFreeSpace / (mBase * mBase * mBase)
                            property double totalGb: mainWindow.targetDriveTotalSpace / (mBase * mBase * mBase)
                            property double usedGb: totalGb - freeGb
                            text: usedGb.toFixed(1) + " GB / " + totalGb.toFixed(1) + " GB"
                            color: textPrimary; font.pixelSize: 11; font.bold: true
                            Layout.alignment: Qt.AlignRight
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 8
                        radius: 4
                        color: borderGlass

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: mainWindow.targetDriveTotalSpace > 0 ? ((mainWindow.targetDriveTotalSpace - mainWindow.targetDriveFreeSpace) / mainWindow.targetDriveTotalSpace) * parent.width : 0
                            radius: 4
                            color: (mainWindow.targetDriveFreeSpace / mainWindow.targetDriveTotalSpace) < 0.05 ? "#FF4D4D" : accentPrimary
                            Behavior on width { NumberAnimation { duration: 350; easing.type: Easing.OutQuad } }
                            Behavior on color { ColorAnimation { duration: 350 } }
                        }
                    }
                }

                // Change Disk Re-router
                Button {
                    objectName: "btnChangeTarget"
                    Layout.fillWidth: true
                    text: qsTr("Disconnect & Change Target")
                    onClicked: {
                        mainWindow.currentLibraryPath = ""
                        targetSetupDialog.open()
                    }
                    contentItem: Text {
                        text: parent.text; color: textSecondary; font.pixelSize: 12; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.down ? "#111" : (parent.hovered ? borderGlass : "transparent")
                        radius: 6; implicitHeight: 32; border.color: borderGlass; border.width: 1
                    }
                }
                
                // --- Update Banner ---
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: updateColumn.implicitHeight + 20
                    radius: 12
                    color: "#2C1A1A"
                    border.color: "#8B3A3A"
                    border.width: 1
                    visible: mainWindow.swissUpdateAvailable
                    
                    ColumnLayout {
                        id: updateColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6
                        
                        Text {
                            text: "⚠️ XStation Update Available"
                            color: "#FF8C8C"; font.pixelSize: 12; font.bold: true
                        }
                        
                        Text {
                            text: "Latest: " + mainWindow.swissRemoteVersion
                            color: "white"; font.pixelSize: 11
                        }
                        
                        Button {
                            objectName: "btnUpdateNow"
                            Layout.fillWidth: true
                            text: "Update Now (" + mainWindow.savedOdeType + ")"
                            onClicked: {
                                swissSetupProgressOverlay.open()
                                ps1XstationLibraryService.startXStationSetupAsync(mainWindow.currentLibraryPath, mainWindow.savedOdeType)
                            }
                            contentItem: Text {
                                text: parent.text; color: "white"; font.bold: true; font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#8B3A3A" : "#6B2A2A"
                                radius: 6; implicitHeight: 28
                            }
                        }
                    }
                }
                
                // ── PS2 Section Divider ───────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    spacing: 8
                    Rectangle { height: 1; Layout.fillWidth: true; color: accentPrimary; opacity: 0.3 }
                    Text {
                        text: "PlayStation 1"
                        color: accentPrimary; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2
                        opacity: 0.7
                    }
                    Rectangle { height: 1; Layout.fillWidth: true; color: accentPrimary; opacity: 0.3 }
                }

                // Navigation Buttons Array
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("PlayStation 1 Import") + "  (" + importGames.length + ")"
                        onClicked: currentTabIndex = 1
                        contentItem: Text {
                            text: parent.text; color: currentTabIndex === 1 ? "white" : textSecondary
                            font.bold: true; font.pixelSize: 14; horizontalAlignment: Text.AlignLeft
                            leftPadding: 20; verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: currentTabIndex === 1 ? "#33FF4D4D" : "transparent"
                            radius: 8; implicitHeight: 46
                            border.color: currentTabIndex === 1 ? accentPrimary : (parent.hovered ? borderGlass : "transparent")
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                    }
                    
                    // Hierarchical Actions Box elegantly organically functionally creatively cleanly properly
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 15
                        implicitHeight: importActionsLayout.implicitHeight + 16
                        color: "#161925"
                        radius: 8
                        border.color: borderGlass; border.width: 1
                        visible: currentTabIndex === 1
                        
                        ColumnLayout {
                            id: importActionsLayout
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 8
                            spacing: 8
                            
                            Text {
                                text: qsTr("ACTIONS")
                                color: textSecondary
                                font.pixelSize: 10
                                font.bold: true; font.letterSpacing: 2
                            }
                            
                            Button {
                                objectName: "btnImportSelected"
                                Layout.fillWidth: true
                                property int selectedCount: Object.values(mainWindow.selectionMap).filter(v => v === true).length
                                text: mainWindow.isBatchExtracting ? qsTr("Importing...") : qsTr("Import ") + selectedCount + qsTr(" Games")
                                enabled: selectedCount > 0 || mainWindow.isBatchExtracting
                                opacity: enabled ? 1.0 : 0.5
                                
                                onClicked: {
                                    if (mainWindow.isBatchExtracting) return;
                                    let queue = []
                                    let totalRequiredBytes = 0
                                    for (let i = 0; i < importGames.length; i++) {
                                        if (mainWindow.selectionMap[importGames[i].path] === true) {
                                            queue.push(importGames[i])
                                            totalRequiredBytes += importGames[i].stats.size
                                        }
                                    }
                                    if (queue.length === 0) return;
                                    
                                    if (totalRequiredBytes > mainWindow.targetDriveFreeSpace) {
                                        insufficientSpacePopup.requiredSpaceStr = (totalRequiredBytes / (1024*1024*1024)).toFixed(2) + " GB"
                                        insufficientSpacePopup.availableSpaceStr = (mainWindow.targetDriveFreeSpace / (1024*1024*1024)).toFixed(2) + " GB"
                                        insufficientSpacePopup.open()
                                        return;
                                    }
        
                                    ps1XstationLibraryService.resetCancelFlag()
                                    mainWindow.extractQueue = queue
                                    mainWindow.extractIndex = 0
                                    mainWindow.isBatchExtracting = true
                                    mainWindow.batchActiveJobs = 0
                                    mainWindow.extractProcessor()
                                }
                                
                                contentItem: Text {
                                    text: parent.text; color: "white"; font.bold: true; font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    width: parent.width - 10
                                }
                                background: Rectangle {
                                    color: parent.down ? "#111" : (parent.hovered ? "#3A3F58" : "transparent")
                                    radius: 6; border.color: accentPrimary; border.width: 1
                                    implicitHeight: 28
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: mainWindow.importProgressOverlay && mainWindow.importProgressOverlay.totalItems > 0 ? (mainWindow.importProgressOverlay.currentIndex / mainWindow.importProgressOverlay.totalItems) * parent.width : 0
                                        radius: 6
                                        color: "#55FF7A00"
                                        Behavior on width { NumberAnimation { duration: 200 } }
                                        visible: mainWindow.isBatchExtracting
                                    }
                                }
                            }

                            Button {
                                objectName: "btnAddGames"
                                Layout.fillWidth: true
                                text: "Add Games"
                                onClicked: addGamesDialog.open()
                                contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                background: Rectangle { color: parent.down ? "#111" : (parent.hovered ? "#3A3F58" : "transparent"); radius: 6; border.color: accentPrimary; border.width: 1; implicitHeight: 28; Behavior on color { ColorAnimation { duration: 150 } } }
                            }

                            Button {
                                objectName: "btnAddFolder"
                                Layout.fillWidth: true
                                text: "Add Folder"
                                onClicked: addGamesFolderDialog.open()
                                contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                background: Rectangle { color: parent.down ? "#111" : (parent.hovered ? "#3A3F58" : "transparent"); radius: 6; border.color: accentPrimary; border.width: 1; implicitHeight: 28; Behavior on color { ColorAnimation { duration: 150 } } }
                            }

                            Button {
                                id: massSelectBtn
                                objectName: "btnSelectAll"
                                Layout.fillWidth: true
                                property bool allSelected: false
                                text: allSelected ? "Deselect All" : "Select All"
                                onClicked: {
                                    allSelected = !allSelected
                                    let newMap = {}
                                    if (allSelected) {
                                        for (let i = 0; i < importGames.length; i++) newMap[importGames[i].path] = true
                                    }
                                    mainWindow.selectionMap = newMap
                                }
                                contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                background: Rectangle { color: parent.down ? "#111" : (parent.hovered ? "#3A3F58" : "transparent"); radius: 6; border.color: accentPrimary; border.width: 1; implicitHeight: 28; Behavior on color { ColorAnimation { duration: 150 } } }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("PlayStation 1 Library") + "  (" + libraryGames.length + ")"
                        onClicked: currentTabIndex = 0
                        contentItem: Text {
                            text: parent.text; color: currentTabIndex === 0 ? "white" : textSecondary
                            font.bold: true; font.pixelSize: 14; horizontalAlignment: Text.AlignLeft
                            leftPadding: 20; verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: currentTabIndex === 0 ? "#33FF4D4D" : "transparent"
                            radius: 8; implicitHeight: 46
                            border.color: currentTabIndex === 0 ? accentPrimary : (parent.hovered ? borderGlass : "transparent")
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                    }
                    
                    // Hierarchical Actions Box for Library nicely flawlessly smartly
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 15
                        implicitHeight: libraryActionsLayout.implicitHeight + 16
                        color: "#161925"
                        radius: 8
                        border.color: borderGlass; border.width: 1
                        visible: currentTabIndex === 0 && libraryGames.length > 0
                        
                        ColumnLayout {
                            id: libraryActionsLayout
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 8
                            spacing: 8
                            
                            Text {
                                text: qsTr("ACTIONS")
                                color: textSecondary
                                font.pixelSize: 10
                                font.bold: true; font.letterSpacing: 2
                            }
                            
                            Button {
                                id: fetchArtBtn
                                objectName: "btnFetchArt"
                                Layout.fillWidth: true
                                
                                property bool fetching: false
                                property int currentOp: 0
                                property int totalOp: 0
                                
                                text: qsTr("Fetch Missing Artwork")
                                enabled: !fetching
                                opacity: enabled ? 1.0 : 0.5
                                
                                Connections {
                                    target: ps1XstationLibraryService
                                    function onBatchArtDownloadProgress(current, total) {
                                        fetchArtBtn.currentOp = current
                                        fetchArtBtn.totalOp = total
                                    }
                                    function onBatchArtDownloadFinished(success) {
                                        fetchArtBtn.fetching = false
                                        refreshGames()
                                    }
                                }
                                
                                onClicked: {
                                    if (fetching || libraryGames.length === 0) return;
                                    fetching = true;
                                    currentOp = 0;
                                    totalOp = libraryGames.length;
                                    
                                    let payload = [];
                                    for(let i=0; i < libraryGames.length; i++) {
                                        let g = libraryGames[i];
                                        payload.push({
                                            path: g.path,
                                            name: g.name,
                                            binaryFileName: g.binaryFileName,
                                            regexId: extractGameId(g.name)
                                        });
                                    }
                                    ps1XstationLibraryService.startBatchArtDownloadAsync(payload);
                                }
                                
                                contentItem: Text {
                                    text: parent.fetching ? qsTr("Downloading... (") + parent.currentOp + "/" + parent.totalOp + ")" : parent.text
                                    color: "white"
                                    font.bold: true; font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    width: parent.width - 10
                                }
                                
                                background: Rectangle {
                                    color: parent.down ? "#111" : (parent.hovered ? "#3A3F58" : "#2A2F40")
                                    radius: 6; border.color: borderGlass; border.width: 1
                                    implicitHeight: 32
                                    
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: fetchArtBtn.totalOp > 0 ? (fetchArtBtn.currentOp / fetchArtBtn.totalOp) * parent.width : 0
                                        radius: 6
                                        color: "#55FF7A00" // Subtle orange progress
                                        Behavior on width { NumberAnimation { duration: 200 } }
                                        visible: fetchArtBtn.fetching
                                    }
                                }
                            }
                            

                            Button {
                                id: deleteGamesBtn
                                objectName: "btnDeleteSelected"
                                Layout.fillWidth: true
                                
                                property int selectedCount: Object.values(mainWindow.librarySelectionMap).filter(v => v === true).length
                                text: qsTr("Delete Selected") + " (" + selectedCount + ")"
                                enabled: selectedCount > 0 && !fetchArtBtn.fetching
                                opacity: enabled ? 1.0 : 0.5
                                
                                onClicked: {
                                    let paths = Object.keys(mainWindow.librarySelectionMap);
                                    let selectedPaths = [];
                                    let selectedNames = [];
                                    for(let i = 0; i < paths.length; i++) {
                                        if (mainWindow.librarySelectionMap[paths[i]] === true) {
                                            selectedPaths.push(paths[i]);
                                            for(let j = 0; j < mainWindow.libraryGames.length; j++) {
                                                if(mainWindow.libraryGames[j].path === paths[i]) {
                                                    selectedNames.push(mainWindow.libraryGames[j].name);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    deleteConfirmationPopup.selectedPaths = selectedPaths;
                                    deleteConfirmationPopup.selectedNames = selectedNames;
                                    deleteConfirmationPopup.open();
                                }
                                
                                contentItem: Text {
                                    text: parent.text
                                    color: enabled && parent.hovered ? "white" : "#FF4D4D"
                                    font.bold: true; font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    width: parent.width - 10
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }
                                
                                background: Rectangle {
                                    color: parent.down ? "#111" : (parent.hovered ? "#CC3333" : "transparent")
                                    radius: 6; border.color: parent.hovered ? "#FF4D4D" : borderGlass; border.width: 1
                                    implicitHeight: 32
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
        
        // --- Loading Library Scan Overlay Modal ---
        Popup {
            id: libraryScanOverlay
            parent: Overlay.overlay
            anchors.centerIn: parent
            width: 300
            height: 140
            modal: true
            closePolicy: Popup.NoAutoClose
            
            property bool isScraping: mainWindow.isScrapingIO
            onIsScrapingChanged: {
                if (isScraping) open(); else close();
            }

            background: Rectangle {
                color: "#EA0A1929"
                radius: 12
                border.color: borderGlass
                border.width: 1
            }
            
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                Text {
                    text: qsTr("Loading Game Library...")
                    color: "#18A0FB"
                    font.bold: true
                    font.pixelSize: 15
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: mainWindow.libraryScanTotal > 0 ? (mainWindow.libraryScanCurrent + " / " + mainWindow.libraryScanTotal + " Targets") : "Locating paths..."
                    color: textSecondary
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignHCenter
                }
                Rectangle {
                    width: 220; height: 8; radius: 4; color: borderGlass
                    Layout.alignment: Qt.AlignHCenter
                    Rectangle {
                        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                        width: mainWindow.libraryScanTotal > 0 ? (mainWindow.libraryScanCurrent / Math.max(1, mainWindow.libraryScanTotal)) * parent.width : 0
                        color: "#18A0FB"
                        radius: 4
                        Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                    }
                }
            }
        }
        
        // --- Main Presentation Surface ---
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 30
                spacing: 25
                
                // Utility Content Top Bar (Search + Sort)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    
                    TextField {
                        id: searchInput
                        placeholderText: qsTr("Search by Game Title or ID...")
                        Layout.fillWidth: true
                        color: "white"
                        placeholderTextColor: textSecondary
                        font.pixelSize: 14
                        leftPadding: 16; rightPadding: 16
                        
                        background: Rectangle {
                            color: "#1E2230"
                            radius: 8
                            border.color: searchInput.activeFocus ? accentPrimary : "transparent"
                            border.width: 1
                            implicitHeight: 40
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                        }
                        onTextChanged: mainWindow.searchQuery = text
                    }
                    


                    ComboBox {
                        id: sortCombo
                        model: [qsTr("Name"), qsTr("Size"), qsTr("Media")]
                        currentIndex: 0
                        background: Rectangle {
                            color: "#1E2230"; radius: 8; implicitWidth: 130; implicitHeight: 40
                            border.color: parent.hovered ? borderGlass : "transparent"
                        }
                        contentItem: Text {
                            text: qsTr("Sort:") + " " + sortCombo.currentText; color: textSecondary; verticalAlignment: Text.AlignVCenter
                            leftPadding: 16; font.pixelSize: 14; elide: Text.ElideRight
                        }
                        indicator: Text {
                            text: "▼"; color: textSecondary; font.pixelSize: 10
                            anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
                        }
                        onCurrentIndexChanged: {
                            let modes = ["name", "size", "media"]
                            mainWindow.sortCriteria = modes[currentIndex]
                            updateSort()
                        }
                    }
                    
                    Button {
                        text: mainWindow.sortAscending ? qsTr("▲ Asc") : qsTr("▼ Desc")
                        onClicked: { mainWindow.sortAscending = !mainWindow.sortAscending; updateSort() }
                        contentItem: Text {
                            text: parent.text; color: textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 14
                        }
                        background: Rectangle {
                            color: parent.hovered ? borderGlass : "#1E2230"
                            radius: 8; implicitWidth: 70; implicitHeight: 40
                        }
                    }
                    
                    Item { width: 10 } // Spacing Buffer
                    Text { text: qsTr("View"); color: textSecondary; font.pixelSize: 14 }
                    
                    // Modern Grid Toggle Array
                    Rectangle {
                        width: 40; height: 40; radius: 8
                        color: mainWindow.isGridView ? "#33FF4D4D" : "transparent"
                        border.color: mainWindow.isGridView ? accentPrimary : "transparent"
                        
                        Grid {
                            anchors.centerIn: parent
                            columns: 2; spacing: 3
                            Repeater { model: 4; Rectangle { width: 7; height: 7; radius: 2; color: mainWindow.isGridView ? accentPrimary : textSecondary } }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: mainWindow.isGridView = true
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                    
                    // Modern List Toggle Array
                    Rectangle {
                        width: 40; height: 40; radius: 8
                        color: !mainWindow.isGridView ? "#33FF4D4D" : "transparent"
                        border.color: !mainWindow.isGridView ? accentPrimary : "transparent"
                        
                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            Repeater {
                                model: 3
                                Row {
                                    spacing: 3
                                    Rectangle { width: 4; height: 4; radius: 1; color: !mainWindow.isGridView ? accentPrimary : textSecondary }
                                    Rectangle { width: 12; height: 4; radius: 1; color: !mainWindow.isGridView ? accentPrimary : textSecondary }
                                }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: mainWindow.isGridView = false
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                }
                
                // Multi-Tab Router Core Framework
                StackLayout {
                    currentIndex: currentTabIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    // --- TAB 0: Modern Grid Library Surface ---
                    Rectangle {
                        color: "transparent"
                        
                        GridView {
                            id: libraryGrid
                            visible: mainWindow.isGridView
                            anchors.fill: parent
                            clip: true
                            model: mainWindow.libraryGames
                            cellWidth: 215
                            cellHeight: 350
                            
                            boundsBehavior: Flickable.StopAtBounds
                            
                            delegate: Item {
                                width: libraryGrid.cellWidth - 20
                                height: libraryGrid.cellHeight - 20
                                
                                Rectangle {
                                    id: cardRect
                                    anchors.fill: parent
                                    color: bgCard
                                    radius: 12
                                    border.color: mainWindow.librarySelectionMap[modelData.path] === true ? cardBorderHover : (mouseArea.containsMouse ? cardBorderHover : cardBorderNormal)
                                    border.width: mainWindow.librarySelectionMap[modelData.path] === true ? 2 : 1
                                    
                                    Behavior on border.color { ColorAnimation { duration: 250 } }
                                    
                                    MouseArea {
                                        id: mouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            let newMap = Object.assign({}, mainWindow.librarySelectionMap)
                                            newMap[modelData.path] = !newMap[modelData.path]
                                            mainWindow.librarySelectionMap = newMap
                                        }
                                    }
                                    
                                    // Checkbox Hook Overlay
                                    Rectangle {
                                        anchors.top: parent.top; anchors.right: parent.right
                                        anchors.margins: 10
                                        width: 22; height: 22; radius: 11
                                        border.color: mainWindow.librarySelectionMap[modelData.path] === true ? accentPrimary : borderGlass
                                        border.width: 2
                                        color: mainWindow.librarySelectionMap[modelData.path] === true ? accentPrimary : "transparent"
                                        z: 10
                                        
                                        Text {
                                            anchors.centerIn: parent; text: "✓"; color: "white"
                                            font.bold: true; font.pixelSize: 14
                                            visible: mainWindow.librarySelectionMap[modelData.path] === true
                                        }
                                    }
                                    
                                    states: State {
                                        name: "hovered"; when: mouseArea.containsMouse
                                        PropertyChanges { target: cardRect; scale: 1.03 }
                                    }
                                    transitions: Transition { NumberAnimation { properties: "scale"; duration: 200; easing.type: Easing.OutBack } }
                                    
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0
                                        
                                        // PS1 CD Jewel Case Physical Wrap
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 180
                                            Layout.margins: 8
                                            Layout.bottomMargin: 0
                                            color: "#181A20" // Dark opaque plastic shell
                                            radius: 3 // Physical DVD cases have sharper corners
                                            border.color: "#080A10"
                                            border.width: 1

                                            // Outer Plastic Bevel Shadow Frame
                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 3
                                                border.color: "#333A4A" // Soft edge bevel lighting
                                                border.width: 1
                                                color: "transparent"
                                                z: 2
                                            }

                                            // Mechanical Spine/Hinge Layer (Thick deep left-side column)
                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                width: 8
                                                color: "#0C0D12"
                                                radius: 2
                                                z: 2
                                                
                                                // Spine plastic curvature specular glint
                                                Rectangle {
                                                    anchors.left: parent.left; anchors.leftMargin: 2
                                                    anchors.top: parent.top; anchors.bottom: parent.bottom
                                                    width: 1
                                                    color: "#22FFFFFF"
                                                }
                                            }

                                            // Inner Sleeve & Artwork Matrix
                                            Rectangle {
                                                anchors.fill: parent
                                                anchors.margins: 2        // Outer plastic rim thickness
                                                anchors.leftMargin: 10    // 8px spine + 2px rim margin
                                                clip: true
                                                radius: 1
                                                color: "#0F0F1A"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.extension.toUpperCase().replace(".", "")
                                                    color: accentPrimary
                                                    font.bold: true; font.pixelSize: 24; opacity: 0.5
                                                    visible: artImage.status !== Image.Ready
                                                }
                                                
                                                Image {
                                                    id: artImage
                                                    anchors.fill: parent
                                                    asynchronous: true
                                                    fillMode: Image.Stretch
                                                    visible: status === Image.Ready
                                                    source: "file://" + modelData.path + "/cover.bmp"
                                                    
                                                    onStatusChanged: {
                                                        // XStation solely relies on cover.bmp explicitly 
                                                    }
                                                }

                                                // Front Plastic Wrap Glare Overlay (Glassmorphism sheen)
                                                Rectangle {
                                                    anchors.fill: parent
                                                    gradient: Gradient {
                                                        GradientStop { position: 0.0; color: "#2AFFFFFF" }
                                                        GradientStop { position: 0.15; color: "transparent" }
                                                        GradientStop { position: 0.85; color: "transparent" }
                                                        GradientStop { position: 1.0; color: "#66000000" }
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // Game Footer Container typography refiner
                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            
                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 12
                                                spacing: 4
                                                
                                                Text {
                                                    text: {
                                                        let gid = extractGameId(modelData.name)
                                                        if (gid && modelData.name.startsWith(gid)) {
                                                            return modelData.name.substring(gid.length + 1)
                                                        }
                                                        return modelData.name
                                                    }
                                                    color: textPrimary
                                                    font.bold: true; font.pixelSize: 14; font.family: "Inter"
                                                    elide: Text.ElideRight; Layout.fillWidth: true; wrapMode: Text.Wrap
                                                    maximumLineCount: 2
                                                    Layout.alignment: Qt.AlignTop
                                                }
                                                
                                                Text {
                                                    text: modelData.discCount !== undefined && modelData.discCount > 1 ? "(" + modelData.discCount + " Discs)" : ""
                                                    visible: modelData.discCount !== undefined && modelData.discCount > 1
                                                    color: accentPrimary
                                                    font.bold: true; font.pixelSize: 11; font.family: "Inter"
                                                    Layout.fillWidth: true
                                                }
                                                
                                                Item { Layout.fillHeight: true }
                                                
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 0
                                                    Text {
                                                        text: {
                                                            let prefix = extractGameId(modelData.name)
                                                            let regionName = "Unknown Region"
                                                            let nameU = modelData.name.toUpperCase()
                                                            
                                                            if (prefix) {
                                                                prefix = prefix.substring(0, 4)
                                                                if (prefix === "SLUS" || prefix === "SCUS") regionName = "NTSC-U"
                                                                else if (prefix === "SLES" || prefix === "SCES" || prefix === "SCED") regionName = "PAL"
                                                                else if (prefix === "SLPM" || prefix === "SLPS" || prefix === "SCPS" || prefix === "SCPM" || prefix === "SLAJ" || prefix === "SCAJ" || prefix === "SCCS") regionName = "NTSC-J"
                                                                else if (prefix === "SLKA" || prefix === "SCKA") regionName = "NTSC-K"
                                                            } else {
                                                                if (nameU.includes("(USA)")) regionName = "NTSC-U"
                                                                else if (nameU.includes("(EUROPE)") || nameU.includes("(PAL)")) regionName = "PAL"
                                                                else if (nameU.includes("(JAPAN)")) regionName = "NTSC-J"
                                                                else if (nameU.includes("(KOREA)")) regionName = "NTSC-K"
                                                            }
                                                            return "Region: " + regionName
                                                        }
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                    Text {
                                                        text: "Format: CD"
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                    Text {
                                                        text: "Version: " + (modelData.version ? "v" + modelData.version : "v1.00")
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                    Text {
                                                        text: "Size: " + systemUtils.formatSize(modelData.stats.size)
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        ListView {
                            id: libraryList
                            visible: !mainWindow.isGridView
                            anchors.fill: parent
                            clip: true
                            model: mainWindow.libraryGames
                            spacing: 12
                            boundsBehavior: Flickable.StopAtBounds
                            
                            delegate: Item {
                                width: libraryList.width - 20
                                height: 72
                                
                                Rectangle {
                                    id: listCardRect
                                    anchors.fill: parent
                                    color: bgCard
                                    radius: 12
                                    border.color: mainWindow.librarySelectionMap[modelData.path] === true ? cardBorderHover : (listMouseArea.containsMouse ? cardBorderHover : cardBorderNormal)
                                    border.width: mainWindow.librarySelectionMap[modelData.path] === true ? 2 : 1
                                    
                                    Behavior on border.color { ColorAnimation { duration: 250 } }
                                    
                                    MouseArea {
                                        id: listMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            let newMap = Object.assign({}, mainWindow.librarySelectionMap)
                                            newMap[modelData.path] = !newMap[modelData.path]
                                            mainWindow.librarySelectionMap = newMap
                                        }
                                    }
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 20
                                        
                                        // Checkbox Hook Overlay
                                        Rectangle {
                                            width: 22; height: 22; radius: 11
                                            border.color: mainWindow.librarySelectionMap[modelData.path] === true ? accentPrimary : borderGlass
                                            border.width: 2
                                            color: mainWindow.librarySelectionMap[modelData.path] === true ? accentPrimary : "transparent"
                                            
                                            Text {
                                                anchors.centerIn: parent; text: "✓"; color: "white"
                                                font.bold: true; font.pixelSize: 14
                                                visible: mainWindow.librarySelectionMap[modelData.path] === true
                                            }
                                        }
                                        
                                        // Mini Jewel Case Overlay
                                        Rectangle {
                                            width: 48; height: 48; radius: 3
                                            color: "#181A20"; border.color: accentPrimary; border.width: 1
                                            
                                            // Inner Sleeve
                                            Rectangle {
                                                anchors.fill: parent
                                                anchors.margins: 1
                                                anchors.leftMargin: 3 // Mini spine
                                                clip: true
                                                radius: 1
                                                color: "#0F0F1A"
                                                
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.extension.toUpperCase().replace(".", "")
                                                    color: accentPrimary
                                                    font.bold: true; font.pixelSize: 13
                                                    visible: listArtImage.status !== Image.Ready
                                                }
                                                
                                                Image {
                                                    id: listArtImage
                                                    anchors.fill: parent
                                                    asynchronous: true
                                                    fillMode: Image.PreserveAspectCrop
                                                    visible: status === Image.Ready
                                                    source: "file://" + modelData.path + "/cover.bmp"
                                                    
                                                    onStatusChanged: {
                                                        // XStation solely relies on cover.bmp explicitly 
                                                    }
                                                }
                                                
                                                // Gloss Sheen
                                                Rectangle {
                                                    anchors.fill: parent
                                                    gradient: Gradient {
                                                        GradientStop { position: 0.0; color: "#33FFFFFF" }
                                                        GradientStop { position: 0.2; color: "transparent" }
                                                        GradientStop { position: 1.0; color: "#55000000" }
                                                    }
                                                }
                                            }
                                        }
                                        
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 6
                                            Text {
                                                text: {
                                                    let gid = extractGameId(modelData.name)
                                                    if (gid && modelData.name.startsWith(gid)) {
                                                        return modelData.name.substring(gid.length + 1)
                                                    }
                                                    return modelData.name
                                                }
                                                color: textPrimary
                                                font.bold: true; font.pixelSize: 17; font.family: "Inter"
                                                elide: Text.ElideRight; Layout.fillWidth: true; Layout.rightMargin: 20
                                            }
                                            
                                            Text {
                                                text: modelData.discCount !== undefined && modelData.discCount > 1 ? "(" + modelData.discCount + " Discs)" : ""
                                                visible: modelData.discCount !== undefined && modelData.discCount > 1
                                                color: accentPrimary
                                                font.bold: true; font.pixelSize: 13; font.family: "Inter"
                                            }
                                            RowLayout {
                                                spacing: 12
                                                
                                                Text {
                                                    text: extractGameId(modelData.name) || "UNKNOWN ID"
                                                    color: accentPrimary
                                                    font.bold: true; font.pixelSize: 11; font.family: "monospace"
                                                }
                                                
                                                Text {
                                                    text: "• " + systemUtils.formatSize(modelData.stats.size)
                                                    color: textSecondary; font.pixelSize: 11; font.bold: true; font.family: "Inter"
                                                }
                                                
                                                Text {
                                                    text: "• CD"
                                                    color: textSecondary; font.pixelSize: 11; font.bold: true
                                                }
                                                
                                                Text {
                                                    text: "• " + (modelData.version ? "v" + modelData.version : "v1.00")
                                                    color: textSecondary; font.pixelSize: 11; font.bold: true
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } // End TAB 0
                    
                    // --- TAB 1: Import Operations UI Refinement ---
                    Rectangle {
                        color: "transparent"
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 15
                            
                            // Import File Matrix
                            GridView {
                                id: importGrid
                                visible: mainWindow.isGridView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                model: mainWindow.importGames
                                cellWidth: 280
                                cellHeight: 120
                                boundsBehavior: Flickable.StopAtBounds
                                
                                delegate: Item {
                                    width: 260
                                    height: 100
                                    
                                    Rectangle {
                                        id: importCardRect
                                        anchors.fill: parent
                                        color: bgCard
                                        radius: 12
                                        border.color: mainWindow.selectionMap[modelData.path] === true ? cardBorderHover : cardBorderNormal
                                        border.width: mainWindow.selectionMap[modelData.path] === true ? 2 : 1
                                        
                                        Behavior on border.color { ColorAnimation { duration: 150 } }
                                        
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                let newMap = Object.assign({}, mainWindow.selectionMap)
                                                newMap[modelData.path] = !newMap[modelData.path]
                                                mainWindow.selectionMap = newMap
                                            }
                                        }
                                        
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 15
                                            
                                            // Checkbox Hook Overlay
                                            Rectangle {
                                                width: 22; height: 22; radius: 11
                                                border.color: mainWindow.selectionMap[modelData.path] === true ? accentPrimary : borderGlass
                                                border.width: 2
                                                color: mainWindow.selectionMap[modelData.path] === true ? accentPrimary : "transparent"
                                                
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "✓"
                                                    color: "white"
                                                    font.bold: true
                                                    font.pixelSize: 14
                                                    visible: mainWindow.selectionMap[modelData.path] === true
                                                }
                                            }
                                            
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                
                                                Text {
                                                    text: modelData.name
                                                    color: textPrimary
                                                    font.bold: true; font.pixelSize: 14; font.family: "Inter"
                                                    elide: Text.ElideRight; Layout.fillWidth: true
                                                    maximumLineCount: 2; wrapMode: Text.Wrap
                                                }
                                                
                                                Item { Layout.fillHeight: true }
                                                
                                                RowLayout {
                                                    spacing: 10
                                                    Layout.fillWidth: true
                                                    
                                                    Text {
                                                        text: modelData.extension.toUpperCase().replace(".", "")
                                                        color: textSecondary; font.pixelSize: 11; font.bold: true
                                                    }
                                                    Text {
                                                        text: "• CD"
                                                        color: accentPrimary; font.pixelSize: 11; font.bold: true
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text {
                                                        text: systemUtils.formatSize(modelData.stats.size)
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // Conversion Node Spinner Overlay
                                        Rectangle {
                                            anchors.fill: parent
                                            color: "#EA131620"
                                            radius: 12
                                            visible: mainWindow.batchConvertingMap[modelData.path] === true
                                            z: 50
                                            
                                            ColumnLayout {
                                                anchors.centerIn: parent
                                                spacing: 8
                                                Text {
                                                    text: "Stripping Sectors..."
                                                    color: accentPrimary; font.bold: true; font.pixelSize: 12
                                                    Layout.alignment: Qt.AlignHCenter
                                                }
                                                Rectangle {
                                                    width: 140; height: 4; radius: 2; color: borderGlass
                                                    Rectangle {
                                                        width: 40; height: parent.height; radius: 2; color: accentPrimary
                                                        SequentialAnimation on x {
                                                            loops: Animation.Infinite
                                                            NumberAnimation { from: 0; to: 100; duration: 600; easing.type: Easing.InOutQuad }
                                                            NumberAnimation { from: 100; to: 0; duration: 600; easing.type: Easing.InOutQuad }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Import File Matrix - List Alternative
                            ListView {
                                id: importList
                                visible: !mainWindow.isGridView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                model: mainWindow.importGames
                                boundsBehavior: Flickable.StopAtBounds
                                spacing: 8
                                
                                delegate: Item {
                                    width: importList.width - 20
                                    height: 70
                                    
                                    Rectangle {
                                        id: importCardListRect
                                        anchors.fill: parent
                                        color: bgCard
                                        radius: 12
                                        border.color: mainWindow.selectionMap[modelData.path] === true ? cardBorderHover : cardBorderNormal
                                        border.width: mainWindow.selectionMap[modelData.path] === true ? 2 : 1
                                        
                                        Behavior on border.color { ColorAnimation { duration: 150 } }
                                        
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                let newMap = Object.assign({}, mainWindow.selectionMap)
                                                newMap[modelData.path] = !newMap[modelData.path]
                                                mainWindow.selectionMap = newMap
                                            }
                                        }
                                        
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 15
                                            spacing: 20
                                            
                                            // Checkbox Hook Overlay
                                            Rectangle {
                                                width: 24; height: 24; radius: 12
                                                border.color: mainWindow.selectionMap[modelData.path] === true ? accentPrimary : borderGlass
                                                border.width: 2
                                                color: mainWindow.selectionMap[modelData.path] === true ? accentPrimary : "transparent"
                                                
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "✓"
                                                    color: "white"; font.bold: true; font.pixelSize: 15
                                                    visible: mainWindow.selectionMap[modelData.path] === true
                                                }
                                            }
                                            
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.alignment: Qt.AlignVCenter
                                                spacing: 2
                                                
                                                Text {
                                                    text: modelData.name
                                                    color: textPrimary
                                                    font.bold: true; font.pixelSize: 15; font.family: "Inter"
                                                    elide: Text.ElideRight; Layout.fillWidth: true
                                                }
                                                
                                                RowLayout {
                                                    spacing: 12
                                                    Layout.fillWidth: true
                                                    
                                                    Text {
                                                        text: modelData.extension.toUpperCase().replace(".", "")
                                                        color: textSecondary; font.pixelSize: 12; font.bold: true
                                                    }
                                                    Text {
                                                        text: "• CD"
                                                        color: accentPrimary; font.pixelSize: 12; font.bold: true
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text {
                                                        text: systemUtils.formatSize(modelData.stats.size)
                                                        color: textSecondary; font.pixelSize: 12; font.family: "Inter"
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // Conversion Node Spinner Overlay
                                        Rectangle {
                                            anchors.fill: parent
                                            color: "#EA131620"
                                            radius: 12
                                            visible: mainWindow.batchConvertingMap[modelData.path] === true
                                            z: 50
                                            
                                            RowLayout {
                                                anchors.centerIn: parent
                                                spacing: 15
                                                Text {
                                                    text: "Stripping Sectors..."
                                                    color: accentPrimary; font.bold: true; font.pixelSize: 13
                                                }
                                                Rectangle {
                                                    width: 140; height: 6; radius: 3; color: borderGlass
                                                    Rectangle {
                                                        width: 40; height: parent.height; radius: 3; color: accentPrimary
                                                        SequentialAnimation on x {
                                                            loops: Animation.Infinite
                                                            NumberAnimation { from: 0; to: 100; duration: 600; easing.type: Easing.InOutQuad }
                                                            NumberAnimation { from: 100; to: 0; duration: 600; easing.type: Easing.InOutQuad }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } // End TAB 1

                }
            }
        }
    }
}
