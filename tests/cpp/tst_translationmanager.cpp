#include <QtTest>
#include <QQmlApplicationEngine>
#include "translation_manager.h"

// Define a minimal Application object needed by QTranslator bindings.
// QtTest already sets up QCoreApplication if we use QTEST_MAIN, 
// but QTranslator and QQmlApplicationEngine need a more robust test setup.

class TestTranslationManager : public QObject
{
    Q_OBJECT

private slots:
    void testSetLanguage();
    void testLanguageSignals();
    void testInvalidLanguageFallback();
};

void TestTranslationManager::testSetLanguage()
{
    QQmlApplicationEngine engine;
    TranslationManager tm(&engine);
    
    tm.setLanguage("pt_BR");
    QCOMPARE(tm.currentLanguage(), QString("pt_BR"));
    
    tm.setLanguage("ja");
    QCOMPARE(tm.currentLanguage(), QString("ja"));
}

void TestTranslationManager::testLanguageSignals()
{
    QQmlApplicationEngine engine;
    TranslationManager tm(&engine);
    
    QSignalSpy spy(&tm, &TranslationManager::languageChanged);
    
    tm.setLanguage("ja");
    QCOMPARE(spy.count(), 1);
}

void TestTranslationManager::testInvalidLanguageFallback()
{
    QQmlApplicationEngine engine;
    TranslationManager tm(&engine);
    
    tm.setLanguage("xyz_INVALID");
    // Should safely accept it but standard output might emit "Failed to load translation"
    QCOMPARE(tm.currentLanguage(), QString("xyz_INVALID"));
}

QTEST_MAIN(TestTranslationManager)
#include "tst_translationmanager.moc"
