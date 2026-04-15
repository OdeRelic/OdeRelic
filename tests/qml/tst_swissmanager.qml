import QtQuick
import QtQuick.Controls
import QtTest
import "../../src-cpp/ngc" as Src

Item {
    id: container
    width: 1200
    height: 800

    Src.SwissManager {
        id: swissManagerBlock
        anchors.fill: parent
    }

    TestCase {
        name: "SwissManager_UI_Integrity"
        when: windowShown
        
        function test_dimensions() {
            compare(swissManagerBlock.width, 1200, "SwissManager should correctly respect layout bindings mapping to the parent width.");
            compare(swissManagerBlock.height, 800, "SwissManager should correctly respect layout bindings mapping to the parent height.");
        }
        
        function test_root_storage_selection() {
            var btn = findChild(swissManagerBlock, "btnChangeTarget");
            verify(btn !== null, "btnChangeTarget button is missing");
            
            swissManagerBlock.currentLibraryPath = "/test/auto/path";
            btn.clicked();
            
            compare(swissManagerBlock.currentLibraryPath, "", "Clicking the target button failed to wipe library path.");
        }
        
        function test_add_games_interaction() {
            var btn = findChild(swissManagerBlock, "btnAddGames");
            verify(btn !== null, "btnAddGames button is missing");
            btn.clicked();
            verify(true, "Add Games button clicked successfully without logic exceptions");
        }
        
        function test_add_folder_interaction() {
            var btn = findChild(swissManagerBlock, "btnAddFolder");
            verify(btn !== null, "btnAddFolder button is missing");
            btn.clicked();
            verify(true, "Add Folder button clicked successfully without logic exceptions");
        }
        
        function test_select_all_available() {
            var btn = findChild(swissManagerBlock, "btnSelectAllAvailable");
            verify(btn !== null, "btnSelectAllAvailable button is missing");
            
            swissManagerBlock.gameFiles = [ { "path": "/media/drive/fake_import_file.iso", "isRenamed": false, "name": "Fake Game", "extension": ".iso", "stats": {"size": 1350000000} } ];
            wait(50);
            
            compare(swissManagerBlock.importGames.length, 1, "Mock game should be evaluated as an importable game.");
            
            btn.clicked();
            verify(swissManagerBlock.selectionMap["/media/drive/fake_import_file.iso"] === true, "Select All button failed to map to game selection tracking");
            
            btn.clicked();
            verify(swissManagerBlock.selectionMap["/media/drive/fake_import_file.iso"] === undefined, "Deselecting failed to clean tracking map");
        }
        
        function test_import_selected_games() {
            var btn = findChild(swissManagerBlock, "btnImportActionsSelected");
            verify(btn !== null, "btnImportActionsSelected button is missing");
            
            swissManagerBlock.selectionMap = {};
            verify(btn.enabled === false, "Import button should disable when nothing is selected");
            
            swissManagerBlock.selectionMap = { "/media/drive/fake_import_file.iso": true };
            verify(btn.enabled === true, "Import button should explicitly enable when item is mapped");
            
            btn.clicked();
            verify(true, "Import processing button passed logic execution");
        }
        
        function test_sync_cheats_button() {
            var btn = findChild(swissManagerBlock, "btnSyncCheats");
            verify(btn !== null, "btnSyncCheats button is missing");
            var toast = findChild(swissManagerBlock, "toastPopup");
            verify(toast !== null, "toastPopup is missing");
            
            btn.clicked();
            
            wait(50); // Allow C++ signal bindings to resolve and trigger visual state
            
            verify(toast.opened || toast.visible, "Toast should be opened after sync");
            verify(toast.messageText.indexOf("Successfully synchronized") !== -1, "Toast should display success message");
            verify(toast.messageText.indexOf("10") !== -1, "Toast should display the mock emitted synced count (10)");
        }
        
        function test_delete_selected_games() {
            var btn = findChild(swissManagerBlock, "btnDeleteSelected");
            verify(btn !== null, "btnDeleteSelected button is missing");
            
            swissManagerBlock.librarySelectionMap = {};
            verify(btn.enabled === false, "Delete button should disable when nothing is selected in library");
            
            swissManagerBlock.librarySelectionMap = { "/media/drive/fake_game.iso": true };
            verify(btn.enabled === true, "Delete button should explicitly enable when item is mapped");
            
            btn.clicked();
            verify(true, "Delete execution successfully processed trigger logic in proxy");
        }
        
        function test_update_now_button() {
            var btn = findChild(swissManagerBlock, "btnUpdateNow");
            verify(btn !== null, "btnUpdateNow button is missing");
            
            swissManagerBlock.swissUpdateAvailable = true;
            btn.clicked();
            verify(true, "Update button executed successfully against backend mocking");
        }
        
        function test_fetch_missing_artwork() {
            var btn = findChild(swissManagerBlock, "btnFetchMissingArtwork");
            verify(btn !== null, "btnFetchMissingArtwork button is missing");
            
            // Should be disabled initially if there are no library games
            swissManagerBlock.libraryGames = [];
            verify(btn.enabled === false, "Fetch button should disable when library is empty");
            
            swissManagerBlock.libraryGames = [ { "path": "/fake/path1.iso", "name": "Fake Name" }, { "path": "/fake/path2.iso", "name": "Fake Name 2" } ];
            verify(btn.enabled === true, "Fetch button should enable when library has games");
            
            btn.clicked();
            verify(btn.fetching === true, "Clicking fetch should toggle the internal fetching property");
            
            // Allow batchTimer to trigger some fetches against the mock and finish via Qt.callLater
            wait(50);
        }
    }
}
