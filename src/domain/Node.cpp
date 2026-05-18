#include "domain/Node.h"

#include <QJsonArray>
#include <QJsonValue>

namespace vws::domain {

namespace {

// 端口目前用 QStringList 表示，序列化时转换成 JSON 数组。
// 后续如果端口需要类型、显示名、是否必填，可以再升级成 Port 结构体。
int normalizeRotation(int degrees)
{
    int normalized = degrees % 360;
    if (normalized < 0) {
        normalized += 360;
    }

    const int step = ((normalized + 45) / 90) * 90;
    return step % 360;
}

QJsonArray stringListToJson(const QStringList& values)
{
    QJsonArray array;
    for (const auto& value : values) {
        array.append(value);
    }
    return array;
}

QStringList stringListFromJson(const QJsonArray& array)
{
    QStringList values;
    for (const auto& value : array) {
        values.append(value.toString());
    }
    return values;
}

} // namespace

QJsonObject NodePosition::toJson() const
{
    return {
        {"x", x},
        {"y", y},
    };
}

NodePosition NodePosition::fromJson(const QJsonObject& object)
{
    NodePosition position;
    position.x = object.value("x").toDouble();
    position.y = object.value("y").toDouble();
    return position;
}

QJsonObject NodeRuntime::toJson() const
{
    return {
        {"timeout_ms", timeoutMs},
        {"retry_count", retryCount},
        {"max_memory_mb", maxMemoryMb},
        {"concurrency_group", concurrencyGroup},
    };
}

NodeRuntime NodeRuntime::fromJson(const QJsonObject& object)
{
    NodeRuntime runtime;
    runtime.timeoutMs = object.value("timeout_ms").toInt(300000);
    runtime.retryCount = object.value("retry_count").toInt(0);
    runtime.maxMemoryMb = object.value("max_memory_mb").toInt(1024);
    runtime.concurrencyGroup = object.value("concurrency_group").toString("default");
    return runtime;
}

QJsonObject Node::toJson() const
{
    QJsonObject object;
    object.insert("node_id", nodeId);
    object.insert("template_id", templateId.isEmpty() ? QJsonValue() : QJsonValue(templateId));
    object.insert("type", type);
    object.insert("name", name);
    object.insert("description", description);
    object.insert("position", position.toJson());
    object.insert("rotation_degrees", normalizeRotation(rotationDegrees));
    object.insert("input_ports", stringListToJson(inputPorts));
    object.insert("output_ports", stringListToJson(outputPorts));
    object.insert("io_spec", ioSpec.toJson());
    object.insert("config", config);
    object.insert("runtime", runtime.toJson());
    return object;
}

Node Node::fromJson(const QJsonObject& object)
{
    Node node;
    node.nodeId = object.value("node_id").toString();
    node.templateId = object.value("template_id").toString();
    node.type = object.value("type").toString();
    node.name = object.value("name").toString();
    node.description = object.value("description").toString();
    node.position = NodePosition::fromJson(object.value("position").toObject());
    node.rotationDegrees = normalizeRotation(object.value("rotation_degrees").toInt(0));
    node.inputPorts = stringListFromJson(object.value("input_ports").toArray());
    node.outputPorts = stringListFromJson(object.value("output_ports").toArray());
    node.ioSpec = NodeIoSpec::fromJson(object.value("io_spec").toObject());
    node.config = object.value("config").toObject();
    node.runtime = NodeRuntime::fromJson(object.value("runtime").toObject());
    return node;
}

} // namespace vws::domain
