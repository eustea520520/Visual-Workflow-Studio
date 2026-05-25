#include "domain/Workflow.h"

#include <QJsonArray>

namespace vws::domain {

QJsonObject Workflow::toJson() const
{
    // Workflow 是聚合根：保存工作流时，需要把内部 nodes 和 edges 一起序列化。
    QJsonArray nodeArray;
    for (const auto& node : nodes) {
        nodeArray.append(node.toJson());
    }

    QJsonArray edgeArray;
    for (const auto& edge : edges) {
        edgeArray.append(edge.toJson());
    }

    return {
        {"schema_version", schemaVersion},
        {"workflow_id", workflowId},
        {"workspace_id", workspaceId},
        {"name", name},
        {"description", description},
        {"nodes", nodeArray},
        {"edges", edgeArray},
        {"created_at", createdAt},
        {"updated_at", updatedAt},
        {"version", version},
    };
}

Workflow Workflow::fromJson(const QJsonObject& object)
{
    // 反序列化只做结构转换，不做合法性判断。
    // 图是否合法由 execution::GraphValidator 负责。
    Workflow workflow;
    workflow.schemaVersion = object.value("schema_version").toInt(CurrentWorkflowSchemaVersion);
    workflow.workflowId = object.value("workflow_id").toString();
    workflow.workspaceId = object.value("workspace_id").toString();
    workflow.name = object.value("name").toString();
    workflow.description = object.value("description").toString();
    workflow.createdAt = object.value("created_at").toString();
    workflow.updatedAt = object.value("updated_at").toString();
    workflow.version = object.value("version").toInt(1);

    const auto nodes = object.value("nodes").toArray();
    for (const auto& nodeValue : nodes) {
        workflow.nodes.append(Node::fromJson(nodeValue.toObject()));
    }

    const auto edges = object.value("edges").toArray();
    for (const auto& edgeValue : edges) {
        workflow.edges.append(Edge::fromJson(edgeValue.toObject()));
    }

    return workflow;
}

} // namespace vws::domain
