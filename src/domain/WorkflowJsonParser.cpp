#include "domain/WorkflowJsonParser.h"

#include "domain/WorkflowSchema.h"

#include <QJsonArray>
#include <QJsonValue>

#include <cmath>
#include <optional>

namespace vws::domain {

namespace {

QString unreadableMessage()
{
    return QStringLiteral("The current workspace is not in a readable format.");
}

bool isIntegerValue(const QJsonValue& value)
{
    if (!value.isDouble()) {
        return false;
    }
    const auto number = value.toDouble();
    return std::floor(number) == number;
}

int requiredInteger(
    const QJsonObject& object,
    const QString& key,
    const QString& label,
    QStringList& errors,
    int minimum = 0)
{
    Q_UNUSED(label);
    if (!object.contains(key)) {
        errors.append(unreadableMessage());
        return minimum - 1;
    }
    const auto value = object.value(key);
    if (!isIntegerValue(value)) {
        errors.append(unreadableMessage());
        return minimum - 1;
    }
    const int parsed = value.toInt(minimum - 1);
    if (parsed < minimum) {
        errors.append(unreadableMessage());
    }
    return parsed;
}

std::optional<Edge> parseEdgeStrict(const QJsonObject& object, int index, QStringList& errors)
{
    Edge edge;
    edge.edgeId = object.value(QStringLiteral("edge_id")).toString();
    edge.fromNode = object.value(QStringLiteral("from_node")).toString();
    edge.fromPort = object.value(QStringLiteral("from_port")).toString();
    edge.toNode = object.value(QStringLiteral("to_node")).toString();
    edge.toPort = object.value(QStringLiteral("to_port")).toString();

    const auto edgeLabel = edge.edgeId.trimmed().isEmpty()
        ? QStringLiteral("Edge at index %1").arg(index)
        : QStringLiteral("Edge %1").arg(edge.edgeId);

    const auto errorCountBeforeSlots = errors.size();
    edge.fromSlot = requiredInteger(object, QStringLiteral("from_slot"), edgeLabel, errors);
    edge.toSlot = requiredInteger(object, QStringLiteral("to_slot"), edgeLabel, errors);
    if (errors.size() != errorCountBeforeSlots) {
        return std::nullopt;
    }

    return edge;
}

} // namespace

QString WorkflowJsonParser::unreadableWorkspaceMessage()
{
    return unreadableMessage();
}

WorkflowParseResult WorkflowJsonParser::parseStrict(const QJsonObject& object)
{
    WorkflowParseResult result;

    if (!object.contains(QStringLiteral("schema_version"))) {
        result.errors.append(unreadableMessage());
        return result;
    }

    const auto schemaVersion = requiredInteger(
        object,
        QStringLiteral("schema_version"),
        QStringLiteral("Workflow"),
        result.errors,
        1);
    if (!result.errors.isEmpty()) {
        result.errors = {unreadableMessage()};
        return result;
    }
    if (schemaVersion < CurrentWorkflowSchemaVersion) {
        result.errors.append(unreadableMessage());
        return result;
    }
    if (schemaVersion > CurrentWorkflowSchemaVersion) {
        result.errors.append(unreadableMessage());
        return result;
    }

    const auto errorCountBeforeShapeCheck = result.errors.size();
    if (!object.value(QStringLiteral("nodes")).isArray()
        || !object.value(QStringLiteral("edges")).isArray()) {
        result.errors.append(unreadableMessage());
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    Workflow workflow = Workflow::fromJson(object);
    workflow.edges.clear();
    const auto edges = object.value(QStringLiteral("edges")).toArray();
    for (int index = 0; index < edges.size(); ++index) {
        if (!edges.at(index).isObject()) {
            result.errors.append(QStringLiteral("Edge at index %1 must be an object.").arg(index));
            continue;
        }
        const auto parsedEdge = parseEdgeStrict(edges.at(index).toObject(), index, result.errors);
        if (parsedEdge.has_value()) {
            workflow.edges.append(parsedEdge.value());
        }
    }

    if (result.errors.size() != errorCountBeforeShapeCheck) {
        result.errors = {unreadableMessage()};
        return result;
    }

    if (!result.errors.isEmpty()) {
        result.errors = {unreadableMessage()};
        return result;
    }

    result.workflow = workflow;
    result.success = true;
    return result;
}

} // namespace vws::domain
