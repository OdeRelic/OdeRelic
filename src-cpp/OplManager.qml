import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: mainWindow
    signal requestBack()
    
    // Modern Dark Theme Colors & Transparent Glass Aesthetics
    property color bgMain: "#D9090A0F" // Highly translucent dark slate
    property color bgSidebar: "#4D131620" // Frosted sidebar tint
    property color bgCard: "transparent" // Fully transparent cards matching mock
    property color cardBorderNormal: "#88FF4D4D" // Soft red border
    property color cardBorderHover: "#FFFF4D4D" // Hard glowing red border
    property color accentPrimary: "#FF4D4D"
    property color accentHover: "#FF6B6B"
    property color textPrimary: "#FFFFFF"
    property color textSecondary: "#8B95A5"
    property color borderGlass: "#2A2F40"
    
    color: bgMain

    function extractGameId(filename) {
        let match = filename.match(/(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|SCAJ|SLKA|SCKA|SCED|SCCS)[_.-]?\d{3}\.?\d{2}/i);
        return match ? match[0].toUpperCase() : "";
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
    property string searchQuery: ""
    
    property bool isGridView: true
    
    property string sortCriteria: "name"
    property bool sortAscending: true
    
    property bool isScrapingIO: false
    
    property var batchConvertingMap: ({})
    property var batchConvertingNames: ({})
    property int batchActiveJobs: 0
    property int batchMaxJobs: 3
    
    property bool isBatchExtracting: false
    property var extractQueue: []
    property int extractIndex: 0
    
    Connections {
        target: oplLibraryService
        function onConversionProgress(sourcePath, percent) {}
        function onImportIsoProgress(sourcePath, percent) {}
        function onArtDownloadProgress(sourcePath, percent) {}
        
        function onGamesFilesLoaded(dirPath, data) {
            if (dirPath === mainWindow.currentLibraryPath) {
                isBatchExtracting = false
                isScrapingIO = false
                mainWindow.gameFiles = []
                mainWindow.gameFiles = data.data
                mainWindow.selectionMap = ({})
                updateSort()
            }
        }
        
        function onArtDownloadFinished(sourcePath, success, message) {
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
                    let res = oplLibraryService.tryDetermineGameIdFromHex(destIsoPath)
                    if (res && res.success) {
                        let artFolder = mainWindow.currentLibraryPath + "/ART"
                        oplLibraryService.startDownloadArtAsync(artFolder, res.gameId, sourcePath)
                        return; // Await async net dispatch
                    }
                } else {
                    console.error("Batch ISO Import Failed: " + message)
                }
                
                // Fallback completion if network fails to dispatch
                let newMap = Object.assign({}, mainWindow.batchConvertingMap)
                delete newMap[sourcePath]
                mainWindow.batchConvertingMap = newMap
                mainWindow.batchActiveJobs--
                extractProcessor()
            }
        }
        
        function onConversionFinished(sourcePath, success, destIsoPath, message) {
            if (mainWindow.batchConvertingMap[sourcePath] === true) {
                let currentName = mainWindow.batchConvertingNames[sourcePath]
                
                if (success) {
                    let res = oplLibraryService.tryDetermineGameIdFromHex(destIsoPath)
                    if (res && res.success) {
                        let isOriginalCD = sourcePath.toLowerCase().endsWith(".bin") || sourcePath.toLowerCase().endsWith(".cue");
                        let renameRes = oplLibraryService.renameGamefile(destIsoPath, mainWindow.currentLibraryPath, res.gameId, currentName, isOriginalCD)
                        if (renameRes.success) {
                            let artFolder = mainWindow.currentLibraryPath + "/ART"
                            oplLibraryService.startDownloadArtAsync(artFolder, res.gameId, sourcePath)
                            oplLibraryService.deleteFileAndCue(sourcePath)
                            return; // Await async net dispatch
                        } else {
                            console.error("Batch Move Failed: " + renameRes.message)
                        }
                    } else {
                        oplLibraryService.deleteFileAndCue(destIsoPath)
                    }
                } else {
                    console.error("Batch CONVERSION Failed: " + message)
                }
                
                // Fallback completion if network fails to dispatch
                let newMap = Object.assign({}, mainWindow.batchConvertingMap)
                delete newMap[sourcePath]
                mainWindow.batchConvertingMap = newMap
                mainWindow.batchActiveJobs--
                extractProcessor()
            }
        }
    }
    
    FolderDialog {
        id: folderDialog
        title: qsTr("Select OPL Root Folder")
        onAccepted: {
            mainWindow.isScrapingIO = true
            mainWindow.currentLibraryPath = selectedFolder.toString().replace("file://", "")
            refreshGames()
            currentTabIndex = 0
        }
    }
    
    FileDialog {
        id: addGamesDialog
        title: qsTr("Select Games to Import")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["PS2 Images (*.iso *.bin *.cue *.zso)", "All files (*)"]
        onAccepted: {
            let urls = []
            for (let i = 0; i < selectedFiles.length; i++) {
                urls.push(selectedFiles[i].toString())
            }
            let newGames = oplLibraryService.getExternalGameFilesData(urls)
            let arr = [].concat(mainWindow.gameFiles)
            for (let i = 0; i < newGames.length; i++) {
                let exists = false;
                for (let j = 0; j < arr.length; j++) {
                    if (arr[j].path === newGames[i].path) { exists = true; break; }
                }
                if (!exists) arr.push(newGames[i]);
            }
            mainWindow.gameFiles = arr
            updateSort()
        }
    }
    
    function refreshGames() {
        if (currentLibraryPath !== "") {
            oplLibraryService.startGetGamesFilesAsync(currentLibraryPath)
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
                valA = a.name.toLowerCase(); valB = b.name.toLowerCase();
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
            let isoPath = g.path
            let isBin = (g.extension.toLowerCase() === ".bin" || g.extension.toLowerCase() === ".cue")
            
            if (isBin) {
                let tempIsoPath = mainWindow.currentLibraryPath + "/.orbit_temp_" + Math.floor(Math.random() * 1000000) + ".iso"
                let newMap = Object.assign({}, mainWindow.batchConvertingMap)
                newMap[g.path] = true
                mainWindow.batchConvertingMap = newMap
                
                let newNames = Object.assign({}, mainWindow.batchConvertingNames)
                newNames[g.path] = g.name
                mainWindow.batchConvertingNames = newNames
                
                mainWindow.batchActiveJobs++;
                oplLibraryService.startConvertBinToIso(isoPath, tempIsoPath)
                continue;
            }

            let res = oplLibraryService.tryDetermineGameIdFromHex(isoPath)
            if (res.success) {
                let newMap = Object.assign({}, mainWindow.batchConvertingMap)
                newMap[isoPath] = true
                mainWindow.batchConvertingMap = newMap
                
                let newNames = Object.assign({}, mainWindow.batchConvertingNames)
                newNames[isoPath] = g.name
                mainWindow.batchConvertingNames = newNames
                
                mainWindow.batchActiveJobs++;
                oplLibraryService.startImportIsoAsync(isoPath, mainWindow.currentLibraryPath, res.gameId, g.name, false)
            }
            
            continue;
        }
        
        if (mainWindow.batchActiveJobs === 0 && extractIndex >= extractQueue.length) {
            isBatchExtracting = false
            mainWindow.selectionMap = ({})
            Qt.callLater(refreshGames)
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
                
                // Navigation Buttons Array
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Game Library") + "  (" + libraryGames.length + ")"
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
                
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Import Operations") + "  (" + importGames.length + ")"
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
                
                Item { Layout.fillHeight: true } 
                
                // Active Disk Router
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: diskColumn.implicitHeight + 30
                    radius: 12
                    color: bgCard
                    border.color: borderGlass
                    
                    ColumnLayout {
                        id: diskColumn
                        anchors.fill: parent
                        anchors.margins: 15
                        spacing: 8
                        
                        Text {
                            text: currentLibraryPath !== "" ? qsTr("Connected Target:") : qsTr("No Disk Selected")
                            color: textSecondary; font.pixelSize: 12; font.bold: true
                        }
                        
                        Text {
                            text: {
                                if (currentLibraryPath === "") return qsTr("Mount ODE Root");
                                let cleanPath = currentLibraryPath;
                                if (cleanPath.endsWith('/')) cleanPath = cleanPath.substring(0, cleanPath.length - 1);
                                if (cleanPath.endsWith('\\')) cleanPath = cleanPath.substring(0, cleanPath.length - 1);
                                return cleanPath.split('/').pop().split('\\').pop() || currentLibraryPath;
                            }
                            color: textPrimary; font.pixelSize: 14; font.bold: true
                            elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        
                        Text {
                            text: currentLibraryPath
                            visible: currentLibraryPath !== ""
                            color: textSecondary; font.pixelSize: 11; font.family: "monospace"
                            elide: Text.ElideMiddle; Layout.fillWidth: true
                        }
                        
                        Button {
                            Layout.fillWidth: true
                            text: currentLibraryPath !== "" ? qsTr("Change Target Disk") : qsTr("Connect Media Layer")
                            onClicked: folderDialog.open()
                            contentItem: Text {
                                text: parent.text; color: accentPrimary; font.bold: true; font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.down ? "#111" : (parent.hovered ? borderGlass : "transparent")
                                radius: 6; implicitHeight: 32; border.color: accentPrimary; border.width: 1
                            }
                        }
                    }
                }
            }
        }
        
        // --- IO Scanning Overlay ---
        Rectangle {
            anchors.centerIn: parent
            width: 250; height: 60
            radius: 30
            color: bgCard
            border.color: accentPrimary; border.width: 1
            visible: mainWindow.isScrapingIO
            z: 99
            
            RowLayout {
                anchors.centerIn: parent
                spacing: 15
                BusyIndicator { running: true; implicitWidth: 26; implicitHeight: 26 }
                Text { text: qsTr("Scanning Storage Arrays..."); color: "white"; font.bold: true; font.pixelSize: 14 }
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
                    
                    // Action Fetch Routine Binding integrated near Search
                    Button {
                        text: qsTr("Fetch Missing Artwork")
                        visible: libraryGames.length > 0
                        
                        property bool fetching: false
                        property int currIndex: 0
                        
                        onClicked: {
                            if (fetching || libraryGames.length === 0) return;
                            fetching = true; currIndex = 0; fetchNext();
                        }
                        function fetchNext() {
                            if (currIndex >= mainWindow.libraryGames.length) {
                                fetching = false; Qt.callLater(refreshGames); return;
                            }
                            let g = mainWindow.libraryGames[currIndex]
                            let extractedId = g.name.substring(0, 11)
                            let artFolder = mainWindow.currentLibraryPath + "/ART"
                            oplLibraryService.startDownloadArtAsync(artFolder, extractedId, g.path)
                            currIndex++;
                            batchTimer.start();
                        }
                        Timer { id: batchTimer; interval: 10; onTriggered: parent.fetchNext() }
                        
                        contentItem: Text {
                            text: parent.fetching ? (qsTr("Network Pulling") + " (" + parent.currIndex + "/" + libraryGames.length + ")") : parent.text
                            color: "white"; font.bold: true; font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.down ? "#111" : (parent.hovered ? borderGlass : "transparent")
                            radius: 8; border.color: accentPrimary; border.width: 1
                            implicitWidth: 180; implicitHeight: 40
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
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
                            cellHeight: 380
                            
                            boundsBehavior: Flickable.StopAtBounds
                            
                            delegate: Item {
                                width: libraryGrid.cellWidth - 20
                                height: libraryGrid.cellHeight - 20
                                
                                Rectangle {
                                    id: cardRect
                                    anchors.fill: parent
                                    color: bgCard
                                    radius: 12
                                    border.color: mouseArea.containsMouse ? cardBorderHover : cardBorderNormal
                                    border.width: 1
                                    
                                    Behavior on border.color { ColorAnimation { duration: 250 } }
                                    
                                    MouseArea {
                                        id: mouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {}
                                    }
                                    
                                    states: State {
                                        name: "hovered"; when: mouseArea.containsMouse
                                        PropertyChanges { target: cardRect; scale: 1.03 }
                                    }
                                    transitions: Transition { NumberAnimation { properties: "scale"; duration: 200; easing.type: Easing.OutBack } }
                                    
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0
                                        
                                        // PS2 Jewel/DVD Case Physical Wrap
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 250
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
                                                    fillMode: Image.Stretch
                                                    visible: status === Image.Ready
                                                    source: "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_COV.png"
                                                    
                                                    onStatusChanged: {
                                                        if (status === Image.Error && source.toString().includes("_COV")) {
                                                            source = "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_ICO.png"
                                                        }
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
                                                
                                                Item { Layout.fillHeight: true }
                                                
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 0
                                                    Text {
                                                        text: {
                                                            let prefix = (extractGameId(modelData.name) || "UNKNOWN").substring(0, 4)
                                                            let regionName = "Unknown Region"
                                                            if (prefix === "SLUS" || prefix === "SCUS") regionName = "NTSC-U"
                                                            else if (prefix === "SLES" || prefix === "SCES") regionName = "PAL"
                                                            else if (prefix === "SLPM" || prefix === "SLPS" || prefix === "SCPS" || prefix === "SCPM") regionName = "NTSC-J"
                                                            return "Region: " + regionName
                                                        }
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                    Text {
                                                        text: "Format: " + (modelData.path.includes("/CD/") ? "CD" : "DVD")
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                    Text {
                                                        text: "Size: " + ((modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) > 0.99 ? (modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) + " GB" : (modelData.stats.size / (1024 * 1024)).toFixed(0) + " MB")
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
                                    border.color: listMouseArea.containsMouse ? cardBorderHover : cardBorderNormal
                                    border.width: 1
                                    
                                    Behavior on border.color { ColorAnimation { duration: 250 } }
                                    
                                    MouseArea {
                                        id: listMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {}
                                    }
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 20
                                        
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
                                                    fillMode: Image.PreserveAspectCrop
                                                    visible: status === Image.Ready
                                                    source: "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_COV.png"
                                                    
                                                    onStatusChanged: {
                                                        if (status === Image.Error && source.toString().includes("_COV")) {
                                                            source = "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_ICO.png"
                                                        }
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
                                                elide: Text.ElideRight; Layout.maximumWidth: parent.width - 20
                                            }
                                            RowLayout {
                                                spacing: 12
                                                
                                                Text {
                                                    text: extractGameId(modelData.name) || "UNKNOWN ID"
                                                    color: accentPrimary
                                                    font.bold: true; font.pixelSize: 11; font.family: "monospace"
                                                }
                                                
                                                Text {
                                                    text: "• " + ((modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) > 0.99 ? (modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) + " GB" : (modelData.stats.size / (1024 * 1024)).toFixed(0) + " MB")
                                                    color: textSecondary; font.pixelSize: 11; font.bold: true; font.family: "Inter"
                                                }
                                                
                                                Text {
                                                    text: "• " + (modelData.path.includes("/CD/") ? "CD" : "DVD")
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
                            
                            // Extractor Top Native Control Overlay
                            Rectangle {
                                Layout.fillWidth: true
                                height: 60
                                radius: 12
                                color: bgCard
                                border.color: borderGlass; border.width: 1
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 15
                                    
                                    Button {
                                        id: massSelectBtn
                                        property bool allSelected: false
                                        text: allSelected ? "Deselect All" : "Select All Available"
                                        onClicked: {
                                            allSelected = !allSelected
                                            let newMap = {}
                                            if (allSelected) {
                                                for (let i = 0; i < importGames.length; i++) newMap[importGames[i].path] = true
                                            }
                                            mainWindow.selectionMap = newMap
                                        }
                                        contentItem: Text { text: parent.text; color: textPrimary; font.bold: true; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                        background: Rectangle { color: parent.hovered ? borderGlass : "transparent"; radius: 6; border.color: borderGlass; implicitWidth: 150; implicitHeight: 36 }
                                    }
                                    
                                    Button {
                                        text: "Add Games"
                                        onClicked: addGamesDialog.open()
                                        contentItem: Text { text: parent.text; color: textPrimary; font.bold: true; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                        background: Rectangle { color: parent.hovered ? borderGlass : "transparent"; radius: 6; border.color: borderGlass; implicitWidth: 100; implicitHeight: 36 }
                                    }
                                    
                                    // Progress Elevated Centerpiece Hook
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 18
                                        radius: 9
                                        color: bgMain
                                        border.color: borderGlass
                                        visible: mainWindow.isBatchExtracting
                                        
                                        Rectangle {
                                            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                            width: mainWindow.extractQueue.length > 0 ? (mainWindow.extractIndex / mainWindow.extractQueue.length) * parent.width : 0
                                            color: accentPrimary
                                            radius: 9
                                            Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.InOutQuad } }
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            text: Math.round((mainWindow.extractIndex / Math.max(1, mainWindow.extractQueue.length)) * 100) + "%"
                                            color: "white"; font.pixelSize: 10; font.bold: true
                                        }
                                    }
                                    
                                    Item { Layout.fillWidth: true; visible: !mainWindow.isBatchExtracting }
                                    
                                    Button {
                                        property int selectedCount: Object.values(mainWindow.selectionMap).filter(v => v === true).length
                                        text: mainWindow.isBatchExtracting ? "Extracting..." : "Process " + selectedCount + " Items"
                                        onClicked: {
                                            if (mainWindow.isBatchExtracting) return;
                                            let queue = []
                                            for (let i = 0; i < importGames.length; i++) {
                                                if (mainWindow.selectionMap[importGames[i].path] === true) queue.push(importGames[i])
                                            }
                                            if (queue.length === 0) return;
                                            
                                            mainWindow.extractQueue = queue
                                            mainWindow.extractIndex = 0
                                            mainWindow.isBatchExtracting = true
                                            mainWindow.batchActiveJobs = 0
                                            mainWindow.extractProcessor()
                                        }
                                        contentItem: Text {
                                            text: parent.text; color: "white"; font.bold: true; font.pixelSize: 14
                                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: parent.down ? "#CC3D3D" : (mainWindow.isBatchExtracting ? borderGlass : accentPrimary)
                                            radius: 12; implicitWidth: 160; implicitHeight: 40
                                        }
                                    }
                                }
                            }
                            
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
                                                        text: "• " + ((modelData.extension.toLowerCase() === ".bin" || modelData.extension.toLowerCase() === ".cue") ? "CD" : "DVD")
                                                        color: accentPrimary; font.pixelSize: 11; font.bold: true
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text {
                                                        text: ((modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) > 0.99 ? (modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) + " GB" : (modelData.stats.size / (1024 * 1024)).toFixed(0) + " MB")
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
                                                        text: "• " + ((modelData.extension.toLowerCase() === ".bin" || modelData.extension.toLowerCase() === ".cue") ? "CD" : "DVD")
                                                        color: accentPrimary; font.pixelSize: 12; font.bold: true
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text {
                                                        text: ((modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) > 0.99 ? (modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) + " GB" : (modelData.stats.size / (1024 * 1024)).toFixed(0) + " MB")
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
