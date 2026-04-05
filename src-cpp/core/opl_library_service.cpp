#include "opl_library_service.h"
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QString>
#include <QThread>
#include <QUrl>
#include <fstream>
#include <thread>
#include <vector>

OplLibraryService::OplLibraryService(QObject *parent) : QObject(parent) {
  // NetworkManager automatically initialized as stack value.
}

void OplLibraryService::startGetGamesFilesAsync(const QString &dirPath) {
  std::thread([this, dirPath]() {
    QVariantMap result;
    QVariantList files;

    QRegularExpression idRegex("(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|"
                               "SCAJ|SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}");

    QStringList subDirs = {
        "CD", "DVD", "."}; // Adding "." explicitly to capture flat directories
                           // like User's test-iso without strict bounds
    for (const QString &subDir : subDirs) {
      QDir dir(dirPath + (subDir == "." ? "" : "/" + subDir));
      if (dir.exists()) {
        QFileInfoList entries =
            dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

        QSet<QString> ignoreBins;
        QMap<QString, qint64> cueSizes;
        // Pre-pass: Find ALL .cue files in the directory and parse referenced
        // .BIN files
        for (const QFileInfo &fileInfo : entries) {
          if (fileInfo.suffix().toLower() == "cue") {
            qint64 totalBinSize = 0;
            QFile cueFile(fileInfo.absoluteFilePath());
            if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
              while (!cueFile.atEnd()) {
                QString line = cueFile.readLine().trimmed();
                if (line.startsWith("FILE")) {
                  // Extract the quoted filename: FILE "Track1.bin" BINARY
                  int firstQuote = line.indexOf('"');
                  int lastQuote = line.lastIndexOf('"');
                  if (firstQuote != -1 && lastQuote > firstQuote) {
                    QString binName =
                        line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
                    ignoreBins.insert(binName);

                    QFileInfo binFInfo(dir, binName);
                    if (binFInfo.exists()) {
                      totalBinSize += binFInfo.size();
                    }
                  }
                }
              }
            }
            if (totalBinSize > 0) {
              cueSizes.insert(fileInfo.fileName(), totalBinSize);
            }
          }
        }

        for (const QFileInfo &fileInfo : entries) {
          QString ext = fileInfo.suffix().toLower();
          if (ext == "iso" || ext == "zso" || ext == "bin" || ext == "cue") {
            if (ext == "bin" && ignoreBins.contains(fileInfo.fileName())) {
              continue; // Skip because it's managed natively by a .cue file
            }
            QVariantMap itemInfo;
            itemInfo["extension"] = "." + ext;
            itemInfo["name"] = fileInfo.completeBaseName();
            itemInfo["isRenamed"] = fileInfo.completeBaseName().contains(idRegex);

            qint64 actualSize = fileInfo.size();
            if (ext == "cue" && cueSizes.contains(fileInfo.fileName())) {
              actualSize = cueSizes.value(fileInfo.fileName());
            }
            itemInfo["parentPath"] = dir.absolutePath();
            itemInfo["path"] = fileInfo.absoluteFilePath();
            itemInfo["size"] = actualSize;
            itemInfo["stats"] =
                QVariantMap{{"size", actualSize}}; // Mimic simple stats
            files.append(itemInfo);
          }
        }
      }
    }

    result["success"] = true;
    result["data"] = files;
    
    QMetaObject::invokeMethod(this, [this, dirPath, result]() {
      emit gamesFilesLoaded(dirPath, result);
    }, Qt::QueuedConnection);

  }).detach();
}

QVariantMap OplLibraryService::getArtFolder(const QString &dirPath) {
  QVariantMap result;
  QVariantList artFiles;

  QDir dir(dirPath + "/ART");
  if (dir.exists()) {
    QFileInfoList entries =
        dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fileInfo : entries) {
      QString ext = fileInfo.suffix().toLower();
      if (ext == "jpg" || ext == "png") {
        QVariantMap item;
        item["name"] = fileInfo.completeBaseName();
        item["extension"] = "." + ext;
        item["path"] = fileInfo.absoluteFilePath();

        QStringList parts = fileInfo.completeBaseName().split("_");
        if (parts.size() >= 2) {
          item["gameId"] = parts[0] + "_" + parts[1];
        }
        if (parts.size() >= 3) {
          item["type"] = parts[2];
        }

        // In a real Qt setup, we might return a file:// URL instead of base64
        // to save memory, but let's mimic the API for now or let QML just load
        // the image via path.
        item["fileUrl"] =
            QUrl::fromLocalFile(fileInfo.absoluteFilePath()).toString();

        artFiles.append(item);
      }
    }
  }

  result["success"] = true;
  result["data"] = artFiles;
  return result;
}

void OplLibraryService::startDownloadArtAsync(const QString &dirPath,
                                                    const QString &gameId,
                                                    const QString &callbackSourceKey) {
  QThread *workerThread = QThread::create([this, dirPath, gameId, callbackSourceKey]() {
    QString baseUrl = "https://raw.githubusercontent.com/Luden02/psx-ps2-opl-art-database/refs/heads/main/PS2";
    QStringList types = {"COV", "ICO", "SCR", "COV2", "SCR2", "BG", "LAB", "LGO"};

    QDir().mkpath(dirPath);

    // Initialized securely on the QThread directly, locking its OS socket affinity.
    QNetworkAccessManager manager;
    int index = 0;
    int total = types.size();

    for (const QString &type : types) {
      QString fileName = gameId + "_" + type + ".png";
      QString urlStr = baseUrl + "/" + gameId + "/" + fileName;
      QString savePath = dirPath + "/" + fileName;

      QNetworkRequest request((QUrl(urlStr)));
      QNetworkReply *reply = manager.get(request);

      // Spin a local micro-event loop to forcefully resolve the TCP hook synchronously on this specific background thread
      QEventLoop loop;
      QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
      loop.exec();

      if (reply->error() == QNetworkReply::NoError) {
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
          file.write(reply->readAll());
          file.close();
        }
      }
      reply->deleteLater();

      index++;
      QMetaObject::invokeMethod(this, [=]() {
        emit artDownloadProgress(callbackSourceKey, (index * 100) / total);
      }, Qt::QueuedConnection);
    }

    QMetaObject::invokeMethod(this, [=]() {
      emit artDownloadFinished(callbackSourceKey, true, "All target art modules pulled.");
    }, Qt::QueuedConnection);

  });

  // Guarantee memory cleanup of the underlying OS thread bindings
  QObject::connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
  workerThread->start();
}

QVariantMap
OplLibraryService::tryDetermineGameIdFromHex(const QString &filepath) {
  QVariantMap result;

  QFile file(filepath);
  if (!file.open(QIODevice::ReadOnly)) {
    result["success"] = false;
    result["message"] = "Unable to open file.";
    return result;
  }

  const qint64 CHUNK_SIZE = 1024 * 1024; // 1MB
  const qint64 OVERLAP = 64;
  QByteArray carry;

  QRegularExpression regex("(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|SCAJ|"
                           "SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}(?:;1)?");

  while (!file.atEnd()) {
    QByteArray buffer = file.read(CHUNK_SIZE);
    if (buffer.isEmpty())
      break;

    QByteArray chunk = carry + buffer;
    QString chunkStr =
        QString::fromLatin1(chunk); // Treat as Latin1 since it's hex/binary

    QRegularExpressionMatch match = regex.match(chunkStr);
    if (match.hasMatch()) {
      QString gameIdRaw = match.captured(0);
      QString gameId = gameIdRaw;
      if (gameId.endsWith(";1")) {
        gameId.chop(2);
      }

      QString formattedGameId = gameId;
      formattedGameId.replace("_", "-").replace(".", "");

      file.close();

      result["success"] = true;
      result["gameId"] = gameId;
      result["formattedGameId"] = formattedGameId;
      return result;
    }

    if (chunk.size() > OVERLAP) {
      carry = chunk.right(OVERLAP);
    } else {
      carry = chunk;
    }
  }

  file.close();

  result["success"] = false;
  result["message"] =
      "Could not locate a PS2 game ID inside the provided file.";
  return result;
}

QVariantMap OplLibraryService::renameGamefile(const QString &dirpath,
                                              const QString &targetLibraryRoot,
                                              const QString &gameId,
                                              const QString &gameName,
                                              bool forceCD) {
  QVariantMap result;
  QFileInfo fileInfo(dirpath);
  QString ext = fileInfo.suffix().toLower();

  // Auto-detect media type based on format (OPL standard: .bin/.cue -> CD, .iso -> DVD) unless overridden
  bool isCD = forceCD || (ext == "bin" || ext == "cue");
  QString targetMediaFolder = isCD ? "CD" : "DVD";

  // Normalize target library root (clean any QUrl encoded spaces if passed
  // accidentally)
  QString cleanRoot = QUrl::fromPercentEncoding(targetLibraryRoot.toUtf8());

  QString newFileName = QString("%1.%2.%3").arg(gameId, gameName, ext);
  QString finalDir = cleanRoot + "/" + targetMediaFolder;

  // Ensure standard OPL folder exists first
  QDir().mkpath(finalDir);

  QString newFilePath = finalDir + "/" + newFileName;

  qDebug() << "[RENAME] Attempting to move file";
  qDebug() << "[RENAME] Source:" << dirpath;
  qDebug() << "[RENAME] Target:" << newFilePath;

  if (dirpath == newFilePath) {
    result["success"] = true;
    result["newPath"] = newFilePath;
    return result;
  }

  QFile file(dirpath);
  if (file.rename(newFilePath)) {
    qDebug() << "[RENAME] Success natively!";
    result["success"] = true;
    result["newPath"] = newFilePath;
  } else {
    QString renameErr = file.errorString();
    qWarning() << "[RENAME] Native rename failed:" << renameErr;

    bool isAlreadyInSameTargetDir =
        QFileInfo(dirpath).absolutePath() == QFileInfo(finalDir).absolutePath();
    if (isAlreadyInSameTargetDir) {
      result["success"] = false;
      result["message"] =
          QString("Intra-directory rename blocked natively: %1").arg(renameErr);
      return result;
    }

    // Fallback for cross-device moves where rename fails natively
    if (QFile::copy(dirpath, newFilePath)) {
      QFile::remove(dirpath);
      qDebug() << "[RENAME] Fallback copy/remove successful!";
      result["success"] = true;
      result["newPath"] = newFilePath;
    } else {
      result["success"] = false;
      result["message"] = file.errorString();
    }
  }

  return result;
}

QVariantMap OplLibraryService::moveFile(const QString &sourcePath,
                                        const QString &destPath) {
  QVariantMap result;
  QString targetPath = destPath;

  QFileInfo destInfo(destPath);
  if (destInfo.isDir()) {
    targetPath = destPath + "/" + QFileInfo(sourcePath).fileName();
  }

  if (QFile::rename(sourcePath, targetPath)) {
    result["success"] = true;
    result["newPath"] = targetPath;
  } else {
    // Fallback to copy and delete
    if (QFile::copy(sourcePath, targetPath)) {
      QFile::remove(sourcePath);
      result["success"] = true;
      result["newPath"] = targetPath;
    } else {
      result["success"] = false;
      result["message"] = "Failed to move file";
    }
  }
  return result;
}

void OplLibraryService::startConvertBinToIso(const QString &sourceBinPath,
                                             const QString &destIsoPath) {
  qDebug() << "[BIN2ISO] Spawning autonomous thread for: " << sourceBinPath;

  std::thread([this, sourceBinPath, destIsoPath]() {
    QFileInfo cueInfo(sourceBinPath);
    QString binFilePath = sourceBinPath;

    if (cueInfo.suffix().toLower() == "cue") {
      QFile cueFile(sourceBinPath);
      if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!cueFile.atEnd()) {
          QString line = cueFile.readLine().trimmed();
          if (line.startsWith("FILE")) {
            int firstQuote = line.indexOf('"');
            int lastQuote = line.lastIndexOf('"');
            if (firstQuote != -1 && lastQuote > firstQuote) {
              QString binName =
                  line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
              binFilePath =
                  QFileInfo(cueInfo.absoluteDir(), binName).absoluteFilePath();
              break;
            }
          }
        }
      }
    }

    QFile bin(binFilePath);
    QFile iso(destIsoPath);

    if (!bin.open(QIODevice::ReadOnly)) {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit conversionFinished(sourceBinPath, false, destIsoPath,
                                    "Unable to read source BIN file.");
          },
          Qt::QueuedConnection);
      return;
    }

    if (!iso.open(QIODevice::WriteOnly)) {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit conversionFinished(sourceBinPath, false, destIsoPath,
                                    "Unable to create target ISO file.");
          },
          Qt::QueuedConnection);
      return;
    }

    const qint64 RAW_SECTOR_SIZE = 2352;
    const qint64 ISO_SECTOR_SIZE = 2048;
    const qint64 SECTOR_HEADER_OFFSET = 24;

    qint64 totalBytes = bin.size();
    qint64 totalSectors = totalBytes / RAW_SECTOR_SIZE;
    qint64 totalProcessed = 0;

    // Buffer 1024 sectors at a time (~2.4 MB chunks) for high-performance
    // extraction natively in C++
    const int SECTORS_PER_CHUNK = 1024;
    const qint64 READ_CHUNK = RAW_SECTOR_SIZE * SECTORS_PER_CHUNK;

    while (!bin.atEnd()) {
      QByteArray chunk = bin.read(READ_CHUNK);
      if (chunk.isEmpty())
        break;

      qint64 bytesInChunk = chunk.size();
      qint64 sectorsInChunk = bytesInChunk / RAW_SECTOR_SIZE;

      QByteArray outChunk;
      outChunk.reserve(sectorsInChunk * ISO_SECTOR_SIZE);

      const char *inData = chunk.constData();
      for (qint64 i = 0; i < sectorsInChunk; ++i) {
        outChunk.append(inData + (i * RAW_SECTOR_SIZE) + SECTOR_HEADER_OFFSET,
                        ISO_SECTOR_SIZE);
      }

      iso.write(outChunk);

      totalProcessed += sectorsInChunk;
      int percent = 0;
      if (totalSectors > 0)
        percent = (int)((totalProcessed * 100) / totalSectors);
      QMetaObject::invokeMethod(
          this, [=]() { emit conversionProgress(sourceBinPath, percent); },
          Qt::QueuedConnection);
    }

    bin.close();
    iso.close();

    qDebug() << "[BIN2ISO] Success outputting pure 2048-byte payload to: "
             << destIsoPath;
    QMetaObject::invokeMethod(
        this,
        [=]() {
          emit conversionFinished(sourceBinPath, true, destIsoPath, "");
        },
        Qt::QueuedConnection);
  }).detach();
}

QVariantMap OplLibraryService::deleteFileAndCue(const QString &sourceRawPath) {
  QVariantMap result;

  if (sourceRawPath.endsWith(".cue", Qt::CaseInsensitive)) {
    // Find all tracks and delete them, then delete the CUE.
    QFile cueFile(sourceRawPath);
    QFileInfo cueInfo(sourceRawPath);
    if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      while (!cueFile.atEnd()) {
        QString line = cueFile.readLine().trimmed();
        if (line.startsWith("FILE")) {
          int firstQuote = line.indexOf('"');
          int lastQuote = line.lastIndexOf('"');
          if (firstQuote != -1 && lastQuote > firstQuote) {
            QString binName =
                line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
            QFile::remove(cueInfo.absolutePath() + "/" + binName);
          }
        }
      }
      cueFile.close();
    }
    QFile::remove(sourceRawPath);
  } else {
    // Standard legacy .BIN logic
    QFileInfo binInfo(sourceRawPath);
    QString basePath =
        binInfo.absolutePath() + "/" + binInfo.completeBaseName();
    QFile::remove(sourceRawPath);
    QFile::remove(basePath + ".cue");
    QFile::remove(basePath + ".CUE");
  }

  result["success"] = true;
  return result;
}

void OplLibraryService::startImportIsoAsync(const QString &sourceIsoPath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName, bool forceCD) {
  qDebug() << "[ISO_ASYNC] Spawning autonomous thread for: " << sourceIsoPath;

  std::thread([this, sourceIsoPath, targetLibraryRoot, gameId, gameName, forceCD]() {
    QFileInfo fileInfo(sourceIsoPath);
    QString ext = fileInfo.suffix().toLower();
    
    bool isCD = forceCD || (ext == "bin" || ext == "cue");
    QString targetMediaFolder = isCD ? "CD" : "DVD";
    QString cleanRoot = QUrl::fromPercentEncoding(targetLibraryRoot.toUtf8());
    QString newFileName = QString("%1.%2.%3").arg(gameId, gameName, ext);
    QString finalDir = cleanRoot + "/" + targetMediaFolder;

    QDir().mkpath(finalDir);
    QString destIsoPath = finalDir + "/" + newFileName;

    if (sourceIsoPath == destIsoPath) {
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, true, destIsoPath, "Already in target directory");
      }, Qt::QueuedConnection);
      return;
    }

    QFile sourceFile(sourceIsoPath);
    // Attempt standard OS level rename index rewrite first for extreme speed
    if (sourceFile.rename(destIsoPath)) {
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoProgress(sourceIsoPath, 100);
        emit importIsoFinished(sourceIsoPath, true, destIsoPath, "Native rename successful");
      }, Qt::QueuedConnection);
      return;
    }

    // NATIVE MOVE FAILED (Cross-Device). Proceeding to Threaded Chunk Stream
    if (!sourceFile.open(QIODevice::ReadOnly)) {
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, false, "", "Unable to read source ISO file.");
      }, Qt::QueuedConnection);
      return;
    }

    QFile destFile(destIsoPath);
    if (!destFile.open(QIODevice::WriteOnly)) {
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, false, "", "Unable to create target ISO file.");
      }, Qt::QueuedConnection);
      return;
    }

    qint64 totalBytes = sourceFile.size();
    qint64 bytesProcessed = 0;
    const qint64 READ_CHUNK = 4 * 1024 * 1024; // 4MB buffer chunks

    int lastPercent = -1;

    while (!sourceFile.atEnd()) {
      QByteArray chunk = sourceFile.read(READ_CHUNK);
      if (chunk.isEmpty()) break;

      destFile.write(chunk);
      bytesProcessed += chunk.size();

      int currentPercent = (int)((bytesProcessed * 100) / totalBytes);
      if (currentPercent > lastPercent) {
        lastPercent = currentPercent;
        QMetaObject::invokeMethod(this, [=]() {
          emit importIsoProgress(sourceIsoPath, currentPercent);
        }, Qt::QueuedConnection);
      }
    }

    sourceFile.close();
    destFile.close();

    // Verify copy succeeded then delete source
    if (bytesProcessed == totalBytes && totalBytes > 0) {
      QFile::remove(sourceIsoPath);
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, true, destIsoPath, "Success");
      }, Qt::QueuedConnection);
    } else {
      destFile.remove(); // Cleanup corrupted target
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, false, "", "Copy validation failed");
      }, Qt::QueuedConnection);
    }
  }).detach();
}

QVariantList OplLibraryService::getExternalGameFilesData(const QStringList &fileUrls) {
  QVariantList files;
  QRegularExpression idRegex("(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|"
                             "SCAJ|SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}");
                             
  for (const QString &urlStr : fileUrls) {
      QUrl url(urlStr);
      QString filePath = url.isLocalFile() ? url.toLocalFile() : urlStr;
      QFileInfo fileInfo(filePath);
      
      if (fileInfo.exists() && fileInfo.isFile()) {
          QString ext = fileInfo.suffix().toLower();
          if (ext == "iso" || ext == "zso" || ext == "bin" || ext == "cue") {
              QVariantMap itemInfo;
              itemInfo["extension"] = "." + ext;
              itemInfo["name"] = fileInfo.completeBaseName();
              itemInfo["isRenamed"] = fileInfo.completeBaseName().contains(idRegex);
              itemInfo["parentPath"] = fileInfo.absolutePath();
              itemInfo["path"] = fileInfo.absoluteFilePath();
              itemInfo["size"] = fileInfo.size();
              itemInfo["stats"] = QVariantMap{{"size", fileInfo.size()}};
              files.append(itemInfo);
          }
      }
  }
  return files;
}
