#pragma once

#include "domain/NodeIoSpec.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace vws::domain {

// 节点在画布上的位置。UI 会用它恢复节点布局。
struct NodePosition {
    double x = 0.0;
    double y = 0.0;

    QJsonObject toJson() const;
    static NodePosition fromJson(const QJsonObject& object);
};

// 节点运行参数。执行层会读取这些值做超时、重试、资源分组等控制。
struct NodeRuntime {
    int timeoutMs = 300000;
    int retryCount = 0;
    int maxMemoryMb = 1024;
    QString concurrencyGroup = "default";

    QJsonObject toJson() const;
    static NodeRuntime fromJson(const QJsonObject& object);
};

// Node 是画布上的一个具体工作步骤。
// 例如一个 Python Function Node 或一个 Agent Node。
// 它是“某个工作流里的实例”，不是可复用模板。
struct Node {
    QString nodeId;
    QString templateId;
    QString type;
    QString name;
    QString description;
    NodePosition position;
    int rotationDegrees = 0;
    QStringList inputPorts;
    QStringList outputPorts;
    NodeIoSpec ioSpec;
    // config 保存不同节点类型自己的配置。
    // starter、function、agent 当前都放 Python 代码，并通过 PythonNodeWorker 执行。
    QJsonObject config;
    NodeRuntime runtime;

    QJsonObject toJson() const;
    static Node fromJson(const QJsonObject& object);
};

} // namespace vws::domain
