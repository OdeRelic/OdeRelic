#include "system_utils.h"
#include <QStorageInfo>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QUrl>
#include <filesystem>

SystemUtils::SystemUtils(QObject* parent) : QObject(parent) {}

QVariantMap SystemUtils::getStorageSpace(const QString& targetPath) {
    qInfo() << "[SystemUtils] Fetching storage space for:" << targetPath;
    if (targetPath.isEmpty()) {
        return {{"total", 0}, {"free", 0}};
    }

    QStorageInfo storage(targetPath);
    if (!storage.isValid() || !storage.isReady()) {
        return {{"total", 0}, {"free", 0}};
    }

    qint64 bytesTotal = storage.bytesTotal();
    qint64 bytesFree = storage.bytesAvailable();

    return {
        {"total", bytesTotal},
        {"free", bytesFree}
    };
}

double SystemUtils::getStorageMultiplier() {
#ifdef Q_OS_MAC
    return 1000.0;
#else
    return 1024.0;
#endif
}

bool SystemUtils::isOnSameDrive(const QString& path1, const QString& path2) {
    if (path1.isEmpty() || path2.isEmpty()) return false;
    
    QStorageInfo storage1(path1);
    QStorageInfo storage2(path2);
    
    if (storage1.isValid() && storage2.isValid()) {
        QString root1 = storage1.rootPath();
        QString root2 = storage2.rootPath();
        
        if (!root1.isEmpty() && root1 == root2) {
            return true;
        }
    }
    return false;
}


QString SystemUtils::formatSize(double bytes) {
    if (bytes <= 0) {
        qWarning() << "[SystemUtils] formatSize called with zero or negative bytes:" << bytes;
        return "0 B";
    }
    double mult = getStorageMultiplier();
    double sizeInGB = bytes / (mult * mult * mult);
    
    if (sizeInGB > 0.99) {
        return QString::number(sizeInGB, 'f', 2) + " GB";
    } else {
        return QString::number(bytes / (mult * mult), 'f', 0) + " MB";
    }
}

qint64 SystemUtils::calculateCueRealSize(const QFileInfo &cueInfo) {
  qInfo() << "[SystemUtils] Calculating pure native CUE aggregated size for:" << cueInfo.absoluteFilePath();
  qint64 totalSize = 0;
  QSet<QString> processedFiles;
  QFile cueFile(cueInfo.absoluteFilePath());
  if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&cueFile);
    while (!in.atEnd()) {
      QString line = in.readLine().trimmed();
      if (line.startsWith("FILE", Qt::CaseInsensitive)) {
        int firstQuote = line.indexOf('"');
        int lastQuote = line.lastIndexOf('"');
        if (firstQuote != -1 && lastQuote != -1 && lastQuote > firstQuote) {
          QString binFileName =
              line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
          QFileInfo binInfo(cueInfo.absolutePath() + "/" + binFileName);
          if (binInfo.exists() && !processedFiles.contains(binInfo.absoluteFilePath())) {
            totalSize += binInfo.size();
            processedFiles.insert(binInfo.absoluteFilePath());
          }
        }
      }
    }
    cueFile.close();
  }
  
  QString base = cueInfo.completeBaseName();
  QString path = cueInfo.absolutePath();
  QStringList auxFormats = {"sbi", "cu2", "m3u", "txt"};
  for (const QString &ext : auxFormats) {
      QFileInfo auxInfo(path + "/" + base + "." + ext);
      if (auxInfo.exists() && !processedFiles.contains(auxInfo.absoluteFilePath())) {
          totalSize += auxInfo.size();
          processedFiles.insert(auxInfo.absoluteFilePath());
      }
  }
  
  return totalSize > 0 ? (totalSize + cueInfo.size()) : cueInfo.size();
}

QVariantMap SystemUtils::deleteGame(const QString& sourceRawPath, bool deleteParentFolder) {
    qInfo() << "[SystemUtils] Orchestrating destructive block wipe on:" << sourceRawPath << "Targeting parent directory deletion:" << deleteParentFolder;
    QVariantMap result;
    result["success"] = false;

    QString cleanPath = sourceRawPath;
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }

    QFileInfo fi(cleanPath);
    if (!fi.exists()) return result;

    std::error_code ec;

    if (cleanPath.endsWith(".cue", Qt::CaseInsensitive)) {
        QFile cueFile(cleanPath);
        if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!cueFile.atEnd()) {
                QString line = cueFile.readLine().trimmed();
                if (line.startsWith("FILE", Qt::CaseInsensitive)) {
                    int firstQuote = line.indexOf('"');
                    int lastQuote = line.lastIndexOf('"');
                    if (firstQuote != -1 && lastQuote > firstQuote) {
                        QString binName = line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
                        QFile::remove(fi.absolutePath() + "/" + binName);
                    }
                }
            }
            cueFile.close();
        }
        QFile::remove(cleanPath);
    } else {
        if (!fi.isDir()) {
            QFile::remove(cleanPath);
        }
    }

    if (deleteParentFolder) {
        if (fi.isDir()) {
            std::filesystem::remove_all(cleanPath.toStdString(), ec);
        } else {
            QDir parentDir = fi.dir();
            std::filesystem::remove_all(parentDir.absolutePath().toStdString(), ec);
        }
    }

    result["success"] = true;
    return result;
}
