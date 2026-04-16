#ifndef GDROM_BUILDER_H
#define GDROM_BUILDER_H

#include <QString>
#include <QStringList>
#include <QDir>
#include <QVariantMap>

class GdromBuilder {
public:
    GdromBuilder();

    /**
     * @brief Builds a valid GDROM disc structure out of source data directories.
     * @param lowDataDir Directory containing the inner ring payload (e.g., OPENMENU.INI).
     * @param highDataDir Directory containing the outer ring payload (e.g., 1ST_READ.BIN, theme data).
     * @param destDir The target directory where track01.iso, track03.iso, track04.raw, track05.iso, and disc.gdi will be generated.
     * @return True if the GDROM was successfully built, false otherwise.
     */
    bool buildMenuGdrom(const QString &lowDataDir, const QString &highDataDir, const QString &destDir);

private:
    /**
     * @brief Creates a standard ISO9660 track using libarchive.
     * @param sourceDir Directory whose contents will be packed into the ISO.
     * @param destIsoPath Absolute path to the output ISO file.
     * @param volumeId ISO volume identifier label.
     * @return True if successful, false otherwise.
     */
    bool createIso9660Track(const QString &sourceDir, const QString &destIsoPath, const QString &volumeId = "CDROM");

    /**
     * @brief Generates the GDI toc descriptor file.
     * @param destGdiPath Path to write the GDI file.
     * @param t4Lba LBA offset for track04.
     * @param t5Lba LBA offset for track05.
     * @return True if successful, false otherwise.
     */
    bool generateGdiFile(const QString &destGdiPath, uint32_t t4Lba, uint32_t t5Lba);
};

#endif // GDROM_BUILDER_H
