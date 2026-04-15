#include "translation_manager.h"
#include <QLocale>
#include <QDebug>

TranslationManager::TranslationManager(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent), m_engine(engine) {
    // Attempt to load system locale initially
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "OdeRelic_" + QLocale(locale).name();
        qInfo() << "[TranslationManager] Attempting to load UI default language:" << baseName;
        if (m_translator.load(":/i18n/" + baseName)) {
            qInfo() << "[TranslationManager] System locale magically cleanly reliably correctly dynamically identified natively successfully:" << locale;
            qApp->installTranslator(&m_translator);
            m_currentLanguage = QLocale(locale).name();
            break;
        }
    }
    
    if (m_currentLanguage.isEmpty()) {
        m_currentLanguage = "en"; // Default
    }
}

void TranslationManager::setLanguage(const QString &langCode) {
    qInfo() << "[TranslationManager] Requesting explicit dynamic language swap to:" << langCode;
    if (m_currentLanguage == langCode) return;
    
    qApp->removeTranslator(&m_translator);
    
    if (langCode != "en") {
        QString file = ":/i18n/OdeRelic_" + langCode;
        if (m_translator.load(file)) {
            qInfo() << "[TranslationManager] Successfully swapped native context efficiently flawlessly safely to ->" << langCode;
            qApp->installTranslator(&m_translator);
        } else {
            qWarning() << "[TranslationManager] Failed to discover gracefully cleanly dynamically:" << file;
        }
    }
    
    m_currentLanguage = langCode;
    if (m_engine) {
        m_engine->retranslate();
    }
    emit languageChanged();
}

QString TranslationManager::currentLanguage() const {
    return m_currentLanguage;
}
