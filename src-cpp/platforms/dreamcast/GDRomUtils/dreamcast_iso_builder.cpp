#include "dreamcast_iso_builder.h"
#include <QFile>
#include <QDirIterator>
#include <QDebug>
#include <QtEndian>
#include <QQueue>
#include <QDateTime>

DreamcastIsoBuilder::DreamcastIsoBuilder() : m_baseLba(45000), m_volumeId("OPENMENU") {}
DreamcastIsoBuilder::~DreamcastIsoBuilder() {}

void DreamcastIsoBuilder::setBaseLba(quint32 lba) { m_baseLba = lba; }
void DreamcastIsoBuilder::setVolumeIdentifier(const QString &volId) { m_volumeId = volId; }

void DreamcastIsoBuilder::injectFile(const QString &fileName, quint32 externalLba, quint32 size) {
    m_injectedFiles[fileName] = qMakePair(externalLba, size);
}

void DreamcastIsoBuilder::overrideFileLba(const QString &absolutePath, quint32 externalLba) {
    m_overriddenLbas[absolutePath] = externalLba;
}

int DreamcastIsoBuilder::padToSector(int size) {
    int rem = size % 2048;
    return rem == 0 ? size : size + (2048 - rem);
}

QString DreamcastIsoBuilder::sanitizeIsoName(const QString &name, bool isDir) {
    QString res = name.toUpper();
    for (int i = 0; i < res.length(); ++i) {
        QChar c = res[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) {
            res[i] = '_';
        }
    }
    if (!isDir && !res.contains('.')) {
        res += ".";
    }
    // Append version 1 for ISO9660
    return res + ";1";
}

void DreamcastIsoBuilder::writeBothEndian32(quint32 value, char *buffer) {
    qToLittleEndian<quint32>(value, buffer);
    qToBigEndian<quint32>(value, buffer + 4);
}
void DreamcastIsoBuilder::writeLittleEndian32(quint32 value, char *buffer) { qToLittleEndian<quint32>(value, buffer); }
void DreamcastIsoBuilder::writeBigEndian32(quint32 value, char *buffer) { qToBigEndian<quint32>(value, buffer); }
void DreamcastIsoBuilder::writeBothEndian16(quint16 value, char *buffer) {
    qToLittleEndian<quint16>(value, buffer);
    qToBigEndian<quint16>(value, buffer + 2);
}

void DreamcastIsoBuilder::writeIsoDate(char *buffer) {
    QDateTime now = QDateTime::currentDateTimeUtc();
    buffer[0] = now.date().year() - 1900;
    buffer[1] = now.date().month();
    buffer[2] = now.date().day();
    buffer[3] = now.time().hour();
    buffer[4] = now.time().minute();
    buffer[5] = now.time().second();
    buffer[6] = 0; // GMT offset
}

void DreamcastIsoBuilder::writeIsoDateLong(char *buffer) {
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString str = now.toString("yyyyMMddhhmmss00");
    memcpy(buffer, str.toLatin1().constData(), 16);
    buffer[16] = 0; // GMT offset
}

void DreamcastIsoBuilder::parseDirectory(const QString &dirPath, IsoEntry *parentEntry) {
    QDir dir(dirPath);
    QStringList entries = dir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs, QDir::Name);
    for (const QString &name : entries) {
        QFileInfo fi(dir.filePath(name));
        IsoEntry *entry = new IsoEntry();
        entry->name = name;
        entry->absolutePath = fi.absoluteFilePath();
        entry->isDirectory = fi.isDir();
        entry->size = fi.size();
        parentEntry->children.append(entry);
        if (entry->isDirectory) {
            parseDirectory(fi.absoluteFilePath(), entry);
        }
    }
}

QByteArray DreamcastIsoBuilder::generateDirectoryRecord(IsoEntry *entry, IsoEntry *parentEntry) {
    QByteArray record(256, 0);
    char *buf = record.data();
    
    QString isoName;
    if (entry->name == ".") isoName = QString(QChar(0));
    else if (entry->name == "..") isoName = QString(QChar(1));
    else isoName = sanitizeIsoName(entry->name, entry->isDirectory);
    
    QByteArray nameBytes = isoName.toLatin1();
    int nameLen = nameBytes.size();
    
    int recordLen = 33 + nameLen;
    if (recordLen % 2 != 0) recordLen++; // pad to even
    
    buf[0] = recordLen;
    buf[1] = 0; // ext attr
    writeBothEndian32(entry->extentLba, buf + 2);
    writeBothEndian32(entry->isDirectory ? 2048 : entry->size, buf + 10);
    writeIsoDate(buf + 18);
    buf[25] = entry->isDirectory ? 0x02 : 0x00; // flags
    buf[26] = 0; // unit
    buf[27] = 0; // gap
    writeBothEndian16(1, buf + 28); // seq
    buf[32] = nameLen;
    memcpy(buf + 33, nameBytes.constData(), nameLen);
    
    record.resize(recordLen);
    return record;
}

QByteArray DreamcastIsoBuilder::generateDirectorySector(IsoEntry *dirEntry) {
    QByteArray sector;
    
    // Sort directories first, then files, both alphabetically
    std::sort(dirEntry->children.begin(), dirEntry->children.end(), [](IsoEntry* a, IsoEntry* b) {
        if (a->isDirectory != b->isDirectory) return a->isDirectory;
        return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
    });

    // Create "." and ".."
    IsoEntry dot;
    dot.name = ".";
    dot.isDirectory = true;
    dot.extentLba = dirEntry->extentLba;
    
    IsoEntry dotdot;
    dotdot.name = "..";
    dotdot.isDirectory = true;
    dotdot.extentLba = dirEntry->parentDirLba == 0 ? dirEntry->extentLba : dirEntry->parentDirLba;
    
    sector.append(generateDirectoryRecord(&dot, nullptr));
    sector.append(generateDirectoryRecord(&dotdot, nullptr));
    
    for (IsoEntry *child : dirEntry->children) {
        QByteArray rec = generateDirectoryRecord(child, dirEntry);
        // Do not span sectors
        if ((sector.size() % 2048) + rec.size() > 2048) {
            int pad = 2048 - (sector.size() % 2048);
            sector.append(QByteArray(pad, 0));
        }
        sector.append(rec);
    }
    
    // Pad end of sector
    if (sector.size() % 2048 != 0) {
        int pad = 2048 - (sector.size() % 2048);
        sector.append(QByteArray(pad, 0));
    }
    
    return sector;
}

bool DreamcastIsoBuilder::build(const QString &sourceDir, const QString &outputIsoPath, const QString &ipBinPath) {
    IsoEntry rootDir;
    rootDir.name = "";
    rootDir.isDirectory = true;
    parseDirectory(sourceDir, &rootDir);
    
    // Add explicitly mapped external files to root
    for (auto it = m_injectedFiles.begin(); it != m_injectedFiles.end(); ++it) {
        IsoEntry *ext = new IsoEntry();
        ext->name = it.key();
        ext->isDirectory = false;
        ext->size = it.value().second;
        ext->extentLba = it.value().first;
        rootDir.children.append(ext);
    }
    
    // Layout LBA map
    quint32 currentLba = m_baseLba + 16; // 16 sectors reserved for system area
    
    // We put PVD at LBA 16
    quint32 pvdLba = currentLba++;
    quint32 bootRecordLba = currentLba++;
    quint32 terminatorLba = currentLba++;
    
    // Flatten directories to allocate LBAs
    QList<IsoEntry*> allDirs;
    QQueue<IsoEntry*> q;
    q.enqueue(&rootDir);
    while (!q.isEmpty()) {
        IsoEntry *d = q.dequeue();
        allDirs.append(d);
        d->pathTableIndex = allDirs.size();
        for (IsoEntry *c : d->children) {
            if (c->isDirectory) {
                c->parentPathTableIndex = d->pathTableIndex;
                c->parentDirLba = d->extentLba;
                q.enqueue(c);
            }
        }
    }
    
    // Allocate directory LBAs
    for (IsoEntry *d : allDirs) {
        d->extentLba = currentLba;
        // Calculate size by generating it
        QByteArray dsec = generateDirectorySector(d);
        currentLba += (dsec.size() / 2048);
    }
    
    // Now Path Tables
    QByteArray lPathTable;
    QByteArray mPathTable;
    for (IsoEntry *d : allDirs) {
        QString isoName = (d == &rootDir) ? QString(QChar(0)) : sanitizeIsoName(d->name, true);
        isoName = isoName.replace(";1", ""); // Path tables don't have ;1
        QByteArray nameBytes = isoName.toLatin1();
        
        QByteArray lRec(8 + nameBytes.size(), 0);
        char *lb = lRec.data();
        lb[0] = nameBytes.size();
        lb[1] = 0; // ext attr
        writeLittleEndian32(d->extentLba, lb + 2);
        qToLittleEndian<quint16>(d == &rootDir ? 1 : d->parentPathTableIndex, lb + 6);
        memcpy(lb + 8, nameBytes.constData(), nameBytes.size());
        if (lRec.size() % 2 != 0) lRec.append('\0');
        lPathTable.append(lRec);
        
        QByteArray mRec(8 + nameBytes.size(), 0);
        char *mb = mRec.data();
        mb[0] = nameBytes.size();
        mb[1] = 0; // ext attr
        writeBigEndian32(d->extentLba, mb + 2);
        qToBigEndian<quint16>(d == &rootDir ? 1 : d->parentPathTableIndex, mb + 6);
        memcpy(mb + 8, nameBytes.constData(), nameBytes.size());
        if (mRec.size() % 2 != 0) mRec.append('\0');
        mPathTable.append(mRec);
    }
    
    quint32 ptSize = lPathTable.size();
    quint32 ptSectors = padToSector(ptSize) / 2048;
    
    quint32 lPathTableLba = currentLba;
    currentLba += ptSectors;
    quint32 mPathTableLba = currentLba;
    currentLba += ptSectors;
    
    // Allocate file LBAs (only those that are NOT externally mapped)
    for (IsoEntry *d : allDirs) {
        for (IsoEntry *c : d->children) {
            if (!c->isDirectory) {
                if (m_overriddenLbas.contains(c->absolutePath)) {
                    c->extentLba = m_overriddenLbas[c->absolutePath];
                } else if (c->extentLba == 0) { // Not injected externally
                    c->extentLba = currentLba;
                    currentLba += padToSector(c->size) / 2048;
                }
            }
        }
    }
    
    quint32 totalSectors = currentLba - m_baseLba;
    
    // Start writing ISO
    QFile isoOut(outputIsoPath);
    if (!isoOut.open(QIODevice::WriteOnly)) return false;
    
    // System Area (32KB)
    QByteArray sysArea(32768, 0);
    QFile ipBin(ipBinPath);
    if (ipBin.open(QIODevice::ReadOnly)) {
        sysArea = ipBin.read(32768);
        if (sysArea.size() < 32768) sysArea.append(QByteArray(32768 - sysArea.size(), 0));
    }
    isoOut.write(sysArea);
    
    // PVD
    QByteArray pvd(2048, 0);
    pvd[0] = 1; // type
    memcpy(pvd.data() + 1, "CD001", 5);
    pvd[6] = 1; // version
    memcpy(pvd.data() + 40, m_volumeId.leftJustified(32, ' ').toLatin1().constData(), 32);
    writeBothEndian32(totalSectors, pvd.data() + 80);
    writeBothEndian16(1, pvd.data() + 120); // set size
    writeBothEndian16(1, pvd.data() + 124); // seq
    writeBothEndian16(2048, pvd.data() + 128); // block size
    writeBothEndian32(ptSize, pvd.data() + 132); // path table size
    writeLittleEndian32(lPathTableLba, pvd.data() + 140);
    writeBigEndian32(mPathTableLba, pvd.data() + 148);
    
    QByteArray rootRec = generateDirectoryRecord(&rootDir, nullptr);
    memcpy(pvd.data() + 156, rootRec.constData(), 34); // root dir record is exactly 34 bytes for root
    
    writeIsoDateLong(pvd.data() + 813); // Creation
    writeIsoDateLong(pvd.data() + 830); // Modification
    memset(pvd.data() + 847, '0', 16); pvd[847+16] = 0; // Expiration
    memset(pvd.data() + 864, '0', 16); pvd[864+16] = 0; // Effective
    pvd[881] = 1; // File structure version
    isoOut.write(pvd);
    
    // Boot Record (optional but let's just emit terminator)
    QByteArray term(2048, 0);
    term[0] = (char)255;
    memcpy(term.data() + 1, "CD001", 5);
    term[6] = 1;
    isoOut.write(term); // Replaces boot record
    isoOut.write(term); // Terminator
    
    // Write Directories
    for (IsoEntry *d : allDirs) {
        QByteArray dsec = generateDirectorySector(d);
        isoOut.write(dsec);
    }
    
    // Write Path Tables
    lPathTable.append(QByteArray(padToSector(lPathTable.size()) - lPathTable.size(), 0));
    isoOut.write(lPathTable);
    mPathTable.append(QByteArray(padToSector(mPathTable.size()) - mPathTable.size(), 0));
    isoOut.write(mPathTable);
    
    // Write Files
    for (IsoEntry *d : allDirs) {
        for (IsoEntry *c : d->children) {
            if (!c->isDirectory && !m_injectedFiles.contains(c->name) && !m_overriddenLbas.contains(c->absolutePath)) {
                QFile f(c->absolutePath);
                if (f.open(QIODevice::ReadOnly)) {
                    QByteArray fileData = f.readAll();
                    fileData.append(QByteArray(padToSector(fileData.size()) - fileData.size(), 0));
                    isoOut.write(fileData);
                } else {
                    isoOut.write(QByteArray(padToSector(c->size), 0));
                }
            }
        }
    }
    
    isoOut.close();
    return true;
}
