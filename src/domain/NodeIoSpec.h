#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace vws::domain {

struct PortDimensionSpec {
    QString portName;
    int dimension = 1;
    QString source = QStringLiteral("default");
    QString description;
    QStringList itemLabels;

    QJsonObject toJson() const;
    static PortDimensionSpec fromJson(const QJsonObject& object);
};

struct NodeIoSpec {
    QList<PortDimensionSpec> inputs;
    QList<PortDimensionSpec> outputs;

    bool isEmpty() const;
    QJsonObject toJson() const;
    static NodeIoSpec fromJson(const QJsonObject& object);
};

} // namespace vws::domain
