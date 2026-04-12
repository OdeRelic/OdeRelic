#include "logger.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <iostream>

#ifdef Q_OS_WIN
#include <QProcessEnvironment>
#endif

namespace {
    QString getLogFilePath() {
#if defined(Q_OS_MAC)
        QString homePath = QDir::homePath();
        return homePath + "/Library/Logs/OdeRelic/oderelic.log";
#elif defined(Q_OS_WIN)
        QString localAppData = QProcessEnvironment::systemEnvironment().value("LOCALAPPDATA", QDir::homePath());
        return localAppData + "/OdeRelic/oderelic.log";
#elif defined(Q_OS_LINUX)
        return "/var/log/OdeRelic/oderelic.log";
#else
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/oderelic.log";
#endif
    }
    
    // Fallback if permission denied (especially for /var/log on Linux without sudo)
    QString getFallbackLogFilePath() {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/oderelic.log";
    }

    void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        (void)context;
        QByteArray localMsg = msg.toLocal8Bit();
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        QString levelStr;
        switch (type) {
        case QtDebugMsg:
            levelStr = "DEBUG";
            break;
        case QtInfoMsg:
            levelStr = "INFO";
            break;
        case QtWarningMsg:
            levelStr = "WARN";
            break;
        case QtCriticalMsg:
            levelStr = "CRITICAL";
            break;
        case QtFatalMsg:
            levelStr = "FATAL";
            break;
        }

        QString logMessage = QString("[%1] [%2] %3\n").arg(timestamp).arg(levelStr).arg(localMsg.constData());
        
        QString logPath = getLogFilePath();
        QFile outFile(logPath);
        
        QFileInfo fileInfo(logPath);
        QDir dir = fileInfo.absoluteDir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QString fallbackPath = getFallbackLogFilePath();
            QFileInfo fallbackInfo(fallbackPath);
            QDir fallbackDir = fallbackInfo.absoluteDir();
            if (!fallbackDir.exists()) {
                fallbackDir.mkpath(".");
            }
            outFile.setFileName(fallbackPath);
            outFile.open(QIODevice::WriteOnly | QIODevice::Append);
        }

        if (outFile.isOpen()) {
            QTextStream ts(&outFile);
            ts << logMessage;
            outFile.close();
        }
    }
}

void Logger::init() {
    qInstallMessageHandler(messageHandler);
}
