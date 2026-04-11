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
    property color accentPs1: "#4D9FFF"        // Blue accent for PS1 cards
    property color accentPs1Hover: "#7AB8FF"
    property color cardBorderNormalPs1: "#884D9FFF"
    property color cardBorderHoverPs1: "#FF4D9FFF"
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
    
    property int scanCurrentFolder: 0
    property int scanTotalFolders: 0
    
    property int libraryScanCurrent: 0
    property int libraryScanTotal: 0
    
    property string sortCriteria: "name"
    property bool sortAscending: true
    
    property bool isScrapingIO: false

    // ── PS2 batch state ───────────────────────────────────────────────────────
    property var batchConvertingMap: ({})
    property var batchConvertingNames: ({})
    property int batchActiveJobs: 0
    property int batchMaxJobs: 3
    property bool isBatchExtracting: false
    property var extractQueue: []
    property int extractIndex: 0

    // ── PS1 / POPS state ──────────────────────────────────────────────────────
    property var ps1GameFiles: []
    property var ps1LibraryGames: {
        let q = searchQuery.toLowerCase()
        return ps1GameFiles.filter(function(g) {
            if (!g.isRenamed) return false;
            if (q === "") return true;
            return g.name.toLowerCase().indexOf(q) !== -1;
        })
    }
    property var ps1ImportGames: {
        let q = searchQuery.toLowerCase()
        return ps1GameFiles.filter(function(g) {
            if (g.isRenamed) return false;
            if (q === "") return true;
            return g.name.toLowerCase().indexOf(q) !== -1;
        })
    }
    property var ps1SelectionMap: ({})
    property var ps1BatchConvertingMap: ({})
    property var ps1BatchConvertingNames: ({})
    property int ps1BatchActiveJobs: 0
    property int ps1BatchMaxJobs: 3
    property bool ps1IsBatchExtracting: false
    property var ps1ExtractQueue: []
    property int ps1ExtractIndex: 0
    property bool isScrapingPs1IO: false
    property var popsStatus: ({ hasPopsFolder: false, hasPopstarter: false, hasPopsIox: false, popsPath: "" })
    
    property int activeTargetPercent: 0
    property double activeTargetMbps: 0.0

    Connections {
        target: oplLibraryService
        function onConversionProgress(sourcePath, percent) {}
        function onImportIsoProgress(sourcePath, percent, mbps) {
            mainWindow.activeTargetPercent = percent
            mainWindow.activeTargetMbps = mbps
        }
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
            if (isPs1) {
                let arr = [].concat(mainWindow.ps1GameFiles)
                for (let i = 0; i < files.length; i++) {
                    let exists = false;
                    for (let j = 0; j < arr.length; j++) { if (arr[j].path === files[i].path) { exists = true; break; } }
                    if (!exists) arr.push(files[i]);
                }
                mainWindow.ps1GameFiles = arr
                updatePs1Sort()
            } else {
                let arr = [].concat(mainWindow.gameFiles)
                for (let i = 0; i < files.length; i++) {
                    let exists = false;
                    for (let j = 0; j < arr.length; j++) { if (arr[j].path === files[i].path) { exists = true; break; } }
                    if (!exists) arr.push(files[i]);
                }
                mainWindow.gameFiles = arr
                updateSort()
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

    // ── PS1 Signal Connections ─────────────────────────────────────────────────
    Connections {
        target: oplLibraryService
        function onPs1ConversionProgress(sourcePath, percent) {}
        function onPs1ImportProgress(sourcePath, percent, mbps) {
            mainWindow.activeTargetPercent = percent
            mainWindow.activeTargetMbps = mbps
        }
        function onPs1ArtDownloadProgress(sourcePath, percent) {}

        function onPs1GamesLoaded(dirPath, data) {
            if (dirPath === mainWindow.currentLibraryPath) {
                ps1IsBatchExtracting = false
                isScrapingPs1IO = false
                mainWindow.ps1GameFiles = []
                mainWindow.ps1GameFiles = data.data
                mainWindow.ps1SelectionMap = ({})
                updatePs1Sort()
            }
        }

        function onPs1ArtDownloadFinished(sourcePath, success, message) {
            if (mainWindow.ps1BatchConvertingMap[sourcePath] === true) {
                let newMap = Object.assign({}, mainWindow.ps1BatchConvertingMap)
                delete newMap[sourcePath]
                mainWindow.ps1BatchConvertingMap = newMap
                mainWindow.ps1BatchActiveJobs--
                ps1ExtractProcessor()
            }
        }

        function onPs1ImportFinished(sourcePath, success, destVcdPath, gameId, message) {
            if (mainWindow.ps1BatchConvertingMap[sourcePath] === true) {
                if (success && gameId !== "") {
                    let artFolder = mainWindow.currentLibraryPath + "/ART"
                    oplLibraryService.startDownloadPs1ArtAsync(artFolder, gameId, sourcePath)
                    return;
                } else if (!success) {
                    console.error("PS1 Import Failed: " + message)
                }
                let newMap = Object.assign({}, mainWindow.ps1BatchConvertingMap)
                delete newMap[sourcePath]
                mainWindow.ps1BatchConvertingMap = newMap
                mainWindow.ps1BatchActiveJobs--
                ps1ExtractProcessor()
            }
        }

        function onPs1ConversionFinished(sourcePath, success, destVcdPath, gameId, message) {
            if (mainWindow.ps1BatchConvertingMap[sourcePath] === true) {
                let currentName = mainWindow.ps1BatchConvertingNames[sourcePath]
                if (success && gameId !== "") {
                    // Map the newly generated VCD path to the UI progress tracking so Import phase can update the UI
                    let newMap = Object.assign({}, mainWindow.ps1BatchConvertingMap)
                    newMap[destVcdPath] = true
                    mainWindow.ps1BatchConvertingMap = newMap
                    
                    let newNames = Object.assign({}, mainWindow.ps1BatchConvertingNames)
                    newNames[destVcdPath] = currentName
                    mainWindow.ps1BatchConvertingNames = newNames

                    // Conversion slot transitions into an import slot — keep activeJobs steady
                    // (conversion job ends, import job starts in same slot)
                    oplLibraryService.startImportVcdAsync(destVcdPath, mainWindow.currentLibraryPath, gameId, currentName)
                    return;  // Do NOT decrement — the slot is now used by the import
                } else if (success && gameId === "") {
                    console.error("PS1 Conversion succeeded but Game ID not detected: " + destVcdPath)
                    oplLibraryService.deleteFileAndCue(destVcdPath)
                } else {
                    console.error("PS1 Conversion Failed: " + message)
                }
                let newMap = Object.assign({}, mainWindow.ps1BatchConvertingMap)
                delete newMap[sourcePath]
                mainWindow.ps1BatchConvertingMap = newMap
                mainWindow.ps1BatchActiveJobs--
                ps1ExtractProcessor()
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
                text: qsTr("Scanning Storage...")
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
        height: 140
        modal: true
        closePolicy: Popup.NoAutoClose
        
        property bool isExtracting: mainWindow.isBatchExtracting || mainWindow.ps1IsBatchExtracting
        onIsExtractingChanged: {
            if (isExtracting) open(); else close();
        }

        property bool isPs1: mainWindow.ps1IsBatchExtracting
        property int currentIndex: isPs1 ? mainWindow.ps1ExtractIndex : mainWindow.extractIndex
        property int totalItems: isPs1 ? mainWindow.ps1ExtractQueue.length : mainWindow.extractQueue.length
        property string actionName: isPs1 ? "Converting PS1 Titles..." : "Importing PS2 games..."
        property color activeColor: isPs1 ? accentPs1 : accentPrimary

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
                    text: mainWindow.activeTargetMbps.toFixed(2) + " MB/s"
                    color: "#00E676"; font.pixelSize: 10; font.bold: true
                }
            }
            
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                Layout.topMargin: 10
                text: qsTr("Cancel Active Import")
                onClicked: {
                    oplLibraryService.cancelAllImports()
                    importProgressOverlay.close()
                }
                contentItem: Text {
                    text: parent.text; color: textSecondary; font.pixelSize: 11; font.bold: true
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.down ? "#111" : (parent.hovered ? borderGlass : "transparent")
                    radius: 6; implicitHeight: 28; border.color: borderGlass; border.width: 1
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
                text: qsTr("This drive cannot be used with OPL.")
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
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Select OPL Root Folder")
        onAccepted: {
            let cleanPath = oplLibraryService.urlToLocalFile(selectedFolder.toString())
            let structureCheck = oplLibraryService.checkOplFolder(cleanPath)
            
            if (!structureCheck.isFormatCorrect || !structureCheck.isPartitionCorrect) {
                let err = []
                if (!structureCheck.isFormatCorrect) err.push("• FAT32 or exFAT required")
                if (!structureCheck.isPartitionCorrect) err.push("• MBR partition table required")
                formatErrorPopup.errorDetails = err.join("\n")
                formatErrorPopup.open()
                return
            }

            mainWindow.currentLibraryPath = cleanPath
            mainWindow.isScrapingIO = true
            mainWindow.isScrapingPs1IO = true
            refreshGames()
            refreshPs1Games()
            mainWindow.popsStatus = oplLibraryService.checkPopsFolder(cleanPath)
            currentTabIndex = 0
        }
    }

    FileDialog {
        id: addPs1GamesDialog
        title: qsTr("Select PS1 Games to Import")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["PS1 Images (*.bin *.cue *.img *.vcd)", "All files (*)"]
        onAccepted: {
            let urls = []
            for (let i = 0; i < selectedFiles.length; i++) urls.push(selectedFiles[i].toString())
            folderScanOverlay.open()
            oplLibraryService.scanExternalFilesAsync(urls, true)
        }
    }
    
    FileDialog {
        id: addGamesDialog
        title: qsTr("Select Games to Import")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["PS2 Images (*.iso *.bin *.cue *.zso)", "All files (*)"]
        onAccepted: {
            let urls = []
            for (let i = 0; i < selectedFiles.length; i++) urls.push(selectedFiles[i].toString())
            folderScanOverlay.open()
            oplLibraryService.scanExternalFilesAsync(urls, false)
        }
    }

    FolderDialog {
        id: addGamesFolderDialog
        title: qsTr("Select Folder to Import PS2 Games")
        onAccepted: {
            folderScanOverlay.open()
            oplLibraryService.scanExternalFilesAsync([selectedFolder.toString()], false)
        }
    }
    
    FolderDialog {
        id: addPs1GamesFolderDialog
        title: qsTr("Select Folder to Import PS1 Games")
        onAccepted: {
            folderScanOverlay.open()
            oplLibraryService.scanExternalFilesAsync([selectedFolder.toString()], true)
        }
    }
    
    function refreshGames() {
        if (currentLibraryPath !== "") {
            oplLibraryService.startGetGamesFilesAsync(currentLibraryPath)
        }
    }

    function refreshPs1Games() {
        if (currentLibraryPath !== "") {
            oplLibraryService.startGetPs1GamesAsync(currentLibraryPath)
        }
    }

    function ps1ExtractProcessor() {
        if (!ps1IsBatchExtracting) return;

        while (mainWindow.ps1BatchActiveJobs < mainWindow.ps1BatchMaxJobs && ps1ExtractIndex < ps1ExtractQueue.length) {
            let g = ps1ExtractQueue[ps1ExtractIndex]
            ps1ExtractIndex++;
            let ext = g.extension.toLowerCase()

            if (ext === ".bin" || ext === ".cue") {
                let tempVcdPath = mainWindow.currentLibraryPath + "/.orbit_ps1_temp_" + Math.floor(Math.random() * 1000000) + ".vcd"
                let newMap = Object.assign({}, mainWindow.ps1BatchConvertingMap)
                newMap[g.path] = true
                mainWindow.ps1BatchConvertingMap = newMap
                let newNames = Object.assign({}, mainWindow.ps1BatchConvertingNames)
                newNames[g.path] = g.name
                mainWindow.ps1BatchConvertingNames = newNames
                mainWindow.ps1BatchActiveJobs++;
                oplLibraryService.startConvertBinToVcd(g.path, tempVcdPath)
                continue;
            }

            // Already a .vcd — import async (the C++ import thread detects the ID internally)
            let newMap = Object.assign({}, mainWindow.ps1BatchConvertingMap)
            newMap[g.path] = true
            mainWindow.ps1BatchConvertingMap = newMap
            let newNames = Object.assign({}, mainWindow.ps1BatchConvertingNames)
            newNames[g.path] = g.name
            mainWindow.ps1BatchConvertingNames = newNames
            mainWindow.ps1BatchActiveJobs++;
            // Pass empty gameId: startImportVcdAsync will detect + emit it in the signal
            oplLibraryService.startImportVcdAsync(g.path, mainWindow.currentLibraryPath, "", g.name)
            continue;
        }

        if (mainWindow.ps1BatchActiveJobs === 0 && ps1ExtractIndex >= ps1ExtractQueue.length) {
            ps1IsBatchExtracting = false
            mainWindow.ps1SelectionMap = ({})
            Qt.callLater(refreshPs1Games)
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
    
    function updatePs1Sort() {
        let sorted = []
        for (let i = 0; i < mainWindow.ps1GameFiles.length; i++) {
            sorted.push(mainWindow.ps1GameFiles[i])
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
        mainWindow.ps1GameFiles = []
        mainWindow.ps1GameFiles = sorted
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
                
                // ── PS2 Section Divider ───────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    spacing: 8
                    Rectangle { height: 1; Layout.fillWidth: true; color: accentPrimary; opacity: 0.3 }
                    Text {
                        text: "PS2"
                        color: accentPrimary; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2
                        opacity: 0.7
                    }
                    Rectangle { height: 1; Layout.fillWidth: true; color: accentPrimary; opacity: 0.3 }
                }

                // Navigation Buttons Array
                Button {
                    Layout.fillWidth: true
                    text: qsTr("PS2 Library") + "  (" + libraryGames.length + ")"
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
                    text: qsTr("PS2 Import") + "  (" + importGames.length + ")"
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

                // ── PS1 Section Divider ───────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    Layout.bottomMargin: 4
                    spacing: 8
                    Rectangle { height: 1; Layout.fillWidth: true; color: accentPs1; opacity: 0.3 }
                    Text {
                        text: "PS1"
                        color: accentPs1; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2
                        opacity: 0.7
                    }
                    Rectangle { height: 1; Layout.fillWidth: true; color: accentPs1; opacity: 0.3 }
                }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("PS1 Library") + "  (" + ps1LibraryGames.length + ")"
                    onClicked: { currentTabIndex = 2; Qt.callLater(refreshPs1Games) }
                    contentItem: Text {
                        text: parent.text; color: currentTabIndex === 2 ? "white" : textSecondary
                        font.bold: true; font.pixelSize: 14; horizontalAlignment: Text.AlignLeft
                        leftPadding: 20; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: currentTabIndex === 2 ? "#334D9FFF" : "transparent"
                        radius: 8; implicitHeight: 46
                        border.color: currentTabIndex === 2 ? accentPs1 : (parent.hovered ? borderGlass : "transparent")
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("PS1 Import") + "  (" + ps1ImportGames.length + ")"
                    onClicked: currentTabIndex = 3
                    contentItem: Text {
                        text: parent.text; color: currentTabIndex === 3 ? "white" : textSecondary
                        font.bold: true; font.pixelSize: 14; horizontalAlignment: Text.AlignLeft
                        leftPadding: 20; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: currentTabIndex === 3 ? "#334D9FFF" : "transparent"
                        radius: 8; implicitHeight: 46
                        border.color: currentTabIndex === 3 ? accentPs1 : (parent.hovered ? borderGlass : "transparent")
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                }

                // POPS prerequisite warning banner (interactive)
                Rectangle {
                    id: popsWarnBanner
                    Layout.fillWidth: true
                    implicitHeight: popsWarnCol.implicitHeight + 16
                    radius: 8
                    color: "#1A4D9FFF"
                    border.color: accentPs1; border.width: 1
                    visible: (currentTabIndex === 2 || currentTabIndex === 3) &&
                             currentLibraryPath !== "" &&
                             (!mainWindow.popsStatus.hasPopstarter || !mainWindow.popsStatus.hasPopsIox)

                    FileDialog {
                        id: popstarterPicker
                        title: qsTr("Select POPSTARTER.ELF")
                        fileMode: FileDialog.OpenFile
                        nameFilters: ["ELF Files (*.ELF *.elf)", "All files (*)"]
                        onAccepted: {
                            let path = oplLibraryService.urlToLocalFile(selectedFile.toString())
                            let res = oplLibraryService.copyFileToPopsFolder(path, mainWindow.currentLibraryPath)
                            if (res.success) mainWindow.popsStatus = oplLibraryService.checkPopsFolder(mainWindow.currentLibraryPath)
                        }
                    }
                    FileDialog {
                        id: popsIoxPicker
                        title: qsTr("Select POPS_IOX.PAK")
                        fileMode: FileDialog.OpenFile
                        nameFilters: ["PAK Files (*.PAK *.pak)", "All files (*)"]
                        onAccepted: {
                            let path = oplLibraryService.urlToLocalFile(selectedFile.toString())
                            let res = oplLibraryService.copyFileToPopsFolder(path, mainWindow.currentLibraryPath)
                            if (res.success) mainWindow.popsStatus = oplLibraryService.checkPopsFolder(mainWindow.currentLibraryPath)
                        }
                    }

                    ColumnLayout {
                        id: popsWarnCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "⚠ POPS Setup Incomplete"
                            color: accentPs1; font.bold: true; font.pixelSize: 11
                            Layout.fillWidth: true; wrapMode: Text.Wrap
                        }

                        RowLayout {
                            visible: !mainWindow.popsStatus.hasPopstarter
                            Layout.fillWidth: true; spacing: 6
                            Text {
                                text: "Missing: POPSTARTER.ELF"
                                color: textSecondary; font.pixelSize: 10
                                Layout.fillWidth: true; wrapMode: Text.Wrap
                            }
                            Rectangle {
                                implicitWidth: 38; implicitHeight: 22; radius: 4
                                color: addElfHover.containsMouse ? "#224D9FFF" : "transparent"
                                border.color: accentPs1; border.width: 1
                                Text { anchors.centerIn: parent; text: qsTr("Add"); color: accentPs1; font.bold: true; font.pixelSize: 10 }
                                MouseArea { id: addElfHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: popstarterPicker.open() }
                            }
                        }

                        RowLayout {
                            visible: !mainWindow.popsStatus.hasPopsIox
                            Layout.fillWidth: true; spacing: 6
                            Text {
                                text: "Missing: POPS_IOX.PAK"
                                color: textSecondary; font.pixelSize: 10
                                Layout.fillWidth: true; wrapMode: Text.Wrap
                            }
                            Rectangle {
                                implicitWidth: 38; implicitHeight: 22; radius: 4
                                color: addPakHover.containsMouse ? "#224D9FFF" : "transparent"
                                border.color: accentPs1; border.width: 1
                                Text { anchors.centerIn: parent; text: qsTr("Add"); color: accentPs1; font.bold: true; font.pixelSize: 10 }
                                MouseArea { id: addPakHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: popsIoxPicker.open() }
                            }
                        }

                        Text {
                            text: "→ " + mainWindow.popsStatus.popsPath
                            color: textSecondary; font.pixelSize: 9; font.family: "monospace"
                            Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.6
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
            
            property bool isScraping: mainWindow.isScrapingIO || mainWindow.isScrapingPs1IO
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
                    text: qsTr("Scanning Storage Arrays...")
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
                                                    asynchronous: true
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
                                                        text: "Version: " + (modelData.version ? "v" + modelData.version : "v1.00")
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
                                                    asynchronous: true
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
                                        property int selectedCount: Object.values(mainWindow.selectionMap).filter(v => v === true).length
                                        text: mainWindow.isBatchExtracting ? "Importing..." : "Process " + selectedCount + " Items"
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
                                    
                                    Item { Layout.fillWidth: true }
                                    
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

                                    Button {
                                        text: "Add Folder"
                                        onClicked: addGamesFolderDialog.open()
                                        contentItem: Text { text: parent.text; color: textPrimary; font.bold: true; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                        background: Rectangle { color: parent.hovered ? borderGlass : "transparent"; radius: 6; border.color: borderGlass; implicitWidth: 100; implicitHeight: 36 }
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

                    // ── TAB 2: PS1 Library ────────────────────────────────────
                    Rectangle {
                        color: "transparent"

                        GridView {
                            id: ps1LibraryGrid
                            visible: mainWindow.isGridView
                            anchors.fill: parent
                            clip: true
                            model: mainWindow.ps1LibraryGames
                            cellWidth: 215
                            cellHeight: 340
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Item {
                                width: ps1LibraryGrid.cellWidth - 20
                                height: ps1LibraryGrid.cellHeight - 20

                                Rectangle {
                                    id: ps1CardRect
                                    anchors.fill: parent
                                    color: bgCard
                                    radius: 12
                                    border.color: ps1CardMouse.containsMouse ? cardBorderHoverPs1 : cardBorderNormalPs1
                                    border.width: 1
                                    Behavior on border.color { ColorAnimation { duration: 250 } }

                                    MouseArea {
                                        id: ps1CardMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {}
                                    }

                                    states: State {
                                        name: "hovered"; when: ps1CardMouse.containsMouse
                                        PropertyChanges { target: ps1CardRect; scale: 1.03 }
                                    }
                                    transitions: Transition { NumberAnimation { properties: "scale"; duration: 200; easing.type: Easing.OutBack } }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        // PS1 Jewel Case
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 195
                                            Layout.margins: 8
                                            Layout.bottomMargin: 0
                                            color: "#181A20"
                                            radius: 3
                                            border.color: "#080A10"
                                            border.width: 1

                                            Rectangle {
                                                anchors.fill: parent; radius: 3
                                                border.color: "#1E3A5A"; border.width: 1; color: "transparent"; z: 2
                                            }

                                            // Blue spine for PS1
                                            Rectangle {
                                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                                width: 8; color: "#0A1829"; radius: 2; z: 2
                                                Rectangle {
                                                    anchors.left: parent.left; anchors.leftMargin: 2
                                                    anchors.top: parent.top; anchors.bottom: parent.bottom
                                                    width: 1; color: "#223355"
                                                }
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                anchors.margins: 2; anchors.leftMargin: 10
                                                clip: true; radius: 1; color: "#0F101A"

                                                // PS1 badge
                                                Rectangle {
                                                    anchors.right: parent.right; anchors.top: parent.top
                                                    anchors.margins: 4
                                                    width: 32; height: 18; radius: 4
                                                    color: accentPs1; opacity: 0.9; z: 5
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: "PS1"; color: "white"
                                                        font.bold: true; font.pixelSize: 9
                                                    }
                                                }

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "VCD"
                                                    color: accentPs1
                                                    font.bold: true; font.pixelSize: 24; opacity: 0.5
                                                    visible: ps1ArtImg.status !== Image.Ready
                                                }

                                                Image {
                                                    id: ps1ArtImg
                                                    anchors.fill: parent
                                                    asynchronous: true
                                                    fillMode: Image.Stretch
                                                    visible: status === Image.Ready
                                                    source: "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_COV.png"
                                                    onStatusChanged: {
                                                        if (status === Image.Error && source.toString().includes("_COV")) {
                                                            source = "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_ICO.png"
                                                        }
                                                    }
                                                }

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

                                        Item {
                                            Layout.fillWidth: true; Layout.fillHeight: true
                                            ColumnLayout {
                                                anchors.fill: parent; anchors.margins: 12; spacing: 4
                                                Text {
                                                    text: {
                                                        let gid = extractGameId(modelData.name)
                                                        if (gid && modelData.name.startsWith(gid)) return modelData.name.substring(gid.length + 1)
                                                        return modelData.name
                                                    }
                                                    color: textPrimary
                                                    font.bold: true; font.pixelSize: 14; font.family: "Inter"
                                                    elide: Text.ElideRight; Layout.fillWidth: true; wrapMode: Text.Wrap
                                                    maximumLineCount: 2; Layout.alignment: Qt.AlignTop
                                                }
                                                Item { Layout.fillHeight: true }
                                                ColumnLayout {
                                                    Layout.fillWidth: true; spacing: 0
                                                    Text {
                                                        text: extractGameId(modelData.name) || "UNKNOWN ID"
                                                        color: accentPs1; font.bold: true; font.pixelSize: 11; font.family: "monospace"
                                                    }
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
                                                        text: "Format: PS1 VCD"
                                                        color: textSecondary; font.pixelSize: 11; font.family: "Inter"
                                                    }
                                                    Text {
                                                        text: "Version: " + (modelData.version ? "v" + modelData.version : "v1.00")
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
                            id: ps1LibraryList
                            visible: !mainWindow.isGridView
                            anchors.fill: parent
                            clip: true
                            model: mainWindow.ps1LibraryGames
                            spacing: 12
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Item {
                                width: ps1LibraryList.width - 20; height: 72
                                Rectangle {
                                    anchors.fill: parent
                                    color: bgCard; radius: 12
                                    border.color: ps1LstMouse.containsMouse ? cardBorderHoverPs1 : cardBorderNormalPs1
                                    border.width: 1
                                    Behavior on border.color { ColorAnimation { duration: 250 } }
                                    MouseArea { id: ps1LstMouse; anchors.fill: parent; hoverEnabled: true }
                                    RowLayout {
                                        anchors.fill: parent; anchors.margins: 12; spacing: 20
                                        Rectangle {
                                            width: 48; height: 48; radius: 3
                                            color: "#181A20"; border.color: accentPs1; border.width: 1
                                            Rectangle {
                                                anchors.fill: parent; anchors.margins: 1; anchors.leftMargin: 3
                                                clip: true; radius: 1; color: "#0F0F1A"
                                                Text { anchors.centerIn: parent; text: "VCD"; color: accentPs1; font.bold: true; font.pixelSize: 11; visible: ps1LstArt.status !== Image.Ready }
                                                Image {
                                                    id: ps1LstArt; anchors.fill: parent; asynchronous: true; fillMode: Image.PreserveAspectCrop; visible: status === Image.Ready
                                                    source: "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_COV.png"
                                                    onStatusChanged: { if (status === Image.Error && source.toString().includes("_COV")) source = "file://" + mainWindow.currentLibraryPath + "/ART/" + extractGameId(modelData.name) + "_ICO.png" }
                                                }
                                                Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0.0; color: "#33FFFFFF" } GradientStop { position: 0.2; color: "transparent" } GradientStop { position: 1.0; color: "#55000000" } } }
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true; spacing: 6
                                            Text {
                                                text: { let gid = extractGameId(modelData.name); if (gid && modelData.name.startsWith(gid)) return modelData.name.substring(gid.length + 1); return modelData.name }
                                                color: textPrimary; font.bold: true; font.pixelSize: 17; font.family: "Inter"
                                                elide: Text.ElideRight; Layout.maximumWidth: parent.width - 20
                                            }
                                            RowLayout {
                                                spacing: 12
                                                Text { text: extractGameId(modelData.name) || "UNKNOWN ID"; color: accentPs1; font.bold: true; font.pixelSize: 11; font.family: "monospace" }
                                                Text { text: "• " + ((modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) > 0.99 ? (modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) + " GB" : (modelData.stats.size / (1024 * 1024)).toFixed(0) + " MB"); color: textSecondary; font.pixelSize: 11; font.bold: true; font.family: "Inter" }
                                                Text { text: "• VCD"; color: textSecondary; font.pixelSize: 11; font.bold: true }
                                                Text { text: "• " + (modelData.version ? "v" + modelData.version : "v1.00"); color: textSecondary; font.pixelSize: 11; font.bold: true }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Scanning overlay
                        Rectangle {
                            anchors.centerIn: parent
                            width: 260; height: 60; radius: 30
                            color: bgCard; border.color: accentPs1; border.width: 1
                            visible: mainWindow.isScrapingPs1IO; z: 99
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 15
                                BusyIndicator { running: true; implicitWidth: 26; implicitHeight: 26 }
                                Text { text: qsTr("Scanning POPS folder..."); color: "white"; font.bold: true; font.pixelSize: 14 }
                            }
                        }
                    } // End TAB 2

                    // ── TAB 3: PS1 Import Operations ──────────────────────────
                    Rectangle {
                        color: "transparent"

                        ColumnLayout {
                            anchors.fill: parent; spacing: 15

                            // Top control bar
                            Rectangle {
                                Layout.fillWidth: true; height: 60; radius: 12
                                color: bgCard; border.color: borderGlass; border.width: 1

                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 15; spacing: 15

                                    Button {
                                        property int selectedCount: Object.values(mainWindow.ps1SelectionMap).filter(v => v === true).length
                                        text: mainWindow.ps1IsBatchExtracting ? qsTr("Converting...") : qsTr("Process ") + selectedCount + qsTr(" Items")
                                        onClicked: {
                                            if (mainWindow.ps1IsBatchExtracting) return;
                                            let queue = []
                                            for (let i = 0; i < ps1ImportGames.length; i++) {
                                                if (mainWindow.ps1SelectionMap[ps1ImportGames[i].path] === true) queue.push(ps1ImportGames[i])
                                            }
                                            if (queue.length === 0) return;
                                            mainWindow.ps1ExtractQueue = queue
                                            mainWindow.ps1ExtractIndex = 0
                                            mainWindow.ps1IsBatchExtracting = true
                                            mainWindow.ps1BatchActiveJobs = 0
                                            mainWindow.ps1ExtractProcessor()
                                        }
                                        contentItem: Text {
                                            text: parent.text; color: "white"; font.bold: true; font.pixelSize: 14
                                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: parent.down ? "#1A4080" : (mainWindow.ps1IsBatchExtracting ? borderGlass : accentPs1)
                                            radius: 12; implicitWidth: 160; implicitHeight: 40
                                        }
                                    }
                                    
                                    Item { Layout.fillWidth: true }
                                    
                                    Button {
                                        id: ps1MassSelectBtn
                                        property bool allSelected: false
                                        text: allSelected ? qsTr("Deselect All") : qsTr("Select All Available")
                                        onClicked: {
                                            allSelected = !allSelected
                                            let newMap = {}
                                            if (allSelected) { for (let i = 0; i < ps1ImportGames.length; i++) newMap[ps1ImportGames[i].path] = true }
                                            mainWindow.ps1SelectionMap = newMap
                                        }
                                        contentItem: Text { text: parent.text; color: textPrimary; font.bold: true; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                        background: Rectangle { color: parent.hovered ? borderGlass : "transparent"; radius: 6; border.color: borderGlass; implicitWidth: 150; implicitHeight: 36 }
                                    }

                                    Button {
                                        text: qsTr("Add PS1 Games")
                                        onClicked: addPs1GamesDialog.open()
                                        contentItem: Text { text: parent.text; color: accentPs1; font.bold: true; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                        background: Rectangle { color: parent.hovered ? "#112040" : "transparent"; radius: 6; border.color: accentPs1; border.width: 1; implicitWidth: 120; implicitHeight: 36 }
                                    }

                                    Button {
                                        text: qsTr("Add Folder")
                                        onClicked: addPs1GamesFolderDialog.open()
                                        contentItem: Text { text: parent.text; color: accentPs1; font.bold: true; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                                        background: Rectangle { color: parent.hovered ? "#112040" : "transparent"; radius: 6; border.color: accentPs1; border.width: 1; implicitWidth: 100; implicitHeight: 36 }
                                    }
                                }
                            }

                            // PS1 Import Grid
                            GridView {
                                id: ps1ImportGrid
                                visible: mainWindow.isGridView
                                Layout.fillWidth: true; Layout.fillHeight: true
                                clip: true
                                model: mainWindow.ps1ImportGames
                                cellWidth: 280; cellHeight: 120
                                boundsBehavior: Flickable.StopAtBounds

                                delegate: Item {
                                    width: 260; height: 100

                                    Rectangle {
                                        id: ps1ImpCard
                                        anchors.fill: parent
                                        color: bgCard; radius: 12
                                        border.color: mainWindow.ps1SelectionMap[modelData.path] === true ? cardBorderHoverPs1 : cardBorderNormalPs1
                                        border.width: mainWindow.ps1SelectionMap[modelData.path] === true ? 2 : 1
                                        Behavior on border.color { ColorAnimation { duration: 150 } }

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                let newMap = Object.assign({}, mainWindow.ps1SelectionMap)
                                                newMap[modelData.path] = !newMap[modelData.path]
                                                mainWindow.ps1SelectionMap = newMap
                                            }
                                        }

                                        RowLayout {
                                            anchors.fill: parent; anchors.margins: 12; spacing: 15

                                            Rectangle {
                                                width: 22; height: 22; radius: 11
                                                border.color: mainWindow.ps1SelectionMap[modelData.path] === true ? accentPs1 : borderGlass
                                                border.width: 2
                                                color: mainWindow.ps1SelectionMap[modelData.path] === true ? accentPs1 : "transparent"
                                                Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.bold: true; font.pixelSize: 14; visible: mainWindow.ps1SelectionMap[modelData.path] === true }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true; spacing: 4
                                                Text {
                                                    text: modelData.name; color: textPrimary
                                                    font.bold: true; font.pixelSize: 14; font.family: "Inter"
                                                    elide: Text.ElideRight; Layout.fillWidth: true
                                                    maximumLineCount: 2; wrapMode: Text.Wrap
                                                }
                                                Item { Layout.fillHeight: true }
                                                RowLayout {
                                                    spacing: 10; Layout.fillWidth: true
                                                    Text { text: modelData.extension.toUpperCase().replace(".", ""); color: textSecondary; font.pixelSize: 11; font.bold: true }
                                                    Text { text: "• PS1"; color: accentPs1; font.pixelSize: 11; font.bold: true }
                                                    Item { Layout.fillWidth: true }
                                                    Text { text: ((modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) > 0.99 ? (modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) + " GB" : (modelData.stats.size / (1024 * 1024)).toFixed(0) + " MB"); color: textSecondary; font.pixelSize: 11; font.family: "Inter" }
                                                }
                                            }
                                        }

                                        // Converting spinner
                                        Rectangle {
                                            anchors.fill: parent; color: "#EA0A1929"; radius: 12
                                            visible: mainWindow.ps1BatchConvertingMap[modelData.path] === true; z: 50
                                            ColumnLayout {
                                                anchors.centerIn: parent; spacing: 8
                                                Text { text: qsTr("Building VCD..."); color: accentPs1; font.bold: true; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter }
                                                Rectangle {
                                                    width: 140; height: 4; radius: 2; color: borderGlass
                                                    Rectangle {
                                                        width: 40; height: parent.height; radius: 2; color: accentPs1
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

                            // PS1 Import List View
                            ListView {
                                id: ps1ImportList
                                visible: !mainWindow.isGridView
                                Layout.fillWidth: true; Layout.fillHeight: true
                                clip: true
                                model: mainWindow.ps1ImportGames
                                boundsBehavior: Flickable.StopAtBounds
                                spacing: 8

                                delegate: Item {
                                    width: ps1ImportList.width - 20; height: 70

                                    Rectangle {
                                        anchors.fill: parent; color: bgCard; radius: 12
                                        border.color: mainWindow.ps1SelectionMap[modelData.path] === true ? cardBorderHoverPs1 : cardBorderNormalPs1
                                        border.width: mainWindow.ps1SelectionMap[modelData.path] === true ? 2 : 1
                                        Behavior on border.color { ColorAnimation { duration: 150 } }

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                let newMap = Object.assign({}, mainWindow.ps1SelectionMap)
                                                newMap[modelData.path] = !newMap[modelData.path]
                                                mainWindow.ps1SelectionMap = newMap
                                            }
                                        }

                                        RowLayout {
                                            anchors.fill: parent; anchors.margins: 15; spacing: 20

                                            Rectangle {
                                                width: 24; height: 24; radius: 12
                                                border.color: mainWindow.ps1SelectionMap[modelData.path] === true ? accentPs1 : borderGlass
                                                border.width: 2
                                                color: mainWindow.ps1SelectionMap[modelData.path] === true ? accentPs1 : "transparent"
                                                Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.bold: true; font.pixelSize: 15; visible: mainWindow.ps1SelectionMap[modelData.path] === true }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; spacing: 2
                                                Text { text: modelData.name; color: textPrimary; font.bold: true; font.pixelSize: 15; font.family: "Inter"; elide: Text.ElideRight; Layout.fillWidth: true }
                                                RowLayout {
                                                    spacing: 12; Layout.fillWidth: true
                                                    Text { text: modelData.extension.toUpperCase().replace(".", ""); color: textSecondary; font.pixelSize: 12; font.bold: true }
                                                    Text { text: "• PS1"; color: accentPs1; font.pixelSize: 12; font.bold: true }
                                                    Item { Layout.fillWidth: true }
                                                    Text { text: ((modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) > 0.99 ? (modelData.stats.size / (1024 * 1024 * 1024)).toFixed(2) + " GB" : (modelData.stats.size / (1024 * 1024)).toFixed(0) + " MB"); color: textSecondary; font.pixelSize: 12; font.family: "Inter" }
                                                }
                                            }
                                        }

                                        // Converting spinner
                                        Rectangle {
                                            anchors.fill: parent; color: "#EA0A1929"; radius: 12
                                            visible: mainWindow.ps1BatchConvertingMap[modelData.path] === true; z: 50
                                            RowLayout {
                                                anchors.centerIn: parent; spacing: 15
                                                Text { text: qsTr("Building VCD..."); color: accentPs1; font.bold: true; font.pixelSize: 13 }
                                                Rectangle {
                                                    width: 140; height: 6; radius: 3; color: borderGlass
                                                    Rectangle {
                                                        width: 40; height: parent.height; radius: 3; color: accentPs1
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
                    } // End TAB 3
                }
            }
        }
    }
}
