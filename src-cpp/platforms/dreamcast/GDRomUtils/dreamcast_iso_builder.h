#ifndef DREAMCAST_ISO_BUILDER_H
#define DREAMCAST_ISO_BUILDER_H

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QByteArray>

struct IsoEntry {
    QString name;
    QString absolutePath;
    bool isDirectory;
    quint32 size;
    quint32 extentLba;
    quint32 parentDirLba;
    QList<IsoEntry*> children;
    
    // For sorting path tables
    int pathTableIndex;
    int parentPathTableIndex;
    
    IsoEntry() : isDirectory(false), size(0), extentLba(0), parentDirLba(0), pathTableIndex(0), parentPathTableIndex(0) {}
    ~IsoEntry() { qDeleteAll(children); }
};

class DreamcastIsoBuilder {
public:
    DreamcastIsoBuilder();
    ~DreamcastIsoBuilder();

    /**
     * @brief Sets the logical block address (LBA) where this track starts (e.g., 45000 for track03).
     * @param lba The base LBA offset.
     */
    void setBaseLba(quint32 lba);
    
    /**
     * @brief Sets the ISO9660 Volume Identifier string.
     * @param volId The string label for the volume.
     */
    void setVolumeIdentifier(const QString &volId);

    /**
     * @brief Builds the GD-ROM structure and ISO9660 filesystem.
     * @param sourceDir Directory containing the payload files to inject.
     * @param outputIsoPath Path where the generated ISO should be saved.
     * @param ipBinPath Path to the IP.BIN boot sector file (optional).
     * @return True if the ISO was built successfully, false otherwise.
     */
    bool build(const QString &sourceDir, const QString &outputIsoPath, const QString &ipBinPath);

    /**
     * @brief Injects a virtual file into the root directory pointing to an external LBA.
     * @param fileName The name of the injected file.
     * @param externalLba The physical sector LBA where the file payload exists.
     * @param size The size of the file in bytes.
     */
    void injectFile(const QString &fileName, quint32 externalLba, quint32 size);

    /**
     * @brief Overrides the LBA mapping for a specific file naturally parsed from the source directory.
     * @param absolutePath The absolute file path of the source file.
     * @param externalLba The fixed LBA the file should be mapped to in the ISO Directory Record.
     */
    void overrideFileLba(const QString &absolutePath, quint32 externalLba);

    /**
     * @brief Helper to align file sizes to the standard 2048-byte sector boundary.
     * @param size Original size in bytes.
     * @return Sector-padded size in bytes.
     */
    int padToSector(int size);

private:
    quint32 m_baseLba;
    QString m_volumeId;
    QMap<QString, QPair<quint32, quint32>> m_injectedFiles; // fileName -> { LBA, size }
    QMap<QString, quint32> m_overriddenLbas; // absolutePath -> LBA

    void parseDirectory(const QString &dirPath, IsoEntry *parentEntry);
    
    // ISO9660 Generators
    QByteArray generatePvd(quint32 rootExtent, quint32 rootSize, quint32 pathTableSize, quint32 lPathTableLba, quint32 mPathTableLba, quint32 volumeSpaceSize);
    QByteArray generateDirectoryRecord(IsoEntry *entry, IsoEntry *parentEntry = nullptr);
    QByteArray generateDirectorySector(IsoEntry *dirEntry);
    
    // Helpers
    void writeBothEndian32(quint32 value, char *buffer);
    void writeLittleEndian32(quint32 value, char *buffer);
    void writeBigEndian32(quint32 value, char *buffer);
    void writeBothEndian16(quint16 value, char *buffer);
    void writeLittleEndian16(quint16 value, char *buffer);
    void writeBigEndian16(quint16 value, char *buffer);
    void writeIsoDate(char *buffer);
    void writeIsoDateLong(char *buffer);
    
    QString sanitizeIsoName(const QString &name, bool isDir);
};

#endif // DREAMCAST_ISO_BUILDER_H
