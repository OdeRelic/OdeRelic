#include "system_utils.h"
#include <QStorageInfo>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QSet>

SystemUtils::SystemUtils(QObject* parent) : QObject(parent) {}

QVariantMap SystemUtils::getStorageSpace(const QString& targetPath) {
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

QString SystemUtils::formatSize(double bytes) {
    if (bytes < 0) bytes = 0;
    double mult = getStorageMultiplier();
    double sizeInGB = bytes / (mult * mult * mult);
    
    if (sizeInGB > 0.99) {
        return QString::number(sizeInGB, 'f', 2) + " GB";
    } else {
        return QString::number(bytes / (mult * mult), 'f', 0) + " MB";
    }
}

qint64 SystemUtils::calculateCueRealSize(const QFileInfo &cueInfo) {
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
