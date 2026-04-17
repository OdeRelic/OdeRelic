#include "gdrom_builder.h"
#include "dreamcast_iso_builder.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QDebug>
#include <QtEndian>
#include <QQueue>
#include <QSet>
#define LIBARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>

GdromBuilder::GdromBuilder() {}

bool GdromBuilder::buildMenuGdrom(const QString &lowDataDir, const QString &highDataDir, const QString &destDir) {
    QDir().mkpath(destDir);
    
    QString track01 = destDir + "/track01.iso";
    QString track02 = destDir + "/track02.raw";
    QString track03 = destDir + "/track03.iso";
    QString track04 = destDir + "/track04.raw";
    QString track05 = destDir + "/track05.iso";
    
    QFile::remove(track02); QFile::copy(":/openmenu/track02.raw", track02);
    QFile::remove(track04); QFile::copy(":/openmenu/track04.raw", track04);

    // Track 1
    if (!createIso9660Track(lowDataDir, track01, "OPENMENU")) {
        qWarning() << "[GdromBuilder] Failed to create track01.iso using libarchive.";
        return false;
    }

    // Track 3
    DreamcastIsoBuilder t03Builder;
    t03Builder.setBaseLba(45000);
    
    QString resDir = QCoreApplication::applicationDirPath() + "/../resources/dreamcast/openmenu";
    if (!QDir(resDir).exists()) resDir = "resources/dreamcast/openmenu";
    
    // 1. Calculate total payload size for Track 5
    quint32 totalSectors = 0;
    quint32 firstReadSectors = 0;
    QString firstReadPath;
    
    QList<QString> track5Files; // Store absolute paths to write to track05
    QDirIterator it(highDataDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString absolutePath = it.fileInfo().absoluteFilePath();
        track5Files.append(absolutePath);
        quint32 secs = t03Builder.padToSector(it.fileInfo().size()) / 2048;
        if (it.fileInfo().fileName().toUpper() == "1ST_READ.BIN") {
            firstReadSectors = secs;
            firstReadPath = absolutePath;
        } else {
            totalSectors += secs;
        }
    }

    QList<QString> dats = {"BOX.DAT", "ICON.DAT", "META.DAT"};
    for (const QString &dat : dats) {
        QFileInfo fi(resDir + "/" + dat);
        if (fi.exists()) {
            totalSectors += (t03Builder.padToSector(fi.size()) / 2048);
        }
    }

    // GD-ROM Physical limit is 549150. Pin 1ST_READ.BIN to 150 sectors before the end.
    quint32 t5Lba = 549150 - 150 - firstReadSectors - totalSectors;
    quint32 t4Lba = t5Lba - 151; // 1 sector for track 4 audio + 150 gap
    
    quint32 extLba = t5Lba;
    
    // 2. Map all natural files to Track 05
    for (const QString &absolutePath : track5Files) {
        if (absolutePath == firstReadPath) {
            t03Builder.overrideFileLba(absolutePath, 549150 - 150 - firstReadSectors);
        } else {
            t03Builder.overrideFileLba(absolutePath, extLba);
            QFileInfo fi(absolutePath);
            extLba += (t03Builder.padToSector(fi.size()) / 2048);
        }
    }

    // 3. Inject external DB files to Track 05
    QMap<QString, quint32> datOffsets;
    for (const QString &dat : dats) {
        QFileInfo fi(resDir + "/" + dat);
        if (fi.exists()) {
            t03Builder.injectFile(dat, extLba, fi.size());
            datOffsets[dat] = extLba;
            extLba += (t03Builder.padToSector(fi.size()) / 2048);
        }
    }
    
    if (!t03Builder.build(highDataDir, track03, resDir + "/IP.BIN")) {
        qWarning() << "[GdromBuilder] Failed to create track03.iso using native builder.";
        return false;
    }

    // Track 5 (Direct emit of ALL mapped data)
    QFile out5(track05);
    if (out5.open(QIODevice::WriteOnly)) {
        // Size track05 to fit all files up to 1ST_READ.BIN
        quint32 endLba = (549150 - 150);
        out5.resize((endLba - t5Lba) * 2048);
        
        // Write the naturally mapped files
        quint32 currentWriteLba = t5Lba;
        for (const QString &filePath : track5Files) {
            if (filePath == firstReadPath) {
                QFile in(filePath);
                if (in.open(QIODevice::ReadOnly)) {
                    out5.seek((549150 - 150 - firstReadSectors - t5Lba) * 2048);
                    QByteArray data = in.readAll();
                    data.append(QByteArray(t03Builder.padToSector(data.size()) - data.size(), 0));
                    out5.write(data);
                    in.close();
                }
            } else {
                QFile in(filePath);
                if (in.open(QIODevice::ReadOnly)) {
                    out5.seek((currentWriteLba - t5Lba) * 2048);
                    QByteArray data = in.readAll();
                    data.append(QByteArray(t03Builder.padToSector(data.size()) - data.size(), 0));
                    out5.write(data);
                    in.close();
                    currentWriteLba += (t03Builder.padToSector(data.size()) / 2048);
                }
            }
        }
        
        // Write the injected DB files
        for (const QString &dat : dats) {
            if (datOffsets.contains(dat)) {
                QFile in(resDir + "/" + dat);
                if (in.open(QIODevice::ReadOnly)) {
                    out5.seek((datOffsets[dat] - t5Lba) * 2048);
                    QByteArray data = in.readAll();
                    data.append(QByteArray(t03Builder.padToSector(data.size()) - data.size(), 0));
                    out5.write(data);
                    in.close();
                }
            }
        }
        out5.close();
    } else {
        qWarning() << "[GdromBuilder] Failed to write track05.iso directly.";
        return false;
    }
    
    QString discGdi = destDir + "/disc.gdi";
    if (!generateGdiFile(discGdi, t4Lba, t5Lba)) {
        qWarning() << "[GdromBuilder] Failed to build disc.gdi.";
        return false;
    }

    return true;
}

bool GdromBuilder::createIso9660Track(const QString &sourceDir, const QString &destIsoPath, const QString &volumeId) {
    struct archive *a;
    struct archive_entry *entry;
    struct stat st;
    char buff[8192];
    int len;

    a = archive_write_new();
    archive_write_set_format_iso9660(a);
    
    // Explicitly configure ISO9660 boolean fields to UNSET using nullptr
    // This forcibly prevents libarchive from inflating tracks or adding boot-busting SVD pointers
    archive_write_set_option(a, "iso9660", "rockridge", nullptr);
    archive_write_set_option(a, "iso9660", "joliet", nullptr);
    archive_write_set_option(a, "iso9660", "pad", nullptr);

    // Provide the correct visual volume name label to the root
    if (!volumeId.isEmpty()) {
        archive_write_set_option(a, "iso9660", "volume-id", volumeId.toUtf8().constData());
    }

    if (archive_write_open_filename(a, destIsoPath.toUtf8().constData()) != ARCHIVE_OK) {
        qWarning() << "[GdromBuilder] Archive fatal:" << archive_error_string(a);
        archive_write_free(a);
        return false;
    }

    QDirIterator it(sourceDir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString filePath = it.fileInfo().absoluteFilePath();
        QString relPath = QDir(sourceDir).relativeFilePath(filePath);

        entry = archive_entry_new();
#ifdef Q_OS_WIN
        qint64 size = it.fileInfo().size();
        if (it.fileInfo().isDir()) {
            archive_entry_set_filetype(entry, AE_IFDIR);
            archive_entry_set_perm(entry, 0755);
            archive_entry_set_size(entry, 0);
        } else {
            archive_entry_set_size(entry, size);
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
        }
#else
        stat(filePath.toUtf8().constData(), &st);
        archive_entry_copy_stat(entry, &st);
#endif
        // Set standard attributes avoiding extensions collision dynamically.
        archive_entry_set_pathname(entry, relPath.toUtf8().constData());
        archive_write_header(a, entry);

        if (it.fileInfo().isDir()) {
            archive_entry_free(entry);
            continue;
        }

        FILE *f = fopen(filePath.toUtf8().constData(), "rb");
        if (f) {
            while ((len = fread(buff, 1, sizeof(buff), f)) > 0) {
                archive_write_data(a, buff, len);
            }
            fclose(f);
        }
        archive_entry_free(entry);
    }

    archive_write_close(a);
    archive_write_free(a);

    return true;
}

bool GdromBuilder::generateGdiFile(const QString &destGdiPath, uint32_t t4Lba, uint32_t t5Lba) {
    QFile file(destGdiPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString gdiContent = QString("5\n"
                             "1 0 4 2048 track01.iso 0\n"
                             "2 450 0 2352 track02.raw 0\n"
                             "3 45000 4 2048 track03.iso 0\n"
                             "4 %1 0 2352 track04.raw 0\n"
                             "5 %2 4 2048 track05.iso 0\n").arg(t4Lba).arg(t5Lba);
        
        file.write(gdiContent.toUtf8());
        file.close();
        return true;
    }
    return false;
}
