#include "application/generation/WorkflowGenerationTypes.h"

#include <QJsonArray>

namespace vws::application {

namespace {
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

QJsonObject NodeTemplateDescriptor::toJson() const
{
    return {
        {"template_id", templateId},
        {"type", type},
        {"display_name", displayName},
        {"description", description},
        {"io_kind", ioKind},
        {"input_ports", stringListToJson(inputPorts)},
        {"output_ports", stringListToJson(outputPorts)},
        {"usage_hint", usageHint},
    };
}

QJsonObject NodeTemplateFullSpec::toJson() const
{
    return {
        {"template_id", templateId},
        {"type", type},
        {"display_name", displayName},
        {"description", description},
        {"io_kind", ioKind},
        {"input_ports", stringListToJson(inputPorts)},
        {"output_ports", stringListToJson(outputPorts)},
        {"default_config", defaultConfig},
        {"default_io_spec", defaultIoSpec.toJson()},
        {"default_runtime", defaultRuntime.toJson()},
        {"code_template", codeTemplate},
        {"programming_instructions", programmingInstructions},
    };
}

QJsonObject WorkflowSkeletonNode::toJson() const
{
    return {
        {"node_id", nodeId},
        {"template_id", templateId},
        {"type", type},
        {"name", name},
        {"purpose", purpose},
        {"input_contract", inputContract},
        {"output_contract", outputContract},
        {"expected_input_dimension", expectedInputDimension},
        {"expected_output_dimension", expectedOutputDimension},
        {"loop_iterations", loopIterations},
        {"input_items", stringListToJson(inputItems)},
        {"output_items", stringListToJson(outputItems)},
        {"depends_on_node_ids", stringListToJson(dependsOnNodeIds)},
        {"layer", layer},
        {"row", row},
    };
}

WorkflowSkeletonNode WorkflowSkeletonNode::fromJson(const QJsonObject& object)
{
    WorkflowSkeletonNode node;
    node.nodeId = object.value("node_id").toString();
    node.templateId = object.value("template_id").toString();
    node.type = object.value("type").toString();
    node.name = object.value("name").toString();
    node.purpose = object.value("purpose").toString();
    node.inputContract = object.value("input_contract").toString();
    node.outputContract = object.value("output_contract").toString();
    node.expectedInputDimension = object.contains("expected_input_dimension")
        ? object.value("expected_input_dimension").toInt(1)
        : (node.type == QStringLiteral("starter") ? 0 : 1);
    node.expectedOutputDimension = object.value("expected_output_dimension").toInt(1);
    node.loopIterations = object.value("loop_iterations").toInt(0);
    node.inputItems = stringListFromJson(object.value("input_items").toArray());
    node.outputItems = stringListFromJson(object.value("output_items").toArray());
    node.dependsOnNodeIds = stringListFromJson(object.value("depends_on_node_ids").toArray());
    node.layer = object.value("layer").toInt(0);
    node.row = object.value("row").toInt(0);
    return node;
}

QJsonObject WorkflowSkeletonEdge::toJson() const
{
    return {
        {"edge_id", edgeId},
        {"from_node", fromNode},
        {"from_port", fromPort},
        {"from_slot", fromSlot},
        {"to_node", toNode},
        {"to_port", toPort},
        {"to_slot", toSlot},
    };
}

WorkflowSkeletonEdge WorkflowSkeletonEdge::fromJson(const QJsonObject& object)
{
    WorkflowSkeletonEdge edge;
    edge.edgeId = object.value("edge_id").toString();
    edge.fromNode = object.value("from_node").toString();
    edge.fromPort = object.value("from_port").toString();
    edge.fromSlot = object.contains("from_slot") ? object.value("from_slot").toInt(-1) : -1;
    edge.toNode = object.value("to_node").toString();
    edge.toPort = object.value("to_port").toString();
    edge.toSlot = object.contains("to_slot") ? object.value("to_slot").toInt(-1) : -1;
    return edge;
}

QJsonObject WorkflowSkeleton::toJson() const
{
    QJsonArray nodeArray;
    for (const auto& node : nodes) {
        nodeArray.append(node.toJson());
    }
    QJsonArray edgeArray;
    for (const auto& edge : edges) {
        edgeArray.append(edge.toJson());
    }
    return {
        {"name", name},
        {"description", description},
        {"nodes", nodeArray},
        {"edges", edgeArray},
    };
}

WorkflowSkeleton WorkflowSkeleton::fromJson(const QJsonObject& object)
{
    WorkflowSkeleton skeleton;
    skeleton.name = object.value("name").toString();
    skeleton.description = object.value("description").toString();
    for (const auto& value : object.value("nodes").toArray()) {
        skeleton.nodes.append(WorkflowSkeletonNode::fromJson(value.toObject()));
    }
    for (const auto& value : object.value("edges").toArray()) {
        skeleton.edges.append(WorkflowSkeletonEdge::fromJson(value.toObject()));
    }
    return skeleton;
}

QJsonObject NodeImplementation::toJson() const
{
    return {
        {"node_id", nodeId},
        {"code", code},
        {"config_patch", configPatch},
        {"io_spec_patch", ioSpecPatch.toJson()},
        {"timeout_ms", timeoutMs},
        {"notes", notes},
    };
}

NodeImplementation NodeImplementation::fromJson(const QJsonObject& object)
{
    NodeImplementation implementation;
    implementation.nodeId = object.value("node_id").toString();
    implementation.code = object.value("code").toString();
    implementation.configPatch = object.value("config_patch").toObject();
    implementation.ioSpecPatch = domain::NodeIoSpec::fromJson(object.value("io_spec_patch").toObject());
    implementation.timeoutMs = object.value("timeout_ms").toInt(300000);
    implementation.notes = object.value("notes").toString();
    return implementation;
}

} // namespace vws::application
