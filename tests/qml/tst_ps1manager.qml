import QtQuick
import QtQuick.Controls
import QtTest
import "../../src-cpp/psx" as Src

Item {
    id: container
    width: 1200
    height: 800

    Src.PS1Manager {
        id: ps1ManagerBlock
        anchors.fill: parent
    }

    TestCase {
        name: "PS1Manager_UI_Integrity"
        when: windowShown
        
        function test_dimensions() {
            compare(ps1ManagerBlock.width, 1200, "PS1Manager should correctly respect layout bindings mapping to the parent width.");
            compare(ps1ManagerBlock.height, 800, "PS1Manager should correctly respect layout bindings mapping to the parent height.");
        }
        
        function test_ps1_library_actions() {
            var btnDelete = findChild(ps1ManagerBlock, "btnDeleteSelected");
            verify(btnDelete !== null, "btnDeleteSelected button is missing");
            
            ps1ManagerBlock.librarySelectionMap = {};
            verify(btnDelete.enabled === false, "Delete button should disable without items");
            ps1ManagerBlock.librarySelectionMap = { "/test/game.bin": true };
            ps1ManagerBlock.libraryGames = [ { "path": "/test/game.bin", "name": "Test Game", "binaryFileName": "game.bin" } ];
            verify(btnDelete.enabled === true, "Delete button should unlock with valid items");
            btnDelete.clicked();
            
            var btnFetch = findChild(ps1ManagerBlock, "btnFetchArt");
            verify(btnFetch !== null, "btnFetchArt button is missing");
            verify(btnFetch.enabled === true, "Fetch button should unlock when library games > 0");
            btnFetch.clicked();
            wait(50); 
        }

        function test_ps1_import_grid() {
            var btnSelectAll = findChild(ps1ManagerBlock, "btnSelectAll");
            verify(btnSelectAll !== null, "btnSelectAll button is missing");
            
            ps1ManagerBlock.importGames = [ { "path": "/media/drive/fake_ps1.bin", "isRenamed": false, "name": "Fake PS1", "extension": ".bin", "stats": {"size": 650000000} } ];
            wait(50);
            
            btnSelectAll.clicked();
            verify(ps1ManagerBlock.selectionMap["/media/drive/fake_ps1.bin"] === true, "Select All button failed mapping");
            btnSelectAll.clicked();
            verify(ps1ManagerBlock.selectionMap["/media/drive/fake_ps1.bin"] === undefined, "Deselecting failed");
            
            var btnImport = findChild(ps1ManagerBlock, "btnImportSelected");
            verify(btnImport !== null, "btnImportSelected button is missing");
            
            ps1ManagerBlock.selectionMap = { "/media/drive/fake_ps1.bin": true };
            verify(btnImport.enabled === true, "Import button should explicitly enable when item is mapped");
            btnImport.clicked();
        }
    }
}
