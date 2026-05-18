#include "application/generation/WorkflowNodeImplementationValidator.h"

#include "application/io/NodeIoSpecValidator.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"

#include <QJsonDocument>

namespace vws::application {

namespace ConfigKeys = domain::NodeConfigKeys;
namespace NodeTypes = domain::NodeTypes;

namespace {
bool isFileOutputTemplate(const NodeTemplateFullSpec& spec)
{
    return spec.templateId == QStringLiteral("starter_file_output")
        || spec.ioKind == QStringLiteral("data_to_file")
        || spec.ioKind == QStringLiteral("file_to_file");
}

bool isFileInputTemplate(const NodeTemplateFullSpec& spec)
{
    return spec.ioKind == QStringLiteral("file_to_data")
        || spec.ioKind == QStringLiteral("file_to_file");
}
} // namespace

bool WorkflowNodeImplementationValidator::validateJsonText(
    const QString& jsonText,
    const WorkflowSkeletonNode& node,
    const NodeTemplateFullSpec& spec,
    NodeImplementation& implementation,
    QStringList& errors) const
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(extractJsonObjectText(jsonText).toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errors.append(QStringLiteral("Node implementation response was not valid JSON: %1").arg(parseError.errorString()));
        return false;
    }
    implementation = NodeImplementation::fromJson(document.object());
    return validate(implementation, node, spec, errors);
}

bool WorkflowNodeImplementationValidator::validate(
    const NodeImplementation& implementation,
    const WorkflowSkeletonNode& node,
    const NodeTemplateFullSpec& spec,
    QStringList& errors) const
{
    if (implementation.nodeId != node.nodeId) {
        errors.append(QStringLiteral("Implementation node_id %1 does not match current node %2.")
            .arg(implementation.nodeId, node.nodeId));
    }
    if (implementation.code.trimmed().isEmpty()) {
        errors.append(QStringLiteral("Implementation code is empty for node %1.").arg(node.nodeId));
    }
    if (!implementation.code.contains("def run(")) {
        errors.append(QStringLiteral("Node %1 code must define def run(inputs, context).").arg(node.nodeId));
    }
    if (implementation.code.contains("```")) {
        errors.append(QStringLiteral("Node %1 code must not contain markdown fences.").arg(node.nodeId));
    }
    if (implementation.timeoutMs <= 0) {
        errors.append(QStringLiteral("Node %1 timeout_ms must be greater than 0.").arg(node.nodeId));
    }
    NodeIoSpecValidator().validate(implementation.ioSpecPatch, errors);

    for (const auto& input : implementation.ioSpecPatch.inputs) {
        if (input.portName == "input" && input.dimension != node.expectedInputDimension) {
            errors.append(QStringLiteral("Node %1 io_spec_patch input dimension must match skeleton expected_input_dimension.")
                .arg(node.nodeId));
        }
    }
    for (const auto& output : implementation.ioSpecPatch.outputs) {
        if (output.portName == "output" && output.dimension != node.expectedOutputDimension) {
            errors.append(QStringLiteral("Node %1 io_spec_patch output dimension must match skeleton expected_output_dimension.")
                .arg(node.nodeId));
        }
    }
    if (node.expectedInputDimension > 0
        && !implementation.code.contains(QStringLiteral("vws:input input dimension=%1").arg(node.expectedInputDimension))) {
        errors.append(QStringLiteral("Node %1 code must include a # vws:input input dimension=%2 comment.")
            .arg(node.nodeId)
            .arg(node.expectedInputDimension));
    }
    if (!implementation.code.contains(QStringLiteral("vws:output output dimension=%1").arg(node.expectedOutputDimension))) {
        errors.append(QStringLiteral("Node %1 code must include a # vws:output output dimension=%2 comment.")
            .arg(node.nodeId)
            .arg(node.expectedOutputDimension));
    }
    if (node.expectedOutputDimension > 1 && !implementation.code.contains("outputs[\"output\"]")) {
        errors.append(QStringLiteral("Node %1 multi-output code should build outputs[\"output\"] as a list.")
            .arg(node.nodeId));
    }
    if (node.expectedInputDimension > 1 && !implementation.code.contains("input_data[0]")) {
        errors.append(QStringLiteral("Node %1 multi-input code should read slot values with input_data[0], input_data[1], etc.")
            .arg(node.nodeId));
    }
    if (isFileOutputTemplate(spec) && !implementation.code.contains("output_file_path")) {
        errors.append(QStringLiteral("Node %1 file-output template must keep output_file_path = ... for the editor field.")
            .arg(node.nodeId));
    }
    if (isFileInputTemplate(spec) && !implementation.code.contains("file_input = input_data[0]")) {
        errors.append(QStringLiteral("Node %1 file-input template must support multi-slot input through file_input = input_data[0] ...")
            .arg(node.nodeId));
    }
    if (spec.type == NodeTypes::Agent) {
        const QStringList agentAssignments = {
            QStringLiteral("base_url"),
            QStringLiteral("model_name"),
            QStringLiteral("api_key"),
            QStringLiteral("max_retries"),
            QStringLiteral("background_prompt"),
            QStringLiteral("task_prompt"),
        };
        for (const auto& assignment : agentAssignments) {
            if (!implementation.code.contains(QStringLiteral("%1 =").arg(assignment))) {
                errors.append(QStringLiteral("Agent node %1 must keep editable assignment line %2 = ...")
                    .arg(node.nodeId, assignment));
            }
        }
    }

    const auto language = implementation.configPatch.value(ConfigKeys::Language).toString();
    if (!language.isEmpty() && language != "python") {
        errors.append(QStringLiteral("Node %1 config_patch.language must be python.").arg(node.nodeId));
    }
    const auto entry = implementation.configPatch.value(ConfigKeys::Entry).toString();
    if (!entry.isEmpty() && entry != "run") {
        errors.append(QStringLiteral("Node %1 config_patch.entry must be run.").arg(node.nodeId));
    }
    if (!implementation.configPatch.value(ConfigKeys::AgentApiKey).toString().trimmed().isEmpty()) {
        errors.append(QStringLiteral("Node %1 must not contain a real agent_api_key.").arg(node.nodeId));
    }

    const auto codeLower = implementation.code.toLower();
    const QStringList forbidden = {
        QStringLiteral("subprocess"),
        QStringLiteral("os.system"),
        QStringLiteral("eval("),
        QStringLiteral("exec("),
        QStringLiteral("open(\"/etc"),
        QStringLiteral("open('/etc"),
        QStringLiteral("shutil.rmtree"),
        QStringLiteral("socket"),
    };
    for (const auto& token : forbidden) {
        if (codeLower.contains(token)) {
            errors.append(QStringLiteral("Node %1 code contains forbidden token: %2").arg(node.nodeId, token));
        }
    }

    const auto configIoTemplate = implementation.configPatch.value(ConfigKeys::IoTemplate).toString();
    if (!configIoTemplate.isEmpty() && configIoTemplate != spec.ioKind && configIoTemplate != spec.templateId) {
        errors.append(QStringLiteral("Node %1 must not change io_template away from template %2.")
            .arg(node.nodeId, spec.templateId));
    }

    return errors.isEmpty();
}

QString WorkflowNodeImplementationValidator::extractJsonObjectText(const QString& text) const
{
    const auto trimmed = text.trimmed();
    if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
        return trimmed;
    }
    const auto start = trimmed.indexOf('{');
    const auto end = trimmed.lastIndexOf('}');
    return start >= 0 && end > start ? trimmed.mid(start, end - start + 1) : QString();
}

} // namespace vws::application
