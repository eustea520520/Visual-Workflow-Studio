#pragma once

#include <QString>
#include <QStringList>

namespace vws::infrastructure::FileSystemUtils {

// 创建目录。目录已存在时返回 true。
bool ensureDirectory(const QString& path, QString* errorMessage = nullptr);

// 返回目录下匹配后缀的文件绝对路径，按文件名排序。
QStringList listFiles(const QString& directoryPath, const QStringList& nameFilters);

// 复制文件，同时确保目标父目录存在。
bool copyFile(const QString& sourcePath, const QString& targetPath, QString* errorMessage = nullptr);

// 把用户输入的名称变成可安全用于文件名的字符串。
QString safeFileStem(const QString& value, const QString& fallback = "untitled");

} // namespace vws::infrastructure::FileSystemUtils
