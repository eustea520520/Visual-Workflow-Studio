#pragma once

#include <QJsonObject>
#include <QString>

namespace vws::infrastructure::JsonUtils {

// 通用 JSON 文件读写工具。
// 注意：这里不理解 Workflow/Node 等业务含义，只负责 QFile/QJsonDocument 层面的读写。
bool readObjectFromFile(const QString& filePath, QJsonObject& object, QString* errorMessage = nullptr);
bool writeObjectToFile(const QString& filePath, const QJsonObject& object, QString* errorMessage = nullptr);

} // namespace vws::infrastructure::JsonUtils
