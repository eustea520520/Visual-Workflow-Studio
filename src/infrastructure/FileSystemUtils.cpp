#include "infrastructure/FileSystemUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace vws::infrastructure::FileSystemUtils {

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

bool ensureDirectory(const QString& path, QString* errorMessage)
{
    if (path.trimmed().isEmpty()) {
        setError(errorMessage, "Directory path is empty.");
        return false;
    }

    QDir directory(path);
    if (directory.exists()) {
        return true;
    }

    if (!directory.mkpath(".")) {
        setError(errorMessage, QString("Could not create directory: %1").arg(path));
        return false;
    }
    return true;
}

QStringList listFiles(const QString& directoryPath, const QStringList& nameFilters)
{
    QDir directory(directoryPath);
    const auto entries = directory.entryInfoList(
        nameFilters,
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    QStringList files;
    for (const auto& entry : entries) {
        files.append(entry.absoluteFilePath());
    }
    return files;
}

bool copyFile(const QString& sourcePath, const QString& targetPath, QString* errorMessage)
{
    if (!QFileInfo::exists(sourcePath)) {
        setError(errorMessage, QString("Source file does not exist: %1").arg(sourcePath));
        return false;
    }

    const QFileInfo targetInfo(targetPath);
    if (!ensureDirectory(targetInfo.absolutePath(), errorMessage)) {
        return false;
    }

    if (QFileInfo::exists(targetPath) && !QFile::remove(targetPath)) {
        setError(errorMessage, QString("Could not overwrite target file: %1").arg(targetPath));
        return false;
    }

    if (!QFile::copy(sourcePath, targetPath)) {
        setError(errorMessage, QString("Could not copy %1 to %2").arg(sourcePath, targetPath));
        return false;
    }
    return true;
}

QString safeFileStem(const QString& value, const QString& fallback)
{
    auto stem = value.trimmed();
    stem.replace(QRegularExpression("[^A-Za-z0-9_\\-]+"), "_");
    stem.replace(QRegularExpression("_+"), "_");
    stem = stem.trimmed();
    while (stem.startsWith('_')) {
        stem.remove(0, 1);
    }
    while (stem.endsWith('_')) {
        stem.chop(1);
    }

    return stem.isEmpty() ? fallback : stem;
}

} // namespace vws::infrastructure::FileSystemUtils
