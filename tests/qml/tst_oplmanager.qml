import QtQuick
import QtQuick.Controls
import QtTest
import "../../src-cpp/ps2" as Src

Item {
    id: container
    width: 1200
    height: 800

    Src.OplManager {
        id: oplManagerBlock
        anchors.fill: parent
    }

    TestCase {
        name: "OplManager_UI_Integrity"
        when: windowShown
        
        function test_dimensions() {
            compare(oplManagerBlock.width, 1200, "OplManager should correctly respect layout bindings mapping to the parent width.");
            compare(oplManagerBlock.height, 800, "OplManager should correctly respect layout bindings mapping to the parent height.");
        }
        
        function test_root_storage_selection() {
            var btn = findChild(oplManagerBlock, "btnChangeTarget");
            verify(btn !== null, "btnChangeTarget button is missing");
            
            oplManagerBlock.currentLibraryPath = "/test/auto/path";
            btn.clicked();
            
            compare(oplManagerBlock.currentLibraryPath, "", "Clicking the target button failed to wipe library path.");
        }
        
        function test_ps2_library_actions() {
            // Delete Selected
            var btnDelete = findChild(oplManagerBlock, "btnPs2DeleteSelected");
            verify(btnDelete !== null, "btnPs2DeleteSelected button is missing");
            
            oplManagerBlock.selectionMap = {};
            verify(btnDelete.enabled === false, "Delete button should disable without items");
            oplManagerBlock.selectionMap = { "/test/game.iso": true };
            oplManagerBlock.libraryGames = [ { "path": "/test/game.iso", "name": "Test Game", "extension": ".iso", "stats": {"size": 4350000000} } ];
            // Force property map mapping logic
            verify(btnDelete.enabled === true, "Delete button should unlock with valid items");
            btnDelete.clicked();
            
            // Fetch Artwork
            var btnFetch = findChild(oplManagerBlock, "btnPs2FetchArt");
            verify(btnFetch !== null, "btnPs2FetchArt button is missing");
            verify(btnFetch.enabled === true, "Fetch button should unlock when library games > 0");
            btnFetch.clicked();
            verify(btnFetch.fetching === true, "Clicking fetch should toggle the internal fetching property");
            wait(50); 
        }

        function test_ps2_dvd_import_flow() {
            var btnSelectAll = findChild(oplManagerBlock, "btnPs2SelectAll");
            verify(btnSelectAll !== null, "btnPs2SelectAll button is missing");
            
            oplManagerBlock.importGames = [ { "path": "/media/drive/fake_import_file.iso", "isRenamed": false, "name": "Fake Game", "extension": ".iso", "stats": {"size": 1350000000} } ];
            wait(50);
            
            btnSelectAll.clicked();
            verify(oplManagerBlock.selectionMap["/media/drive/fake_import_file.iso"] === true, "Select All button failed to map to game selection tracking");
            btnSelectAll.clicked();
            verify(oplManagerBlock.selectionMap["/media/drive/fake_import_file.iso"] === undefined, "Deselecting failed to clean tracking map");
            
            var btnImport = findChild(oplManagerBlock, "btnPs2ImportSelected");
            verify(btnImport !== null, "btnPs2ImportSelected button is missing");
            
            oplManagerBlock.selectionMap = { "/media/drive/fake_import_file.iso": true };
            verify(btnImport.enabled === true, "Import button should explicitly enable when item is mapped");
            btnImport.clicked();
        }

        function test_ps1_import_flow() {
            var btnSelectAll = findChild(oplManagerBlock, "btnPs1SelectAll");
            verify(btnSelectAll !== null, "btnPs1SelectAll button is missing");
            
            oplManagerBlock.ps1ImportGames = [ { "path": "/media/drive/fake_ps1.bin", "isRenamed": false, "name": "Fake PS1", "extension": ".bin", "stats": {"size": 650000000} } ];
            wait(50);
            
            btnSelectAll.clicked();
            verify(oplManagerBlock.ps1SelectionMap["/media/drive/fake_ps1.bin"] === true, "PS1 Select All button failed mapping");
            btnSelectAll.clicked();
            verify(oplManagerBlock.ps1SelectionMap["/media/drive/fake_ps1.bin"] === undefined, "PS1 Deselecting failed");
            
            var btnImport = findChild(oplManagerBlock, "btnPs1ImportSelected");
            verify(btnImport !== null, "btnPs1ImportSelected button is missing");
            
            oplManagerBlock.ps1SelectionMap = { "/media/drive/fake_ps1.bin": true };
            verify(btnImport.enabled === true, "Import button should explicitly enable when item is mapped");
            btnImport.clicked();
        }
    }
}
