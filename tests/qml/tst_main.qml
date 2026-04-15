import QtQuick
import QtQuick.Controls
import QtTest
import "../../src-cpp" as Src

Item {
    id: container
    width: 1200
    height: 800

    Src.HomeScreen {
        id: homeScreenBlock
        anchors.fill: parent
    }

    TestCase {
        name: "HomeScreen_UI_Integrity"
        when: windowShown
        
        function test_language_combo() {
            var combo = findChild(homeScreenBlock, "langCombo");
            verify(combo !== null, "langCombo is missing");
            verify(combo.count === 4, "Combo box must have 4 built-in translations at minimum");
            
            // Should be available dynamically
            combo.currentIndex = 1;
            verify(combo.currentText === "Português", "Failed translating locale selection to Portuguese");
        }
        
        function test_search_filtering() {
            var search = findChild(homeScreenBlock, "searchInput");
            verify(search !== null, "searchInput is missing");
            
            var cardGc = findChild(homeScreenBlock, "card_gc");
            verify(cardGc !== null, "Swiss card is missing");
            
            var cardPs1 = findChild(homeScreenBlock, "card_ps1");
            verify(cardPs1 !== null, "XStation card is missing");
            
            verify(cardGc.parent.width === 320, "Cards should default to 320px width when fully visible");
            
            // Search specifically for gamecube
            search.text = "gamecube";
            wait(50);
            
            verify(cardGc.parent.visible === true, "Swiss card must be visible when 'gamecube' is queried");
            verify(cardPs1.parent.visible === false, "PS1 card MUST hide when query excludes it");
            
            // Search another query
            search.text = "swiss";
            wait(50);
            verify(cardGc.parent.visible === true, "Swiss card handles exact ODE keyword arrays correctly");
        }
    }
}
