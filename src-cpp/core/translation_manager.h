#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QQmlApplicationEngine>
#include <QGuiApplication>

class TranslationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY languageChanged)

public:
    explicit TranslationManager(QQmlApplicationEngine *engine, QObject *parent = nullptr);
    
    Q_INVOKABLE void setLanguage(const QString &langCode);
    QString currentLanguage() const;

signals:
    void languageChanged();

private:
    QQmlApplicationEngine *m_engine;
    QTranslator m_translator;
    QString m_currentLanguage;
};
