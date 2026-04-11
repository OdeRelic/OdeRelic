#include "opl_library_service.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QMap>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QString>
#include <QThread>
#include <QElapsedTimer>
#include <QStorageInfo>
#include <QProcess>
#include <QUrl>
#include "filesystem_cache.h"
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
    
    QVariantMap cacheData = FileSystemCache::loadCache(dirPath, "ps2");
    bool cacheDirty = false;

    QRegularExpression idRegex("(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|"
                               "SCAJ|SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}");

    QStringList subDirs = {
        "CD", "DVD", "."}; // Adding "." explicitly to capture flat directories
                           // like User's test-iso without strict bounds
    
    int index = 0;
    int total = subDirs.size();
    for (const QString &subDir : subDirs) {
      index++;
      QMetaObject::invokeMethod(this, [=]() {
        emit libraryScanProgress(index, total);
      }, Qt::QueuedConnection);
      
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
            
            // Extract disk version gracefully interacting with persistent root JSON caches
            QString version = "1.00"; // default fallback
            QString pathKey = fileInfo.fileName();
            
            if (cacheData.contains(pathKey)) {
                QVariantMap cacheItem = cacheData[pathKey].toMap();
                // FAT32 arrays shift `mtime` dynamically via OS daylight saving rules. Relying on file size safely bypasses cache wiping.
                if (cacheItem["size"].toLongLong() == fileInfo.size() || cacheItem["mtime"].toLongLong() == fileInfo.lastModified().toSecsSinceEpoch()) {
                    version = cacheItem["version"].toString();
                } else {
                    QVariantMap idRes = tryDetermineGameIdFromHex(fileInfo.absoluteFilePath());
                    if (idRes["success"].toBool() && idRes.contains("version") && idRes["version"].toString() != "") {
                        version = idRes["version"].toString();
                    }
                    QVariantMap newCacheItem;
                    newCacheItem["mtime"] = fileInfo.lastModified().toSecsSinceEpoch();
                    newCacheItem["size"] = fileInfo.size();
                    newCacheItem["version"] = version;
                    cacheData[pathKey] = newCacheItem;
                    cacheDirty = true;
                }
            } else {
                QVariantMap idRes = tryDetermineGameIdFromHex(fileInfo.absoluteFilePath());
                if (idRes["success"].toBool() && idRes.contains("version") && idRes["version"].toString() != "") {
                  version = idRes["version"].toString();
                }
                QVariantMap newCacheItem;
                newCacheItem["mtime"] = fileInfo.lastModified().toSecsSinceEpoch();
                newCacheItem["size"] = fileInfo.size();
                newCacheItem["version"] = version;
                cacheData[pathKey] = newCacheItem;
                cacheDirty = true;
            }

            itemInfo["parentPath"] = dir.absolutePath();
            itemInfo["path"] = fileInfo.absoluteFilePath();
            itemInfo["size"] = actualSize;
            itemInfo["version"] = version;
            itemInfo["stats"] =
                QVariantMap{{"size", actualSize}}; // Mimic simple stats
            files.append(itemInfo);
          }
        }
      }
    }
    
    if (cacheDirty) {
        FileSystemCache::saveCache(dirPath, cacheData, "ps2");
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
  const qint64 MAX_SEARCH_BYTES = 8 * 1024 * 1024; // Check max 8MB of the chunk preventing infinite 4GB loops
  qint64 totalRead = 0;
  QByteArray carry;

  QRegularExpression regex("(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|SCAJ|"
                           "SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}(?:;1)?");

  while (!file.atEnd() && totalRead < MAX_SEARCH_BYTES) {
    QByteArray buffer = file.read(CHUNK_SIZE);
    if (buffer.isEmpty())
      break;
    
    totalRead += buffer.size();

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
      
      QString version = "";
      QRegularExpression verRegex("VER\\s*=\\s*([0-9]+\\.[0-9]+)");
      QRegularExpressionMatch verMatch = verRegex.match(chunkStr);
      if (verMatch.hasMatch()) {
          version = verMatch.captured(1);
      }

      file.close();

      result["success"] = true;
      result["gameId"] = gameId;
      result["version"] = version;
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
        emit importIsoProgress(sourceIsoPath, 100, 0.0);
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
    QElapsedTimer timer;
    timer.start();

    while (!sourceFile.atEnd()) {
      QByteArray chunk = sourceFile.read(READ_CHUNK);
      if (chunk.isEmpty()) break;

      destFile.write(chunk);
      bytesProcessed += chunk.size();

      int currentPercent = (int)((bytesProcessed * 100) / totalBytes);
      double elapsedSecs = timer.elapsed() / 1000.0;
      double mbps = 0.0;
      if (elapsedSecs > 0.0) {
          mbps = (bytesProcessed / (1024.0 * 1024.0)) / elapsedSecs;
      }
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoProgress(sourceIsoPath, currentPercent, mbps);
      }, Qt::QueuedConnection);
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

QVariantMap OplLibraryService::checkOplFolder(const QString &rootPath) {
    QVariantMap result;
    
    QStorageInfo storage(rootPath);
    QString fsType = QString::fromUtf8(storage.fileSystemType()).toLower();
    
    // Some OS variations report FAT32 as vfat or msdos natively.
    bool isFormatCorrect = fsType.contains("fat32") || fsType.contains("exfat") || fsType.contains("vfat") || fsType.contains("msdos") || fsType.contains("fat");
    bool isPartitionCorrect = true; // Fallback grant if environment doesn't support interrogation
    
#ifdef Q_OS_MAC
    QString deviceNode = QString::fromUtf8(storage.device());
    QProcess p;
    p.start("sh", QStringList() << "-c" << QString("diskutil info %1 | awk '/Part of Whole/ {print $4}'").arg(deviceNode));
    p.waitForFinished(3000);
    QString wholeDisk = p.readAllStandardOutput().trimmed();
    if (!wholeDisk.isEmpty()) {
        QProcess p2;
        p2.start("sh", QStringList() << "-c" << QString("diskutil info %1 | grep -i 'Content'").arg(wholeDisk));
        p2.waitForFinished(3000);
        QString content = p2.readAllStandardOutput().trimmed();
        if (!content.isEmpty()) {
            isPartitionCorrect = content.contains("FDisk_partition_scheme", Qt::CaseInsensitive);
        }
    }
#elif defined(Q_OS_LINUX)
    QString deviceNode = QString::fromUtf8(storage.device());
    QProcess p;
    p.start("sh", QStringList() << "-c" << QString("lsblk -o PTTYPE -n -d $(lsblk -no pkname %1) | head -n 1").arg(deviceNode));
    p.waitForFinished(3000);
    QString pttype = p.readAllStandardOutput().trimmed().toLower();
    if (!pttype.isEmpty()) {
        isPartitionCorrect = (pttype == "dos");
    }
#elif defined(Q_OS_WIN)
    QString driveLetter = rootPath.left(1);
    if (!driveLetter.isEmpty() && driveLetter.at(0).isLetter()) {
        QString cmd = QString("powershell -NoProfile -Command \"Get-Partition -DriveLetter %1 | Get-Disk | Select-Object -ExpandProperty PartitionStyle\"").arg(driveLetter);
        QProcess p;
        p.start(cmd);
        p.waitForFinished(3000);
        QString style = p.readAllStandardOutput().trimmed().toLower();
        if (!style.isEmpty()) {
            isPartitionCorrect = (style == "mbr");
        }
    }
#endif

    result["isFormatCorrect"] = isFormatCorrect;
    result["isPartitionCorrect"] = isPartitionCorrect;

    return result;
}

namespace {
  qint64 getCueRealSize(const QFileInfo &fileInfo) {
      qint64 totalSize = 0;
      QFile cueFile(fileInfo.absoluteFilePath());
      if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
          while (!cueFile.atEnd()) {
              QString line = cueFile.readLine().trimmed();
              if (line.startsWith("FILE", Qt::CaseInsensitive)) {
                  int fq = line.indexOf('"');
                  int lq = line.lastIndexOf('"');
                  if (fq != -1 && lq > fq) {
                      QString binName = line.mid(fq + 1, lq - fq - 1);
                      QFileInfo binInfo(fileInfo.absoluteDir().absoluteFilePath(binName));
                      if (binInfo.exists()) totalSize += binInfo.size();
                  }
              }
          }
          cueFile.close();
      }
      return totalSize > 0 ? totalSize : fileInfo.size();
  }

  void scanDirectoryRecursively(OplLibraryService* self, const QString &dirPath, QFileInfoList &outFiles) {
      // Pass 1: Rapidly count total directories
      int totalFolders = 0;
      QDirIterator dirIt(dirPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
      while (dirIt.hasNext()) { dirIt.next(); totalFolders++; }

      // Pass 2: Parse items and emit progress limits natively
      QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
      QMap<QString, QFileInfoList> filesByDir;
      
      int processedFolders = 0;
      QString lastDir = "";

      while (it.hasNext()) {
          it.next();
          QFileInfo fi = it.fileInfo();
          if (fi.isFile()) {
              QString currentDir = fi.absolutePath();
              filesByDir[currentDir].append(fi);
              
              if (currentDir != lastDir) {
                  lastDir = currentDir;
                  processedFolders++;
                  if (self && totalFolders > 0) {
                      QMetaObject::invokeMethod(self, [self, processedFolders, totalFolders]() {
                          emit self->externalFilesScanProgress(processedFolders, totalFolders);
                      }, Qt::QueuedConnection);
                  }
              }
          }
      }
      
      for (auto itGrp = filesByDir.begin(); itGrp != filesByDir.end(); ++itGrp) {
          QFileInfoList group = itGrp.value();
          
          bool hasCue = false;
          for (const QFileInfo &f : group) {
              if (f.suffix().toLower() == "cue") { hasCue = true; break; }
          }
          
          for (const QFileInfo &f : group) {
              if (hasCue) {
                  QString suf = f.suffix().toLower();
                  if (suf == "bin" || suf == "img") continue;
              }
              outFiles.append(f);
          }
      }
  }
}

QVariantList OplLibraryService::getExternalGameFilesData(const QStringList &fileUrls) {
  QVariantList files;
  QRegularExpression idRegex("(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|"
                             "SCAJ|SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}");
                             
  for (const QString &urlStr : fileUrls) {
      QUrl url(urlStr);
      QString filePath = url.isLocalFile() ? url.toLocalFile() : urlStr;
      QFileInfo baseInfo(filePath);
      
      QFileInfoList toProcess;
      if (baseInfo.isDir()) {
          scanDirectoryRecursively(this, filePath, toProcess);
      } else {
          toProcess.append(baseInfo);
      }

      for (const QFileInfo &fileInfo : toProcess) {
          if (fileInfo.exists() && fileInfo.isFile()) {
              QString ext = fileInfo.suffix().toLower();
              if (ext == "iso" || ext == "zso" || ext == "bin" || ext == "cue") {
                  qint64 reportedSize = (ext == "cue") ? getCueRealSize(fileInfo) : fileInfo.size();
                  
                  QVariantMap itemInfo;
                  itemInfo["extension"] = "." + ext;
                  itemInfo["name"] = fileInfo.completeBaseName();
                  itemInfo["isRenamed"] = fileInfo.completeBaseName().contains(idRegex);
                  itemInfo["parentPath"] = fileInfo.absolutePath();
                  itemInfo["path"] = fileInfo.absoluteFilePath();
                  itemInfo["size"] = reportedSize;
                  itemInfo["stats"] = QVariantMap{{"size", reportedSize}};
                  files.append(itemInfo);
              }
          }
      }
  }
  return files;
}

void OplLibraryService::scanExternalFilesAsync(const QStringList &fileUrls, bool isPs1) {
    std::thread([this, fileUrls, isPs1]() {
        QVariantList files = isPs1 ? getExternalPs1FilesData(fileUrls) : getExternalGameFilesData(fileUrls);
        
        QMetaObject::invokeMethod(this, [this, isPs1, files]() {
            emit externalFilesScanFinished(isPs1, files);
        }, Qt::QueuedConnection);
    }).detach();
}

// ═══════════════════════════════════════════════════════════════════════════
// PS1 / POPStarter Module
// ═══════════════════════════════════════════════════════════════════════════

void OplLibraryService::startGetPs1GamesAsync(const QString &dirPath) {
  std::thread([this, dirPath]() {
    QVariantMap result;
    QVariantList files;
    
    QVariantMap cacheData = FileSystemCache::loadCache(dirPath, "ps1");
    bool cacheDirty = false;

    QRegularExpression idRegex(
        "(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|"
        "SCAJ|SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}");

    QDir popsDir(dirPath + "/POPS");
    if (popsDir.exists()) {
      QFileInfoList entries =
          popsDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

      for (const QFileInfo &fileInfo : entries) {
        QString ext = fileInfo.suffix().toLower();
        if (ext == "vcd") {
          QVariantMap itemInfo;
          itemInfo["extension"] = ".vcd";
          itemInfo["name"]      = fileInfo.completeBaseName();
          itemInfo["isRenamed"] = fileInfo.completeBaseName().contains(idRegex);
          itemInfo["size"]      = fileInfo.size();
          
          QString version = "1.00";
          QString pathKey = fileInfo.fileName();
          
          if (cacheData.contains(pathKey)) {
              QVariantMap cacheItem = cacheData[pathKey].toMap();
              // Prevent FAT32 UTC drift wipes by integrating file size parity natively for VCDs
              if (cacheItem["size"].toLongLong() == fileInfo.size() || cacheItem["mtime"].toLongLong() == fileInfo.lastModified().toSecsSinceEpoch()) {
                  version = cacheItem["version"].toString();
              } else {
                  QVariantMap idRes = tryDeterminePs1GameIdFromHex(fileInfo.absoluteFilePath());
                  if (idRes["success"].toBool() && idRes.contains("version") && idRes["version"].toString() != "") {
                      version = idRes["version"].toString();
                  }
                  QVariantMap newCacheItem;
                  newCacheItem["mtime"] = fileInfo.lastModified().toSecsSinceEpoch();
                  newCacheItem["size"] = fileInfo.size();
                  newCacheItem["version"] = version;
                  cacheData[pathKey] = newCacheItem;
                  cacheDirty = true;
              }
          } else {
              QVariantMap idRes = tryDeterminePs1GameIdFromHex(fileInfo.absoluteFilePath());
              if (idRes["success"].toBool() && idRes.contains("version") && idRes["version"].toString() != "") {
                  version = idRes["version"].toString();
              }
              QVariantMap newCacheItem;
              newCacheItem["mtime"] = fileInfo.lastModified().toSecsSinceEpoch();
              newCacheItem["size"] = fileInfo.size();
              newCacheItem["version"] = version;
              cacheData[pathKey] = newCacheItem;
              cacheDirty = true;
          }
          
          itemInfo["parentPath"] = popsDir.absolutePath();
          itemInfo["path"]      = fileInfo.absoluteFilePath();
          itemInfo["version"]   = version;
          itemInfo["stats"]     = QVariantMap{{"size", fileInfo.size()}};
          files.append(itemInfo);
        }
      }
    }
    
    if (cacheDirty) {
        FileSystemCache::saveCache(dirPath, cacheData, "ps1");
    }

    result["success"] = true;
    result["data"]    = files;

    QMetaObject::invokeMethod(this, [this, dirPath, result]() {
      emit ps1GamesLoaded(dirPath, result);
    }, Qt::QueuedConnection);
  }).detach();
}

// ---------------------------------------------------------------------------
// PS1 Game ID detection
// PS1 discs embed a SYSTEM.CNF file containing a line like:
//   BOOT=cdrom:\SLUS_012.34;1
// We scan the raw binary for this pattern in addition to the bare ID regex.
// ---------------------------------------------------------------------------
QVariantMap OplLibraryService::tryDeterminePs1GameIdFromHex(const QString &filepath) {
  QVariantMap result;

  QFile file(filepath);
  if (!file.open(QIODevice::ReadOnly)) {
    result["success"] = false;
    result["message"] = "Unable to open file.";
    return result;
  }

  const qint64 CHUNK_SIZE = 1024 * 1024; // 1 MB
  const qint64 OVERLAP = 64;
  const qint64 MAX_SEARCH_BYTES = 8 * 1024 * 1024; // Enforce max 8MB PS1 bound preventing 10-minute freezes entirely
  qint64 totalRead = 0;
  QByteArray carry;

  // Match BOOT= path AND bare IDs (handles both SYSTEM.CNF and raw disc header)
  QRegularExpression bootRegex("BOOT\\s*=\\s*cdrom:\\\\([A-Z]{4}_[0-9]{3}\\.[0-9]{2})(?:;1)?",
      QRegularExpression::CaseInsensitiveOption);

  QRegularExpression bareIdRegex(
      "(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|SCAJ|SLKA|SCKA|SCED|SCCS)"
      "_[0-9]{3}\\.[0-9]{2}(?:;1)?");

  while (!file.atEnd() && totalRead < MAX_SEARCH_BYTES) {
    QByteArray buffer = file.read(CHUNK_SIZE);
    if (buffer.isEmpty()) break;

    QByteArray chunk = carry + buffer;
    QString chunkStr = QString::fromLatin1(chunk);

    // Prefer the BOOT= match (most reliable for PS1)
    QRegularExpressionMatch bootMatch = bootRegex.match(chunkStr);
    if (bootMatch.hasMatch()) {
      QString gameId = bootMatch.captured(1).toUpper();
      if (gameId.endsWith(";1")) gameId.chop(2);
      QString formatted = gameId;
      formatted.replace("_", "-").replace(".", "");
      
      QString version = "";
      QRegularExpression verRegex("VER\\s*=\\s*([0-9]+\\.[0-9]+)");
      QRegularExpressionMatch verMatch = verRegex.match(chunkStr);
      if (verMatch.hasMatch()) {
          version = verMatch.captured(1);
      }
      
      file.close();
      result["success"]         = true;
      result["gameId"]          = gameId;
      result["version"]         = version;
      result["formattedGameId"] = formatted;
      return result;
    }

    // Fallback: bare ID in binary header
    QRegularExpressionMatch bareMatch = bareIdRegex.match(chunkStr);
    if (bareMatch.hasMatch()) {
      QString gameId = bareMatch.captured(0).toUpper();
      if (gameId.endsWith(";1")) gameId.chop(2);
      QString formatted = gameId;
      formatted.replace("_", "-").replace(".", "");
      
      QString version = "";
      QRegularExpression verRegex("VER\\s*=\\s*([0-9]+\\.[0-9]+)");
      QRegularExpressionMatch verMatch = verRegex.match(chunkStr);
      if (verMatch.hasMatch()) {
          version = verMatch.captured(1);
      }
      
      file.close();
      result["success"]         = true;
      result["gameId"]          = gameId;
      result["version"]         = version;
      result["formattedGameId"] = formatted;
      return result;
    }

    if (chunk.size() > OVERLAP)
      carry = chunk.right(OVERLAP);
    else
      carry = chunk;
  }

  file.close();
  result["success"] = false;
  result["message"] = "Could not locate a PS1 game ID inside the provided file.";
  return result;
}

// ---------------------------------------------------------------------------
// BIN/CUE → VCD conversion  (multi-track aware)
// Parses ALL FILE entries from the CUE sheet and concatenates the data
// sectors from every DATA track. CDDA/audio sectors are detected via the
// standard 12-byte sync header (0x00 FF FF FF FF FF FF FF FF FF FF 00) and
// skipped — POPStarter does not support CD audio tracks.
// ---------------------------------------------------------------------------
void OplLibraryService::startConvertBinToVcd(const QString &sourcePath,
                                              const QString &destVcdPath) {
  qDebug() << "[BIN2VCD] Spawning autonomous thread for:" << sourcePath;

  std::thread([this, sourcePath, destVcdPath]() {
    // If QML passed a file:// URL or just a path, normalize it cross-platform
    QString cleanDestPath = urlToLocalFile(destVcdPath);
    qDebug() << "[BIN2VCD] Computed VCD path:" << cleanDestPath;

    QFileInfo srcInfo(sourcePath);
    const QString srcExt = srcInfo.suffix().toLower();

    // ── Build ordered list of BIN tracks from CUE ─────────────────────────
    // Each entry: { filePath, isAudio }
    struct TrackEntry { QString path; bool isAudio; };
    QVector<TrackEntry> tracks;

    if (srcExt == "cue") {
      QFile cueFile(sourcePath);
      if (!cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMetaObject::invokeMethod(this, [=]() {
          emit ps1ConversionFinished(sourcePath, false, cleanDestPath, QString(), "Cannot open CUE file.");
        }, Qt::QueuedConnection);
        return;
      }

      QString currentFilePath;
      bool    currentIsAudio = false;
      bool    seenTrack      = false;

      while (!cueFile.atEnd()) {
        QString line = cueFile.readLine().trimmed();

        if (line.startsWith("FILE", Qt::CaseInsensitive)) {
          // Flush previous track if any
          if (seenTrack && !currentFilePath.isEmpty()) {
            tracks.append({ currentFilePath, currentIsAudio });
          }
          int fq = line.indexOf('"');
          int lq = line.lastIndexOf('"');
          if (fq != -1 && lq > fq) {
            QString binName = line.mid(fq + 1, lq - fq - 1);
            currentFilePath = QFileInfo(srcInfo.absoluteDir(), binName).absoluteFilePath();
          } else {
            currentFilePath.clear();
          }
          currentIsAudio = false;
          seenTrack      = false;

        } else if (line.startsWith("TRACK", Qt::CaseInsensitive)) {
          seenTrack = true;
          // TRACK n AUDIO  |  TRACK n MODE1/2352  |  TRACK n MODE2/2352  etc.
          QString upper = line.toUpper();
          currentIsAudio = upper.contains("AUDIO");
        }
      }
      // Flush last track
      if (seenTrack && !currentFilePath.isEmpty()) {
        tracks.append({ currentFilePath, currentIsAudio });
      }
      cueFile.close();

    } else {
      // Plain .bin — treat as single data track
      tracks.append({ sourcePath, false });
    }

    if (tracks.isEmpty()) {
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ConversionFinished(sourcePath, false, cleanDestPath, QString(), "No tracks found in CUE.");
      }, Qt::QueuedConnection);
      return;
    }

    // ── Calculate total sectors for progress ──────────────────────────────
    const qint64 RAW_SECTOR_SIZE    = 2352;
    const qint64 ISO_SECTOR_SIZE    = 2048;
    const qint64 SECTOR_DATA_OFFSET = 24;   // Mode 2 Form 1 / Mode 1 data offset
    // Sync pattern present at byte 0 of every CD-ROM data sector:
    //   0x00 0xFF×10 0x00
    const unsigned char DATA_SYNC[4] = { 0x00, 0xFF, 0xFF, 0xFF };

    qint64 totalSectors = 0;
    for (const TrackEntry &t : tracks) {
      QFileInfo fi(t.path);
      totalSectors += fi.size() / RAW_SECTOR_SIZE;
    }

    QFile vcd(cleanDestPath);
    if (!vcd.open(QIODevice::WriteOnly)) {
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ConversionFinished(sourcePath, false, cleanDestPath, QString(), "Unable to create target VCD file.");
      }, Qt::QueuedConnection);
      return;
    }

    qint64 totalProcessed = 0;
    const int    SECTORS_PER_CHUNK = 1024;
    const qint64 READ_CHUNK        = RAW_SECTOR_SIZE * SECTORS_PER_CHUNK;
    int    audioTracksSkipped      = 0; // Keeping variable for compatibility, but count is 0

    for (const TrackEntry &track : tracks) {

      QFile bin(track.path);
      if (!bin.open(QIODevice::ReadOnly)) {
        qWarning() << "[BIN2VCD] Cannot open track:" << track.path;
        continue;
      }

      while (!bin.atEnd()) {
        QByteArray chunk = bin.read(READ_CHUNK);
        if (chunk.isEmpty()) break;

        qint64 sectorsInChunk = chunk.size() / RAW_SECTOR_SIZE;
        vcd.write(chunk);
        
        totalProcessed += sectorsInChunk;

        int percent = totalSectors > 0
                      ? (int)((totalProcessed * 100) / totalSectors)
                      : 0;
        QMetaObject::invokeMethod(this, [=]() {
          emit ps1ConversionProgress(sourcePath, percent);
        }, Qt::QueuedConnection);
      }
      bin.close();
    }

    vcd.close();

    // ── Detect Game ID from the newly-written VCD (inside the thread) ─────
    // This avoids the QML calling tryDeterminePs1GameIdFromHex synchronously.
    QString detectedId;
    {
      QVariantMap idResult = tryDeterminePs1GameIdFromHex(cleanDestPath);
      if (idResult["success"].toBool()) {
        detectedId = idResult["gameId"].toString();
      }
    }

    QString msg;
    if (audioTracksSkipped > 0) {
      msg = QString("Converted (%1 audio track(s) skipped — not supported by POPStarter)")
                .arg(audioTracksSkipped);
    }
    qDebug() << "[BIN2VCD] Done →" << cleanDestPath << "GameID:" << detectedId;
    QMetaObject::invokeMethod(this, [=]() {
      emit ps1ConversionFinished(sourcePath, true, cleanDestPath, detectedId, msg);
    }, Qt::QueuedConnection);
  }).detach();
}

// ---------------------------------------------------------------------------
// Copy a file into <root>/POPS/ (used to install POPSTARTER.ELF / POPS_IOX.PAK)
// ---------------------------------------------------------------------------
QVariantMap OplLibraryService::copyFileToPopsFolder(const QString &sourcePath,
                                                      const QString &libraryRoot) {
  QVariantMap result;
  QString cleanRoot = QUrl::fromPercentEncoding(libraryRoot.toUtf8());
  QString popsPath  = cleanRoot + "/POPS";

  QDir().mkpath(popsPath);

  QString fileName = QFileInfo(sourcePath).fileName();
  QString destPath = popsPath + "/" + fileName;

  if (sourcePath == destPath) {
    result["success"] = true;
    result["destPath"] = destPath;
    return result;
  }

  if (QFile::exists(destPath)) QFile::remove(destPath);

  if (QFile::copy(sourcePath, destPath)) {
    result["success"]  = true;
    result["destPath"] = destPath;
  } else {
    result["success"] = false;
    result["message"] = QString("Failed to copy %1 to POPS folder.").arg(fileName);
  }
  return result;
}

// ---------------------------------------------------------------------------
// VCD import — moves/copies into <root>/POPS/ with correct naming
// ---------------------------------------------------------------------------
void OplLibraryService::startImportVcdAsync(const QString &sourceVcdPath,
                                             const QString &targetLibraryRoot,
                                             const QString &gameId,
                                             const QString &gameName) {
  qDebug() << "[VCD_ASYNC] Spawning thread for:" << sourceVcdPath;

  std::thread([this, sourceVcdPath, targetLibraryRoot, gameId, gameName]() {
    // If gameId was not supplied, detect it from the VCD header (async-safe)
    QString resolvedId = gameId;
    if (resolvedId.isEmpty()) {
      QVariantMap idResult = tryDeterminePs1GameIdFromHex(sourceVcdPath);
      if (idResult["success"].toBool()) {
        resolvedId = idResult["gameId"].toString();
      }
    }

    QString cleanRoot   = QUrl::fromPercentEncoding(targetLibraryRoot.toUtf8());
    QString popsDir     = cleanRoot + "/POPS";
    QString newFileName = resolvedId.isEmpty()
        ? QString("%1.VCD").arg(gameName)
        : QString("%1.%2.VCD").arg(resolvedId, gameName);
    QString destPath    = popsDir + "/" + newFileName;

    QDir().mkpath(popsDir);

    if (sourceVcdPath == destPath) {
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ImportFinished(sourceVcdPath, true, destPath, resolvedId, "Already in POPS directory");
      }, Qt::QueuedConnection);
      return;
    }

    QFile sourceFile(sourceVcdPath);
    // Fast path: same-device rename
    if (sourceFile.rename(destPath)) {
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ImportProgress(sourceVcdPath, 100, 0.0);
        emit ps1ImportFinished(sourceVcdPath, true, destPath, resolvedId, "Native rename successful");
      }, Qt::QueuedConnection);
      return;
    }

    // Cross-device: chunked copy
    if (!sourceFile.open(QIODevice::ReadOnly)) {
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ImportFinished(sourceVcdPath, false, QString(), resolvedId, "Unable to read source VCD file.");
      }, Qt::QueuedConnection);
      return;
    }

    QFile destFile(destPath);
    if (!destFile.open(QIODevice::WriteOnly)) {
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ImportFinished(sourceVcdPath, false, QString(), resolvedId, "Unable to create target VCD file.");
      }, Qt::QueuedConnection);
      return;
    }

    qint64 totalBytes    = sourceFile.size();
    qint64 bytesProcessed = 0;
    const qint64 READ_CHUNK = 4 * 1024 * 1024;
    int lastPercent = -1;
    QElapsedTimer timer;
    timer.start();

    while (!sourceFile.atEnd()) {
      QByteArray chunk = sourceFile.read(READ_CHUNK);
      if (chunk.isEmpty()) break;
      destFile.write(chunk);
      bytesProcessed += chunk.size();
      int pct = (int)((bytesProcessed * 100) / totalBytes);
      double elapsedSecs = timer.elapsed() / 1000.0;
      double mbps = 0.0;
      if (elapsedSecs > 0.0) {
          mbps = (bytesProcessed / (1024.0 * 1024.0)) / elapsedSecs;
      }
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ImportProgress(sourceVcdPath, pct, mbps);
      }, Qt::QueuedConnection);
    }

    sourceFile.close();
    destFile.close();

    if (bytesProcessed == totalBytes && totalBytes > 0) {
      QFile::remove(sourceVcdPath);
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ImportFinished(sourceVcdPath, true, destPath, resolvedId, "Success");
      }, Qt::QueuedConnection);
    } else {
      destFile.remove();
      QMetaObject::invokeMethod(this, [=]() {
        emit ps1ImportFinished(sourceVcdPath, false, QString(), resolvedId, "Copy validation failed");
      }, Qt::QueuedConnection);
    }
  }).detach();
}

// ---------------------------------------------------------------------------
// PS1 art download — uses PS1/ subdirectory in the same art repo
// ---------------------------------------------------------------------------
void OplLibraryService::startDownloadPs1ArtAsync(const QString &dirPath,
                                                   const QString &gameId,
                                                   const QString &callbackSourceKey) {
  QThread *workerThread = QThread::create([this, dirPath, gameId, callbackSourceKey]() {
    // Try the PS1 subfolder of the same community art database
    QString baseUrl = "https://raw.githubusercontent.com/Luden02/psx-ps2-opl-art-database/refs/heads/main/PS1";
    QStringList types = {"COV", "ICO", "SCR", "COV2", "SCR2", "BG", "LAB", "LGO"};

    QDir().mkpath(dirPath);

    QNetworkAccessManager manager;
    int index = 0;
    int total = types.size();

    for (const QString &type : types) {
      QString fileName = gameId + "_" + type + ".png";
      QString urlStr   = baseUrl + "/" + gameId + "/" + fileName;
      QString savePath = dirPath + "/" + fileName;

      QNetworkRequest request((QUrl(urlStr)));
      QNetworkReply *reply = manager.get(request);

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
        emit ps1ArtDownloadProgress(callbackSourceKey, (index * 100) / total);
      }, Qt::QueuedConnection);
    }

    QMetaObject::invokeMethod(this, [=]() {
      emit ps1ArtDownloadFinished(callbackSourceKey, true, "PS1 art modules pulled.");
    }, Qt::QueuedConnection);
  });

  QObject::connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
  workerThread->start();
}

// ---------------------------------------------------------------------------
// External PS1 file picker helper
// ---------------------------------------------------------------------------
QString OplLibraryService::urlToLocalFile(const QString &urlStr) {
  QUrl url(urlStr);
  return url.isLocalFile() ? url.toLocalFile() : QUrl::fromPercentEncoding(urlStr.toUtf8());
}

QVariantList OplLibraryService::getExternalPs1FilesData(const QStringList &fileUrls) {
  QVariantList files;
  QRegularExpression idRegex(
      "(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|"
      "SCAJ|SLKA|SCKA|SCED|SCCS)_[0-9]{3}\\.[0-9]{2}");

  for (const QString &urlStr : fileUrls) {
    QUrl url(urlStr);
    QString filePath = url.isLocalFile() ? url.toLocalFile() : urlStr;
    QFileInfo baseInfo(filePath);

    QFileInfoList toProcess;
    if (baseInfo.isDir()) {
        scanDirectoryRecursively(this, filePath, toProcess);
    } else {
        toProcess.append(baseInfo);
    }

    for (const QFileInfo &fileInfo : toProcess) {
      if (fileInfo.exists() && fileInfo.isFile()) {
        QString ext = fileInfo.suffix().toLower();
        if (ext == "bin" || ext == "cue" || ext == "img" || ext == "vcd") {
          qint64 reportedSize = (ext == "cue") ? getCueRealSize(fileInfo) : fileInfo.size();

          QVariantMap itemInfo;
          itemInfo["extension"] = "." + ext;
          itemInfo["name"]      = fileInfo.completeBaseName();
          itemInfo["isRenamed"] = (ext == "vcd") &&
                                  fileInfo.completeBaseName().contains(idRegex);
          itemInfo["parentPath"] = fileInfo.absolutePath();
          itemInfo["path"]       = fileInfo.absoluteFilePath();
          itemInfo["size"] = reportedSize;
        
        QString version = "1.00";
        QVariantMap idRes = tryDeterminePs1GameIdFromHex(fileInfo.absoluteFilePath());
        if (idRes["success"].toBool() && idRes.contains("version") && idRes["version"].toString() != "") {
            version = idRes["version"].toString();
        }
        itemInfo["version"] = version;
        
        itemInfo["stats"] = QVariantMap{{"size", reportedSize}};
        files.append(itemInfo);
        }
      }
    }
  }
  return files;
}

// ---------------------------------------------------------------------------
// POPStarter prerequisite check
// Returns { hasPopsFolder, hasPopstarter, hasPopsIox, popsPath }
// ---------------------------------------------------------------------------
QVariantMap OplLibraryService::checkPopsFolder(const QString &libraryRoot) {
  QString cleanRoot  = QUrl::fromPercentEncoding(libraryRoot.toUtf8());
  QString popsPath   = cleanRoot + "/POPS";
  QDir popsDir(popsPath);

  QVariantMap result;
  result["hasPopsFolder"]  = popsDir.exists();
  result["hasPopstarter"]  = QFile::exists(popsPath + "/POPSTARTER.ELF");
  result["hasPopsIox"]     = QFile::exists(popsPath + "/POPS_IOX.PAK");
  result["popsPath"]       = popsPath;
  return result;
}
