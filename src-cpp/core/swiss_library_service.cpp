#include "swiss_library_service.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QString>
#include <QThread>
#include <QUrl>
#include "filesystem_cache.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QStandardPaths>
#include <thread>

SwissLibraryService::SwissLibraryService(QObject *parent) : QObject(parent) {
    m_cancelRequested = false;
}

void SwissLibraryService::cancelAllImports() {
    m_cancelRequested = true;
}

void SwissLibraryService::resetCancelFlag() {
    m_cancelRequested = false;
}

QString SwissLibraryService::urlToLocalFile(const QString &urlStr) {
#ifdef Q_OS_WIN
  if (urlStr.startsWith("file:///")) {
    return QUrl(urlStr).toLocalFile();
  }
#else
  if (urlStr.startsWith("file://")) {
    return QUrl(urlStr).toLocalFile();
  }
#endif
  return urlStr;
}

void SwissLibraryService::startGetGamesFilesAsync(const QString &dirPath) {
  std::thread([this, dirPath]() {
    QVariantMap result;
    QVariantList files;
    QHash<QString, QVariantMap> groupedFiles;
    
    QVariantMap cacheData = FileSystemCache::loadCache(dirPath, "gc");
    bool cacheDirty = false;

    // Swiss supports games anywhere. Iterate root recursively. 
    QStringList subDirs = { "." };
    
    int index = 0;
    int total = subDirs.size();
    
    for (const QString &subDir : subDirs) {
      index++;
      QMetaObject::invokeMethod(this, [=]() {
        emit libraryScanProgress(index, total);
      }, Qt::QueuedConnection);
      
      QString targetDirStr = dirPath + (subDir == "." ? "" : "/" + subDir);
      QDir dir(targetDirStr);
      
      if (!dir.exists()) continue;

      QDirIterator it(targetDirStr, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
      
      while(it.hasNext()) {
        it.next();
        QFileInfo fileInfo = it.fileInfo();
        if (fileInfo.fileName().startsWith("._")) continue;
        QString ext = fileInfo.suffix().toLower();
        
        if (ext == "iso" || ext == "gcm") {
          QVariantMap itemInfo;
          itemInfo["extension"] = "." + ext;
          QString baseName = fileInfo.completeBaseName();
          QString parentName = fileInfo.dir().dirName();
          
          if (baseName.toLower() == "game" || baseName.toLower().startsWith("disc")) {
              itemInfo["name"] = parentName;
          } else {
              itemInfo["name"] = baseName;
          }
          
          // Check if parent directory or filename has Game ID like [GALE01]
          QRegularExpression idRegex("\\[([A-Z0-9]{6})\\]");
          itemInfo["isRenamed"] = idRegex.match(fileInfo.absoluteFilePath()).hasMatch();

          qint64 actualSize = fileInfo.size();
          QString version = "1.00";
          QString pathKey = fileInfo.fileName() + "_" + QString::number(actualSize);
          
          if (cacheData.contains(pathKey)) {
              QVariantMap cacheItem = cacheData[pathKey].toMap();
              if (cacheItem["size"].toLongLong() == actualSize) {
                  version = cacheItem["version"].toString();
              }
          } else {
              QVariantMap idRes = tryDetermineGameIdFromHex(fileInfo.absoluteFilePath());
              if (idRes["success"].toBool() && idRes.contains("version")) {
                  version = idRes["version"].toString();
              }
              QVariantMap newCacheItem;
              newCacheItem["mtime"] = fileInfo.lastModified().toSecsSinceEpoch();
              newCacheItem["size"] = actualSize;
              newCacheItem["version"] = version;
              cacheData[pathKey] = newCacheItem;
              cacheDirty = true;
          }

          itemInfo["parentPath"] = fileInfo.absolutePath();
          itemInfo["path"] = fileInfo.absoluteFilePath();
          itemInfo["size"] = actualSize;
          itemInfo["version"] = version;
          itemInfo["stats"] = QVariantMap{{"size", actualSize}};
          
          QString groupKey = itemInfo["name"].toString();
          if (groupedFiles.contains(groupKey)) {
              QVariantMap existing = groupedFiles[groupKey];
              existing["discCount"] = existing["discCount"].toInt() + 1;
              existing["size"] = existing["size"].toLongLong() + actualSize;
              QVariantMap stats = existing["stats"].toMap();
              stats["size"] = stats["size"].toLongLong() + actualSize;
              existing["stats"] = stats;
              groupedFiles[groupKey] = existing;
          } else {
              itemInfo["discCount"] = 1;
              groupedFiles[groupKey] = itemInfo;
          }
        }
      }
    }
    
    for (auto it = groupedFiles.begin(); it != groupedFiles.end(); ++it) {
        files.append(it.value());
    }
    
    if (cacheDirty) {
        FileSystemCache::saveCache(dirPath, cacheData, "gc");
    }

    result["success"] = true;
    result["data"] = files;
    
    QMetaObject::invokeMethod(this, [this, dirPath, result]() {
      emit gamesFilesLoaded(dirPath, result);
    }, Qt::QueuedConnection);

  }).detach();
}

QVariantMap SwissLibraryService::tryDetermineGameIdFromHex(const QString &filepath) {
  QVariantMap result;

  QFile file(filepath);
  if (!file.open(QIODevice::ReadOnly)) {
    result["success"] = false;
    result["message"] = "Unable to open file.";
    return result;
  }

  // Gamecube game id is 6 bytes at offset 0
  QByteArray header = file.read(6);
  file.close();

  if (header.size() == 6) {
      QString gameId = QString::fromLatin1(header);
      
      // Basic check to see if it looks like a 6-character game ID (alphanumeric)
      QRegularExpression regex("^[A-Z0-9]{6}$");
      if (regex.match(gameId).hasMatch()) {
          result["success"] = true;
          result["gameId"] = gameId;
          result["version"] = "1.00"; // Usually don't parse GC version here easily
          result["formattedGameId"] = gameId;
          return result;
      }
  }

  result["success"] = false;
  result["message"] = "Could not locate a GameCube game ID inside the provided file.";
  return result;
}

bool SwissLibraryService::provisionCheat(const QString& gameId, const QString& libraryRoot) {
    if (gameId.isEmpty() || libraryRoot.isEmpty()) return false;
    QString cleanId = gameId.toUpper();

    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/assets/cheats/gamecube - wii",
        QCoreApplication::applicationDirPath() + "/../../../assets/cheats/gamecube - wii",
        QCoreApplication::applicationDirPath() + "/../Resources/assets/cheats/gamecube - wii",
        QDir::currentPath() + "/assets/cheats/gamecube - wii"
    };

    QString sourceCheatPath = "";
    for (const QString& path : searchPaths) {
        QString candidate = path + "/" + cleanId + ".txt";
        if (QFile::exists(candidate)) {
            sourceCheatPath = candidate;
            break;
        }
    }

    if (sourceCheatPath.isEmpty()) {
        return false;
    }

    QString cheatsDir = QUrl::fromPercentEncoding(libraryRoot.toUtf8()) + "/cheats";
    QDir().mkpath(cheatsDir);

    QString destCheatPath = cheatsDir + "/" + cleanId + ".txt";
    if (QFile::exists(destCheatPath)) {
        QFile::remove(destCheatPath);
    }
    
    return QFile::copy(sourceCheatPath, destCheatPath);
}

int SwissLibraryService::syncCheats(const QString &libraryRoot) {
    if (libraryRoot.isEmpty()) return 0;
    QString cleanRoot = QUrl::fromPercentEncoding(libraryRoot.toUtf8());
    QDir gamesDir(cleanRoot + "/games");
    if (!gamesDir.exists()) return 0;

    int syncCount = 0;
    QStringList folders = gamesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &folderName : folders) {
        // Extract Game ID from folder name e.g. "Game Name [GM8E01]"
        QRegularExpression regex("\\[([A-Za-z0-9]{6})\\]");
        QRegularExpressionMatch match = regex.match(folderName);
        if (match.hasMatch()) {
            QString gameId = match.captured(1);
            if (provisionCheat(gameId, libraryRoot)) {
                syncCount++;
            }
        }
    }
    return syncCount;
}

QVariantMap SwissLibraryService::renameGamefile(const QString &dirpath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName) {
  QVariantMap result;
  QFileInfo fileInfo(dirpath);
  QString ext = fileInfo.suffix().toLower();

  QString cleanRoot = QUrl::fromPercentEncoding(targetLibraryRoot.toUtf8());
  
  QString cleanName = gameName;
  bool isMultiDisc = false;
  int discNum = 1;
  QRegularExpression discRegex("\\(Disc\\s*([1-9])\\)", QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch match = discRegex.match(cleanName);
  if (match.hasMatch()) {
      isMultiDisc = true;
      discNum = match.captured(1).toInt();
      cleanName.remove(match.captured(0));
      cleanName = cleanName.trimmed();
      // Clean up potential trailing hyphens
      while (cleanName.endsWith("-")) {
          cleanName.chop(1);
          cleanName = cleanName.trimmed();
      }
  }
  
  cleanName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), ""); // sanitize
  
  QString folderName = QString("%1 [%2]").arg(cleanName, gameId);
  QString finalDir = cleanRoot + "/games/" + folderName;
  
  QDir().mkpath(finalDir);
  QString isoName = isMultiDisc ? QString("disc %1").arg(discNum) : "game";
  QString newFilePath = finalDir + "/" + isoName + "." + ext;

  if (dirpath == newFilePath) {
    result["success"] = true;
    result["newPath"] = newFilePath;
    return result;
  }

  if (dirpath.contains("/.orbit_temp_")) {
      // This is an internal temporary file created by our BIN2ISO converter, so we can securely move it
      QFile file(dirpath);
      if (file.rename(newFilePath)) {
        result["success"] = true;
        result["newPath"] = newFilePath;
      } else {
        if (QFile::copy(dirpath, newFilePath)) {
          QFile::remove(dirpath);
          provisionCheat(gameId, targetLibraryRoot);
          result["success"] = true;
          result["newPath"] = newFilePath;
        } else {
          result["success"] = false;
          result["message"] = file.errorString();
        }
      }
  } else {
      // Strict User File Preservation: ONLY copy the file, do not remove the original
      if (QFile::copy(dirpath, newFilePath)) {
        provisionCheat(gameId, targetLibraryRoot);
        result["success"] = true;
        result["newPath"] = newFilePath;
      } else {
        QFile file(dirpath);
        result["success"] = false;
        result["message"] = file.errorString();
      }
  }

  return result;
}

void SwissLibraryService::startImportIsoAsync(const QString &sourceIsoPath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName) {
  std::thread([this, sourceIsoPath, targetLibraryRoot, gameId, gameName]() {
    QFileInfo fileInfo(sourceIsoPath);
    QString ext = fileInfo.suffix().toLower();
    
    QString cleanRoot = QUrl::fromPercentEncoding(targetLibraryRoot.toUtf8());
    QString cleanName = gameName;
    bool isMultiDisc = false;
    int discNum = 1;
    QRegularExpression discRegex("\\(Disc\\s*([1-9])\\)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = discRegex.match(cleanName);
    if (match.hasMatch()) {
        isMultiDisc = true;
        discNum = match.captured(1).toInt();
        cleanName.remove(match.captured(0));
        cleanName = cleanName.trimmed();
        while (cleanName.endsWith("-")) {
            cleanName.chop(1);
            cleanName = cleanName.trimmed();
        }
    }
    
    cleanName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "");
    
    QString folderName = QString("%1 [%2]").arg(cleanName, gameId);
    QString finalDir = cleanRoot + "/games/" + folderName;
    QDir().mkpath(finalDir);
    
    QString isoName = isMultiDisc ? QString("disc %1").arg(discNum) : "game";
    QString destIsoPath = finalDir + "/" + isoName + "." + ext;

    if (sourceIsoPath == destIsoPath) {
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, true, destIsoPath, "Already in target directory");
      }, Qt::QueuedConnection);
      return;
    }

    // PRESERVE: We do not invoke sourceFile.rename() anymore so we don't accidentally rip the file away from the user's origin folder
    QFile sourceFile(sourceIsoPath);
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
    const qint64 READ_CHUNK = 4 * 1024 * 1024;

    int lastPercent = -1;

    while (!sourceFile.atEnd()) {
      if (m_cancelRequested.load()) {
          sourceFile.close();
          destFile.close();
          destFile.remove();
          QMetaObject::invokeMethod(this, [=]() {
            emit importIsoFinished(sourceIsoPath, false, "", "Import canceled by user.");
          }, Qt::QueuedConnection);
          return;
      }

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

    if (bytesProcessed == totalBytes && totalBytes > 0) {
      // PRESERVE: Removing the QFile::remove(sourceIsoPath) block. Origin payload is preserved.
      provisionCheat(gameId, targetLibraryRoot);
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, true, destIsoPath, "Success");
      }, Qt::QueuedConnection);
    } else {
      destFile.remove();
      QMetaObject::invokeMethod(this, [=]() {
        emit importIsoFinished(sourceIsoPath, false, "", "Copy validation failed");
      }, Qt::QueuedConnection);
    }
  }).detach();
}

void SwissLibraryService::startDownloadArtAsync(const QString &dirPath, const QString &gameId, const QString &callbackSourceKey) {
  QThread *workerThread = QThread::create([this, dirPath, gameId, callbackSourceKey]() {
    QDir().mkpath(dirPath);

    QNetworkAccessManager manager;
    
    // We try to pull from GameTDB US region for gamecube. E.g. https://art.gametdb.com/wii/cover/US/GALE01.png
    // If not found, try EN. 
    QStringList regions = {"US", "EN", "JA", "EU"};
    bool success = false;
    
    for (int i=0; i < regions.size(); i++) {
        QString urlStr = "https://art.gametdb.com/wii/cover/" + regions[i] + "/" + gameId + ".png";
        QString savePath = dirPath + "/" + gameId + "_COV.png";

        QNetworkRequest request((QUrl(urlStr)));
        QNetworkReply *reply = manager.get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
          QByteArray data = reply->readAll();
          if (!data.isEmpty()) {
              QFile file(savePath);
              if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                success = true;
              }
          }
        }
        reply->deleteLater();
        if (success) {
            break;
        }
    }

    QMetaObject::invokeMethod(this, [=]() {
        emit artDownloadProgress(callbackSourceKey, 100);
        emit artDownloadFinished(callbackSourceKey, success, success ? "Downloaded art successfully." : "Art not found on GameTDB for GameCube.");
    }, Qt::QueuedConnection);
  });

  QObject::connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
  workerThread->start();
}

QVariantMap SwissLibraryService::moveFile(const QString &sourcePath, const QString &destPath) {
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

QVariantMap SwissLibraryService::deleteFile(const QString &sourceRawPath) {
  QVariantMap result;
  QFile::remove(sourceRawPath);
  result["success"] = true;
  return result;
}

namespace {
  void scanDirectoryRecursivelyGc(SwissLibraryService* self, const QString &dirPath, QFileInfoList &outFiles) {
      int totalFolders = 0;
      QDirIterator dirIt(dirPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
      while (dirIt.hasNext()) { dirIt.next(); totalFolders++; }

      QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
      
      int processedFolders = 0;
      QString lastDir = "";

      while (it.hasNext()) {
          it.next();
          QFileInfo fi = it.fileInfo();
          if (fi.isFile() && !fi.fileName().startsWith("._")) {
              QString currentDir = fi.absolutePath();
              
              if (currentDir != lastDir) {
                  lastDir = currentDir;
                  processedFolders++;
                  if (self && totalFolders > 0) {
                      QMetaObject::invokeMethod(self, [self, processedFolders, totalFolders]() {
                          emit self->externalFilesScanProgress(processedFolders, totalFolders);
                      }, Qt::QueuedConnection);
                  }
              }
              
              QString ext = fi.suffix().toLower();
              if (ext == "iso" || ext == "gcm") {
                 outFiles.append(fi);
              }
          }
      }
  }
}

QVariantList SwissLibraryService::getExternalGameFilesData(const QStringList &fileUrls) {
  QVariantList files;
  
  for (const QString &urlStr : fileUrls) {
      QUrl url(urlStr);
      QString filePath = url.isLocalFile() ? url.toLocalFile() : urlStr;
      QFileInfo baseInfo(filePath);
      
      QFileInfoList toProcess;
      if (baseInfo.isDir()) {
          scanDirectoryRecursivelyGc(this, filePath, toProcess);
      } else {
          QString ext = baseInfo.suffix().toLower();
          if (ext == "iso" || ext == "gcm") {
              toProcess.append(baseInfo);
          }
      }

      for (const QFileInfo &fileInfo : toProcess) {
          if (fileInfo.exists() && fileInfo.isFile()) {
              QVariantMap itemInfo;
              itemInfo["extension"] = "." + fileInfo.suffix().toLower();
              QString baseName = fileInfo.completeBaseName();
              QString parentName = fileInfo.dir().dirName();
              
              if (baseName.toLower() == "game" || baseName.toLower().startsWith("disc")) {
                  itemInfo["name"] = parentName;
              } else {
                  itemInfo["name"] = baseName;
              }
              QRegularExpression idRegex("\\[([A-Z0-9]{6})\\]");
              itemInfo["isRenamed"] = idRegex.match(fileInfo.absoluteFilePath()).hasMatch();
              
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

void SwissLibraryService::scanExternalFilesAsync(const QStringList &fileUrls, bool dummy) {
    (void)dummy;
    std::thread([this, fileUrls]() {
        QVariantList files = getExternalGameFilesData(fileUrls);
        
        QMetaObject::invokeMethod(this, [this, files]() {
            emit externalFilesScanFinished(false, files);
        }, Qt::QueuedConnection);
    }).detach();
}

QVariantMap SwissLibraryService::checkSwissFolder(const QString &rootPath) {
    QVariantMap result;
    QDir dir(rootPath);
    bool hasGames = dir.exists("games");
    bool hasApps = dir.exists("apps");
    bool hasDol = dir.exists("dol");
    bool hasSwiss = dir.exists("swiss");
    bool hasSwissFile = dir.exists("boot.iso") || dir.exists("ipl.dol") || dir.exists("autoexec.dol") || dir.exists("swiss_version.json");
    
    result["isValid"] = hasGames && hasApps && hasDol && hasSwiss && hasSwissFile;
    result["hasGames"] = hasGames;
    result["hasApps"] = hasApps;
    result["hasDol"] = hasDol;
    result["hasSwiss"] = hasSwiss;
    result["hasSwissFile"] = hasSwissFile;
    
    return result;
}

QVariantMap SwissLibraryService::createSwissFolder(const QString &rootPath) {
    QVariantMap result;
    QDir dir(rootPath);
    
    bool createdGames = dir.mkpath("games");
    bool createdApps = dir.mkpath("apps");
    bool createdDol = dir.mkpath("dol");
    
    bool createdSwiss = dir.mkpath("swiss");
    dir.mkpath("swiss/patches");
    dir.mkpath("swiss/settings");
    
    result["success"] = createdGames && createdApps && createdDol && createdSwiss;
    return result;
}

void SwissLibraryService::startSwissSetupAsync(const QString &rootPath, const QString &odeType) {
    std::thread([this, rootPath, odeType]() {
        QMetaObject::invokeMethod(this, [=]() { emit setupSwissProgress(5, "Contacting GitHub API..."); }, Qt::QueuedConnection);
        
        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl("https://api.github.com/repos/emukidid/swiss-gc/releases/latest"));
        request.setHeader(QNetworkRequest::UserAgentHeader, "OdeRelic-Swiss-Installer");
        
        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
        if (reply->error() != QNetworkReply::NoError) {
            QMetaObject::invokeMethod(this, [=]() { emit setupSwissFinished(false, "Failed to reach GitHub API: " + reply->errorString()); }, Qt::QueuedConnection);
            reply->deleteLater();
            return;
        }
        
        QByteArray responseData = reply->readAll();
        reply->deleteLater();
        
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject rootObj = doc.object();
        QString fetchedVersion = rootObj["tag_name"].toString("unknown");
        QJsonArray assets = rootObj["assets"].toArray();
        QString targetDownloadUrl = "";
        
        for (int i = 0; i < assets.size(); ++i) {
            QJsonObject asset = assets[i].toObject();
            QString name = asset["name"].toString();
            if (name.endsWith(".tar.xz")) {
                targetDownloadUrl = asset["browser_download_url"].toString();
                break;
            }
        }
        
        if (targetDownloadUrl.isEmpty()) {
            QMetaObject::invokeMethod(this, [=]() { emit setupSwissFinished(false, "Could not find valid .tar.xz archive in the latest release."); }, Qt::QueuedConnection);
            return;
        }
        
        QMetaObject::invokeMethod(this, [=]() { emit setupSwissProgress(20, "Downloading Swiss release..."); }, Qt::QueuedConnection);
        
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/oderelic_swiss";
        QDir().mkpath(tempDir);
        QString archivePath = tempDir + "/swiss_release.tar.xz";
        
        QNetworkRequest dlRequest((QUrl(targetDownloadUrl)));
        dlRequest.setHeader(QNetworkRequest::UserAgentHeader, "OdeRelic-Swiss-Installer");
        dlRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        
        QNetworkReply *dlReply = manager.get(dlRequest);
        QEventLoop dlLoop;
        QObject::connect(dlReply, &QNetworkReply::finished, &dlLoop, &QEventLoop::quit);
        dlLoop.exec();
        
        if (dlReply->error() != QNetworkReply::NoError) {
            QMetaObject::invokeMethod(this, [=]() { emit setupSwissFinished(false, "Failed to download Swiss archive: " + dlReply->errorString()); }, Qt::QueuedConnection);
            dlReply->deleteLater();
            return;
        }
        
        QFile dlFile(archivePath);
        if (dlFile.open(QIODevice::WriteOnly)) {
            dlFile.write(dlReply->readAll());
            dlFile.close();
        }
        dlReply->deleteLater();
        
        QMetaObject::invokeMethod(this, [=]() { emit setupSwissProgress(60, "Extracting payload via OS native bindings..."); }, Qt::QueuedConnection);
        
        QProcess tarProcess;
        tarProcess.setProgram("tar");
        tarProcess.setArguments({"-xf", archivePath, "-C", tempDir});
        tarProcess.start();
        tarProcess.waitForFinished(-1);
        
        if (tarProcess.exitCode() != 0) {
            QMetaObject::invokeMethod(this, [=]() { emit setupSwissFinished(false, "Failed to extract tar archive using OS native bindings."); }, Qt::QueuedConnection);
            return;
        }
        
        QMetaObject::invokeMethod(this, [=]() { emit setupSwissProgress(85, "Configuring ODE Boot payload..."); }, Qt::QueuedConnection);
        
        QDir extractDir(tempDir);
        QDirIterator it(tempDir, QDir::Files, QDirIterator::Subdirectories);
        QString dolPath = "";
        QString isoPath = "";
        
        while (it.hasNext()) {
            it.next();
            QFileInfo fi = it.fileInfo();
            if (fi.absolutePath().endsWith("/DOL") && fi.suffix() == "dol" && fi.fileName().startsWith("swiss_r")) {
                dolPath = fi.absoluteFilePath();
            }
            if (fi.absolutePath().endsWith("/ISO") && fi.suffix() == "iso" && fi.fileName().startsWith("swiss_r") && fi.fileName().contains("ntsc-u", Qt::CaseInsensitive)) {
                isoPath = fi.absoluteFilePath();
            }
            if (isoPath.isEmpty() && fi.absolutePath().endsWith("/ISO") && fi.suffix() == "iso" && fi.fileName().startsWith("swiss_r")) {
                isoPath = fi.absoluteFilePath();
            }
        }
        
        QString targetRoot = QUrl::fromPercentEncoding(rootPath.toUtf8());
        
        if (odeType == "PicoBoot" || odeType == "Standalone") {
            if (dolPath.isEmpty()) {
                QMetaObject::invokeMethod(this, [=]() { emit setupSwissFinished(false, "Could not find a valid DOL file in the Github release bundle."); }, Qt::QueuedConnection);
                return;
            }
            QString finalDolName = (odeType == "PicoBoot") ? "ipl.dol" : "autoexec.dol";
            QFile::remove(targetRoot + "/" + finalDolName);
            QFile::copy(dolPath, targetRoot + "/" + finalDolName);
            
            // IGR (In-Game Reset) backwards compatibility integration
            QFile::remove(targetRoot + "/igr.dol");
            QFile::copy(dolPath, targetRoot + "/igr.dol");
        } else if (odeType == "GC Loader") {
            if (isoPath.isEmpty()) {
                QMetaObject::invokeMethod(this, [=]() { emit setupSwissFinished(false, "Could not find a valid ISO file in the Github release bundle."); }, Qt::QueuedConnection);
                return;
            }
            QFile::remove(targetRoot + "/boot.iso");
            QFile::copy(isoPath, targetRoot + "/boot.iso");
        }
        
        // Deploy safe foundational hierarchy
        createSwissFolder(targetRoot);
        
        if (!dolPath.isEmpty()) {
            QFile::remove(targetRoot + "/dol/" + QFileInfo(dolPath).fileName());
            QFile::copy(dolPath, targetRoot + "/dol/" + QFileInfo(dolPath).fileName());
        }
        
        extractDir.removeRecursively(); // Cleanup temp safely
        
        // Save native Swiss deployment identity
        QJsonObject identity;
        identity["version"] = fetchedVersion;
        identity["ode"] = odeType;
        QJsonDocument idDoc(identity);
        QFile idFile(targetRoot + "/swiss_version.json");
        if (idFile.open(QIODevice::WriteOnly)) {
            idFile.write(idDoc.toJson());
            idFile.close();
        }
        
        QMetaObject::invokeMethod(this, [=]() { emit setupSwissProgress(100, "Setup complete!"); }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(this, [=]() { emit setupSwissFinished(true, "Swiss ecosystem has been installed correctly."); }, Qt::QueuedConnection);
        
    }).detach();
}

void SwissLibraryService::checkSwissUpdateAsync(const QString &rootPath) {
    std::thread([this, rootPath]() {
        QString identityPath = QUrl::fromPercentEncoding(rootPath.toUtf8()) + "/swiss_version.json";
        QFile f(identityPath);
        if (!f.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(this, [=]() { emit swissUpdateCheckFinished(false, "", "", ""); }, Qt::QueuedConnection);
            return;
        }
        QByteArray data = f.readAll();
        f.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QString localVersion = obj["version"].toString();
        QString savedOde = obj["ode"].toString();
        
        if (localVersion.isEmpty() || savedOde.isEmpty()) {
            QMetaObject::invokeMethod(this, [=]() { emit swissUpdateCheckFinished(false, "", "", ""); }, Qt::QueuedConnection);
            return;
        }

        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl("https://api.github.com/repos/emukidid/swiss-gc/releases/latest"));
        request.setHeader(QNetworkRequest::UserAgentHeader, "OdeRelic-Swiss-Installer");
        
        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
        if (reply->error() != QNetworkReply::NoError) {
            QMetaObject::invokeMethod(this, [=]() { emit swissUpdateCheckFinished(false, localVersion, "", savedOde); }, Qt::QueuedConnection);
            reply->deleteLater();
            return;
        }
        
        QByteArray responseData = reply->readAll();
        reply->deleteLater();
        
        QJsonDocument rDoc = QJsonDocument::fromJson(responseData);
        QJsonObject rObj = rDoc.object();
        QString remoteVersion = rObj["tag_name"].toString();
        
        bool updateAvailable = (!remoteVersion.isEmpty() && remoteVersion != localVersion);
        
        QMetaObject::invokeMethod(this, [=]() { emit swissUpdateCheckFinished(updateAvailable, localVersion, remoteVersion, savedOde); }, Qt::QueuedConnection);
        
    }).detach();
}
