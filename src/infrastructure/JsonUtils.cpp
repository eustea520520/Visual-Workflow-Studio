#include "infrastructure/JsonUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>

namespace vws::infrastructure::JsonUtils {

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

bool readObjectFromFile(const QString& filePath, QJsonObject& object, QString* errorMessage)
{
    // 统一处理“文件打不开、JSON 解析失败、根节点不是对象”等基础错误。
    // 上层服务只需要关心 true/false 和 errorMessage。
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(errorMessage, QString("Could not open JSON file for reading: %1").arg(file.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(errorMessage, QString("Could not parse JSON file at offset %1: %2")
            .arg(parseError.offset)
            .arg(parseError.errorString()));
        return false;
    }

    if (!document.isObject()) {
        setError(errorMessage, "JSON root must be an object.");
        return false;
    }

    object = document.object();
    return true;
}

bool writeObjectToFile(const QString& filePath, const QJsonObject& object, QString* errorMessage)
{
    // 写文件前确保父目录存在，这样保存 workflow.json 时不用每个调用方都重复 mkpath。
    const QFileInfo fileInfo(filePath);
    const auto parentPath = fileInfo.absolutePath();
    if (!parentPath.isEmpty()) {
        QDir parentDir(parentPath);
        if (!parentDir.exists() && !parentDir.mkpath(".")) {
            setError(errorMessage, QString("Could not create directory: %1").arg(parentPath));
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setError(errorMessage, QString("Could not open JSON file for writing: %1").arg(file.errorString()));
        return false;
    }

    const QJsonDocument document(object);
    file.write(document.toJson(QJsonDocument::Indented));
    return true;
}

} // namespace vws::infrastructure::JsonUtils
