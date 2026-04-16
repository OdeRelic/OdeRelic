#include "ps1_xstation_library_service.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QUrl>
#include <QtConcurrent>
#include <filesystem>
#include <thread>

#include "../../core/common/system_utils.h"
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/types.h>
#include <utime.h>
#endif

#include "../../core/logging/logger.h"

PS1XstationLibraryService::PS1XstationLibraryService(QObject *parent)
    : QObject(parent) {}

QString PS1XstationLibraryService::urlToLocalFile(const QString &urlStr) {
  QUrl url(urlStr);
  return url.isLocalFile() ? url.toLocalFile() : urlStr;
}

void PS1XstationLibraryService::cancelAllImports() {
  cancelRequested.store(true);
}

void PS1XstationLibraryService::resetCancelFlag() {
  cancelRequested.store(false);
}

QVariantMap
PS1XstationLibraryService::checkXStationFolder(const QString &rootPath) {
  QVariantMap result;
  QString cleanPath = urlToLocalFile(rootPath);
  QDir dir(cleanPath);

  bool hasXStationFolder = dir.exists("00xstation");
  bool hasLoader = false;
  bool hasUpdate = false;
  QString version = "Unknown";
  QString odeType = "XStation";

  if (hasXStationFolder) {
    QDir xsDir(cleanPath + "/00xstation");
    hasLoader = xsDir.exists("loader.bin");
    hasUpdate = xsDir.exists("update.bin");

    QFile idFile(cleanPath + "/00xstation/xstation_version.json");
    if (idFile.open(QIODevice::ReadOnly)) {
      QJsonObject obj = QJsonDocument::fromJson(idFile.readAll()).object();
      if (obj.contains("version"))
        version = obj["version"].toString();
      idFile.close();
    }
  }

  result["hasXStationFolder"] = hasXStationFolder;
  result["hasLoader"] = hasLoader;
  result["hasUpdate"] = hasUpdate;
  result["isSetup"] = hasXStationFolder && hasLoader && hasUpdate;
  result["version"] = version;
  result["odeType"] = odeType;

  return result;
}

QVariantMap
PS1XstationLibraryService::tryDetermineGameIdFromHex(const QString &filepath) {
  QVariantMap result;
  QString targetPath = filepath;
  QFileInfo fi(filepath);

  qInfo() << "[XStation Scraper] Executing ID Extraction on ->" << targetPath;
  if (fi.suffix().toLower() == "cue") {
    qInfo() << "[XStation Scraper] CUE string detected, tracking mapped binary "
               "internally.";
    QFile cueFile(filepath);
    if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&cueFile);
      while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("FILE")) {
          int firstQuote = line.indexOf('"');
          int lastQuote = line.lastIndexOf('"');
          if (firstQuote != -1 && lastQuote != -1 && lastQuote > firstQuote) {
            QString binFileName =
                line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
            QFileInfo binInfo(fi.absolutePath() + "/" + binFileName);
            if (binInfo.exists()) {
              targetPath = binInfo.absoluteFilePath();
              qInfo()
                  << "[XStation Scraper] Located natively resolved payload ->"
                  << binFileName;
              break;
            }
          }
        }
      }
      cueFile.close();
    }
  }

  QFile file(targetPath);
  qInfo() << "[XStation Scraper] Executing hex chunk mapping on ->"
          << targetPath;
  if (!file.open(QIODevice::ReadOnly)) {
    result["success"] = false;
    result["message"] = "Unable to open file.";
    qCritical() << "PS1 ID Scraper: Fatal natively reading payload bounds.";
    return result;
  }

  const qint64 CHUNK_SIZE = 1024 * 1024; // 1 MB
  const qint64 OVERLAP = 64;
  const qint64 MAX_SEARCH_BYTES =
      8 * 1024 * 1024; // Enforce max 8MB PS1 bound preventing freezes
  qint64 totalRead = 0;
  QByteArray carry;

  QRegularExpression bootRegex(
      "BOOT\\s*=\\s*cdrom:\\\\([A-Z]{4}_[0-9]{3}\\.[0-9]{2})(?:;1)?",
      QRegularExpression::CaseInsensitiveOption);

  QRegularExpression bareIdRegex(
      "(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|SCAJ|SLKA|SCKA|SCED|SCCS)"
      "_[0-9]{3}\\.[0-9]{2}(?:;1)?");

  while (!file.atEnd() && totalRead < MAX_SEARCH_BYTES) {
    QByteArray buffer = file.read(CHUNK_SIZE);
    if (buffer.isEmpty())
      break;

    QByteArray chunk = carry + buffer;
    QString chunkStr = QString::fromLatin1(chunk);

    QRegularExpressionMatch bootMatch = bootRegex.match(chunkStr);
    if (bootMatch.hasMatch()) {
      QString gameId = bootMatch.captured(1).toUpper();
      if (gameId.endsWith(";1"))
        gameId.chop(2);
      QString formatted = gameId;

      file.close();
      result["success"] = true;
      result["gameId"] = formatted;
      qInfo() << "[XStation Scraper] Natively matched internal `BOOT=` "
                 "descriptor ->"
              << formatted;
      return result;
    }

    QRegularExpressionMatch bareMatch = bareIdRegex.match(chunkStr);
    if (bareMatch.hasMatch()) {
      QString gameId = bareMatch.captured(0).toUpper();
      if (gameId.endsWith(";1"))
        gameId.chop(2);
      QString formatted = gameId;

      file.close();
      result["success"] = true;
      result["gameId"] = formatted;
      qInfo() << "[XStation Scraper] Natively matched bare sector serial ->"
              << formatted;
      return result;
    }

    if (chunk.size() > OVERLAP)
      carry = chunk.right(OVERLAP);
    else
      carry = chunk;
  }

  file.close();
  result["success"] = false;
  qWarning() << "PS1 ID Scraper: Completed scraping bounds; no valid "
                "identifiers naturally found.";
  return result;
}

QVariantMap
PS1XstationLibraryService::createXStationFolder(const QString &rootPath) {
  QVariantMap result;
  QString cleanPath = urlToLocalFile(rootPath);
  QDir dir(cleanPath);

  if (!dir.exists("00xstation")) {
    if (dir.mkdir("00xstation")) {
      result["success"] = true;
      result["message"] = "00xstation directory successfully injected.";
    } else {
      result["success"] = false;
      result["message"] =
          "Failed to create 00xstation volume. Check physical locking switch.";
      return result;
    }
  } else {
    result["success"] = true;
    result["message"] = "00xstation already established natively.";
  }

  return result;
}

void PS1XstationLibraryService::startXStationSetupAsync(
    const QString &rootPath, const QString &odeType) {
  QtConcurrent::run([this, rootPath, odeType]() {
    QNetworkAccessManager manager;
    QEventLoop loop;
    QNetworkRequest apiRequest(QUrl("https://api.github.com/repos/x-station/"
                                    "xstation-releases/releases/latest"));
    apiRequest.setRawHeader("User-Agent", "OdeRelic-XStation/1.0");

    QMetaObject::invokeMethod(
        this,
        [=]() {
          emit setupXStationProgress(5,
                                     "Contacting XStation Github Core API...");
        },
        Qt::QueuedConnection);

    QNetworkReply *apiReply = manager.get(apiRequest);
    QObject::connect(apiReply, &QNetworkReply::finished, &loop,
                     &QEventLoop::quit);
    loop.exec();

    if (apiReply->error() != QNetworkReply::NoError) {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit setupXStationFinished(false,
                                       "Failed to reach GitHub API securely.");
          },
          Qt::QueuedConnection);
      apiReply->deleteLater();
      return;
    }

    QJsonObject jsonResponse =
        QJsonDocument::fromJson(apiReply->readAll()).object();
    apiReply->deleteLater();

    QString fetchedVersion = jsonResponse["tag_name"].toString();
    QJsonArray assets = jsonResponse["assets"].toArray();
    QString downloadUrl;

    for (const QJsonValue &asset : assets) {
      QString assetName = asset.toObject()["name"].toString();
      if (assetName.endsWith(".zip") && assetName.startsWith("update")) {
        downloadUrl = asset.toObject()["browser_download_url"].toString();
        break;
      }
    }

    if (downloadUrl.isEmpty()) {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit setupXStationFinished(false,
                                       "Could not locate valid update.zip "
                                       "asset within latest release bounds.");
          },
          Qt::QueuedConnection);
      return;
    }

    QMetaObject::invokeMethod(
        this,
        [=]() {
          emit setupXStationProgress(
              20, "Downloading native XStation firmware array...");
        },
        Qt::QueuedConnection);

    QString tempDir = QDir::tempPath() + "/oderelic_xstation_" +
                      QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempDir);
    QString archivePath = tempDir + "/update.zip";

    QNetworkRequest dlRequest((QUrl(downloadUrl)));
    dlRequest.setRawHeader("User-Agent", "OdeRelic-XStation/1.0");
    QNetworkReply *dlReply = manager.get(dlRequest);
    QObject::connect(dlReply, &QNetworkReply::finished, &loop,
                     &QEventLoop::quit);
    loop.exec();

    if (dlReply->error() != QNetworkReply::NoError) {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit setupXStationFinished(false,
                                       "Network interruption detected cleanly "
                                       "halting deployment natively.");
          },
          Qt::QueuedConnection);
      dlReply->deleteLater();
      return;
    }

    QFile dlFile(archivePath);
    if (dlFile.open(QIODevice::WriteOnly)) {
      dlFile.write(dlReply->readAll());
      dlFile.close();
    }
    dlReply->deleteLater();

    QMetaObject::invokeMethod(
        this,
        [=]() {
          emit setupXStationProgress(
              60, "Extracting payload mapping via OS native zipper...");
        },
        Qt::QueuedConnection);

    QProcess unzipProcess;
    unzipProcess.setProgram("unzip");
    unzipProcess.setArguments({"-o", archivePath, "-d", tempDir});
    unzipProcess.start();
    unzipProcess.waitForFinished(-1);

    if (unzipProcess.exitCode() != 0) {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit setupXStationFinished(
                false, "Failed to natively extract zipper mapped package.");
          },
          Qt::QueuedConnection);
      return;
    }

    QMetaObject::invokeMethod(
        this,
        [=]() {
          emit setupXStationProgress(
              85, "Flashing extracted structural nodes identically across SD "
                  "Card boundaries...");
        },
        Qt::QueuedConnection);

    QString targetRoot = urlToLocalFile(rootPath);
    createXStationFolder(rootPath);
    QString xsRoot = targetRoot + "/00xstation";

    QDirIterator it(tempDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      it.next();
      QFileInfo fi = it.fileInfo();
      if (fi.fileName() == "update.bin" || fi.fileName() == "loader.bin") {
        QFile::remove(xsRoot + "/" + fi.fileName());
        QFile::copy(fi.absoluteFilePath(), xsRoot + "/" + fi.fileName());
      }
    }

    QDir(tempDir).removeRecursively();

    QJsonObject identity;
    identity["version"] = fetchedVersion;
    identity["ode"] = odeType;
    QJsonDocument idDoc(identity);
    QFile idFile(xsRoot + "/xstation_version.json");
    if (idFile.open(QIODevice::WriteOnly)) {
      idFile.write(idDoc.toJson());
      idFile.close();
    }

    QMetaObject::invokeMethod(
        this, [=]() { emit setupXStationProgress(100, "Setup complete!"); },
        Qt::QueuedConnection);
    QMetaObject::invokeMethod(
        this,
        [=]() {
          emit setupXStationFinished(
              true,
              "XStation PS1 OS deployment fully integrated successfully!");
        },
        Qt::QueuedConnection);
  });
}

void PS1XstationLibraryService::checkXStationUpdateAsync(
    const QString &rootPath) {
  // Empty stub
}

void PS1XstationLibraryService::scanExternalFilesAsync(
    const QStringList &fileUrls, bool dummy) {
  QtConcurrent::run([this, fileUrls]() {
    QVariantList files = getExternalGameFilesData(fileUrls);
    QMetaObject::invokeMethod(
        this, [this, files]() { emit externalFilesScanFinished(true, files); },
        Qt::QueuedConnection);
  });
}

static void scanDirectoryRecursivelyXs(PS1XstationLibraryService *service,
                                       const QString &dirPath,
                                       QFileInfoList &toProcess) {
  QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    QString ext = it.fileInfo().suffix().toLower();
    if (ext == "iso" || ext == "bin" || ext == "cue" || ext == "img" ||
        ext == "chd") {
      toProcess.append(it.fileInfo());
    }
  }
}

QVariantList PS1XstationLibraryService::getExternalGameFilesData(
    const QStringList &fileUrls) {
  QVariantList files;
  for (const QString &urlStr : fileUrls) {
    QUrl url(urlStr);
    QString filePath = url.isLocalFile() ? url.toLocalFile() : urlStr;
    QFileInfo baseInfo(filePath);

    QFileInfoList toProcess;
    if (baseInfo.isDir()) {
      scanDirectoryRecursivelyXs(this, filePath, toProcess);
    } else {
      QString ext = baseInfo.suffix().toLower();
      if (ext == "iso" || ext == "bin" || ext == "cue" || ext == "img" ||
          ext == "chd") {
        toProcess.append(baseInfo);
      }
    }

    QFileInfoList filtered;
    for (const QFileInfo &fi : toProcess) {
      if (fi.suffix().toLower() == "bin") {
        QDir d(fi.absolutePath());
        QStringList cues = d.entryList(QStringList() << "*.cue", QDir::Files);
        if (!cues.isEmpty())
          continue; // Let the CUE intelligently map its tracks natively safely.
      }
      filtered.append(fi);
    }

    for (const QFileInfo &fileInfo : filtered) {
      if (fileInfo.exists() && fileInfo.isFile()) {
        QVariantMap itemInfo;
        QString ext = fileInfo.suffix().toLower();
        itemInfo["extension"] = "." + ext;
        itemInfo["name"] = fileInfo.completeBaseName();
        itemInfo["parentPath"] = fileInfo.absolutePath();
        itemInfo["path"] = fileInfo.absoluteFilePath();
        qint64 rSize = (ext == "cue")
                           ? SystemUtils::calculateCueRealSize(fileInfo)
                           : fileInfo.size();
        itemInfo["size"] = rSize;
        itemInfo["stats"] = QVariantMap{{"size", rSize}};
        // Fake ID detection mapping
        QRegularExpression idRegex(
            "(SLUS|SCUS|SLES|SCES|SLPM|SLPS|SCPS|SCPM|SLAJ|SCAJ|SLKA|SCKA|SCED|"
            "SCCS)[_-]?\\d{3}\\.?\\d{2}");
        itemInfo["isRenamed"] =
            idRegex.match(fileInfo.absoluteFilePath()).hasMatch();
        files.append(itemInfo);
      }
    }
  }
  return files;
}

void PS1XstationLibraryService::startImportIsoAsync(
    const QString &sourceIsoPath, const QString &targetLibraryRoot,
    const QString &gameId, const QString &gameName) {

  qInfo() << "[XStation Importer] Spawning import thread for:" << sourceIsoPath;
  QtConcurrent::run([this, sourceIsoPath, targetLibraryRoot, gameId,
                     gameName]() {
    QFileInfo srcInfo(sourceIsoPath);
    QString ext = srcInfo.suffix().toLower();
    QString safeDirName = gameName;
    safeDirName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");

    // Group identical multi-disc instances into a single parent entity
    // accurately
    safeDirName.remove(
        QRegularExpression("\\s*\\(Disc\\s*[0-9A-Za-z]+\\)",
                           QRegularExpression::CaseInsensitiveOption));
    safeDirName.remove(
        QRegularExpression("\\s*-\\s*Disc\\s*[0-9A-Za-z]+",
                           QRegularExpression::CaseInsensitiveOption));
    safeDirName = safeDirName.trimmed();

    QString cleanTarget = urlToLocalFile(targetLibraryRoot);
    QDir targetDir(cleanTarget);
    targetDir.mkpath(safeDirName);
    QString finalDestDir = targetDir.absoluteFilePath(safeDirName);

    QFileInfoList itemsToCopy;
    itemsToCopy.append(srcInfo);

    if (ext == "cue") {
      QFile cueFile(srcInfo.absoluteFilePath());
      if (cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&cueFile);
        while (!in.atEnd()) {
          QString line = in.readLine().trimmed();
          if (line.startsWith("FILE")) {
            int firstQuote = line.indexOf('"');
            int lastQuote = line.lastIndexOf('"');
            if (firstQuote != -1 && lastQuote != -1 && lastQuote > firstQuote) {
              QString binFileName =
                  line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
              QFileInfo binInfo(srcInfo.absolutePath() + "/" + binFileName);
              if (binInfo.exists()) {
                itemsToCopy.append(binInfo);
              }
            }
          }
        }
        cueFile.close();
      }
    }

    qint64 totalBytes = 0;
    for (const QFileInfo &f : itemsToCopy) {
      totalBytes += f.size();
    }

    qint64 copiedBytes = 0;
    bool allSuccess = true;
    qint64 startTimeMs = QDateTime::currentMSecsSinceEpoch();
    qint64 lastReportTime = 0;

    for (const QFileInfo &f : itemsToCopy) {
      if (cancelRequested.load()) {
        QMetaObject::invokeMethod(
            this,
            [=]() {
              emit importIsoFinished(sourceIsoPath, false, "",
                                     "Import gracefully cancelled "
                                     "unconditionally mapping correctly.");
            },
            Qt::QueuedConnection);
        return;
      }
      QString destPath = finalDestDir + "/" + f.fileName();
      if (QFile::exists(destPath))
        QFile::remove(destPath);

      QFile srcFile(f.absoluteFilePath());
      QFile destFile(destPath);

      if (!srcFile.open(QIODevice::ReadOnly) ||
          !destFile.open(QIODevice::WriteOnly)) {
        allSuccess = false;
        break;
      }

      char buffer[4096 * 16];
      qint64 bytesRead;

      while ((bytesRead = srcFile.read(buffer, sizeof(buffer))) > 0) {
        if (cancelRequested.load()) {
          destFile.close();
          srcFile.close();
          QFile::remove(destPath);
          QMetaObject::invokeMethod(
              this,
              [=]() {
                emit importIsoFinished(sourceIsoPath, false, "",
                                       "Cancelled mapping effectively.");
              },
              Qt::QueuedConnection);
          return;
        }
        destFile.write(buffer, bytesRead);
        copiedBytes += bytesRead;

        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - lastReportTime > 500) {
          int percent = totalBytes > 0
                            ? static_cast<int>((copiedBytes * 100) / totalBytes)
                            : 100;
          double elapsedTimeS = (currentTime - startTimeMs) / 1000.0;
          double MBps =
              (elapsedTimeS > 0)
                  ? (static_cast<double>(copiedBytes) / 1024.0 / 1024.0) /
                        elapsedTimeS
                  : 0.0;

          QMetaObject::invokeMethod(
              this,
              [=]() { emit importIsoProgress(sourceIsoPath, percent, MBps); },
              Qt::QueuedConnection);
          lastReportTime = currentTime;
        }
      }
      srcFile.close();
      destFile.close();
    }

    if (allSuccess) {
      QMetaObject::invokeMethod(
          this, [=]() { emit importIsoProgress(sourceIsoPath, 100, 0.0); },
          Qt::QueuedConnection);
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit importIsoFinished(
                sourceIsoPath, true, finalDestDir,
                "Import completed natively implicitly smoothly.");
          },
          Qt::QueuedConnection);
    } else {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit importIsoFinished(
                sourceIsoPath, false, "",
                "Native system error mapping I/O bytes sequentially.");
          },
          Qt::QueuedConnection);
    }
  });
}

void PS1XstationLibraryService::startDownloadArtAsync(
    const QString &dirPath, const QString &gameId,
    const QString &callbackSourceKey) {
  QtConcurrent::run([this, dirPath, gameId, callbackSourceKey]() {
    QString baseUrl = "https://raw.githubusercontent.com/Luden02/"
                      "psx-ps2-opl-art-database/refs/heads/main/PS1";
    QStringList types = {"COV", "COV2", "SCR", "SCR2"};

    QDir().mkpath(dirPath);

    QNetworkAccessManager manager;
    int index = 0;
    int total = types.size();
    bool artFound = false;

    // XStation strictly dictates explicitly sized 80x84 Windows BMP structures
    // statically inside each Game directory globally.
    QString savePath = dirPath + "/cover.bmp";

    for (const QString &type : types) {
      if (artFound)
        break;

      QString fileName = gameId + "_" + type + ".png";
      QString urlStr = baseUrl + "/" + gameId + "/" + fileName;

      QNetworkRequest request((QUrl(urlStr)));
      QNetworkReply *reply = manager.get(request);

      QEventLoop loop;
      QObject::connect(reply, &QNetworkReply::finished, &loop,
                       &QEventLoop::quit);
      loop.exec();

      if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        QImage srcImg;
        if (srcImg.loadFromData(imageData)) {
          // Downsample and explicitly structure natively into BMP format
          // exclusively accepted by XStation
          QImage resized = srcImg.scaled(80, 84, Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);

          if (QFile::exists(savePath))
            QFile::remove(savePath);
          if (resized.save(savePath, "BMP")) {
            artFound = true;
          }
        }
      }
      reply->deleteLater();

      index++;
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit artDownloadProgress(callbackSourceKey, (index * 100) / total);
          },
          Qt::QueuedConnection);
    }

    if (artFound) {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit artDownloadFinished(callbackSourceKey, true,
                                     "Native 80x84 BMP converted efficiently.");
          },
          Qt::QueuedConnection);
    } else {
      QMetaObject::invokeMethod(
          this,
          [=]() {
            emit artDownloadFinished(callbackSourceKey, false,
                                     "Luden02 backend block mapping missing.");
          },
          Qt::QueuedConnection);
    }
  });
}

void PS1XstationLibraryService::startBatchArtDownloadAsync(
    const QVariantList &gamesList) {
  qInfo() << "[XStation Artwork] Batch download started for" << gamesList.size()
          << "items";
  QtConcurrent::run([this, gamesList]() {
    int total = gamesList.size();
    int current = 0;

    QNetworkAccessManager manager;
    QString baseUrl = "https://raw.githubusercontent.com/Luden02/"
                      "psx-ps2-opl-art-database/refs/heads/main/PS1";
    QStringList types = {"COV", "COV2", "SCR", "SCR2"};

    for (const QVariant &item : gamesList) {
      if (cancelRequested.load())
        break;

      QVariantMap g = item.toMap();
      QString path = g.value("path").toString();
      QString binaryFileName = g.value("binaryFileName").toString();
      QString regexId = g.value("regexId").toString();

      QString extractedId = regexId;
      if (extractedId.isEmpty()) {
        QVariantMap hexScan =
            tryDetermineGameIdFromHex(path + "/" + binaryFileName);
        if (hexScan.value("success").toBool())
          extractedId = hexScan.value("gameId").toString();
      }

      if (!extractedId.isEmpty()) {
        bool artFound = false;
        QString savePath = path + "/cover.bmp";

        // Ensure folder existence correctly reliably cleanly
        QDir().mkpath(path);

        for (const QString &type : types) {
          if (artFound)
            break;

          QString fileName = extractedId + "_" + type + ".png";
          QString urlStr = baseUrl + "/" + extractedId + "/" + fileName;

          QNetworkRequest request((QUrl(urlStr)));
          QNetworkReply *reply = manager.get(request);

          QEventLoop loop;
          QObject::connect(reply, &QNetworkReply::finished, &loop,
                           &QEventLoop::quit);
          loop.exec();

          if (reply->error() == QNetworkReply::NoError) {
            QByteArray imageData = reply->readAll();
            QImage srcImg;
            if (srcImg.loadFromData(imageData)) {
              QImage resized = srcImg.scaled(80, 84, Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation);
              if (QFile::exists(savePath))
                QFile::remove(savePath);
              if (resized.save(savePath, "BMP")) {
                artFound = true;
              }
            }
          }
          reply->deleteLater();
        }
      }

      current++;
      QMetaObject::invokeMethod(
          this, [=]() { emit batchArtDownloadProgress(current, total); },
          Qt::QueuedConnection);
    }

    QMetaObject::invokeMethod(
        this, [=]() { emit batchArtDownloadFinished(!cancelRequested.load()); },
        Qt::QueuedConnection);
  });
}

// Stubs for compiling basic UI functions perfectly mirroring others
// sequentially
void PS1XstationLibraryService::startGetGamesFilesAsync(
    const QString &dirPath) {
  qInfo() << "[XStation Scanner] Starting async PS1 scan on directory:"
          << dirPath;
  QtConcurrent::run([this, dirPath]() {
    QString cleanT = urlToLocalFile(dirPath);
    QVariantMap result;
    QVariantList gamesList;

    QDirIterator it(cleanT, QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    int scannedFolders = 0;

    while (it.hasNext()) {
      it.next();
      scannedFolders++;
      if (scannedFolders % 10 == 0) {
        QMetaObject::invokeMethod(
            this, [=]() { emit libraryScanProgress(scannedFolders, 0); },
            Qt::QueuedConnection);
      }

      QFileInfo dirInfo = it.fileInfo();
      if (dirInfo.fileName() == "00xstation")
        continue;

      QDir gameDir(dirInfo.absoluteFilePath());
      QStringList filters;
      filters << "*.bin" << "*.cue" << "*.iso" << "*.img" << "*.chd";
      gameDir.setNameFilters(filters);

      QFileInfoList files = gameDir.entryInfoList(QDir::Files);
      if (!files.isEmpty()) {
        qint64 totalGameSize = 0;
        QFileInfoList allFiles =
            gameDir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System);
        for (const QFileInfo &fi : allFiles) {
          totalGameSize += fi.size();
        }

        QFileInfo targetFile = files.first();
        for (const QFileInfo &fi : files) {
          if (fi.suffix().toLower() == "cue") {
            targetFile = fi;
          }
        }

        QVariantMap gameNode;
        gameNode["name"] = dirInfo.fileName();
        gameNode["path"] = dirInfo.absoluteFilePath();
        gameNode["extension"] = targetFile.suffix();
        gameNode["binaryFileName"] = targetFile.fileName();
        gameNode["isRenamed"] = true; // Folders natively form the title safely
        gameNode["size"] = totalGameSize;

        QVariantMap stats;
        stats["size"] = totalGameSize;
        gameNode["stats"] = stats;

        gamesList.append(gameNode);
      }
    }

    result["data"] = gamesList;
    result["totalCount"] = gamesList.size();

    QMetaObject::invokeMethod(
        this, [=]() { emit gamesFilesLoaded(dirPath, result); },
        Qt::QueuedConnection);
  });
}

QVariantMap PS1XstationLibraryService::renameGamefile(
    const QString &dirpath, const QString &targetLibraryRoot,
    const QString &gameId, const QString &gameName) {
  return QVariantMap();
}
QVariantMap PS1XstationLibraryService::moveFile(const QString &sourcePath,
                                                const QString &destPath) {
  return QVariantMap();
}
