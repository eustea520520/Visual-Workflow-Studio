#include "application/generation/WorkflowGenerationPromptBuilder.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace vws::application {

namespace {

QString jsonObjectToText(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QString descriptorCatalogText(const QList<NodeTemplateDescriptor>& descriptors)
{
    QJsonArray array;
    for (const auto& descriptor : descriptors) {
        array.append(descriptor.toJson());
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

QJsonObject upstreamContractsForPrompt(const QJsonObject& contracts)
{
    return contracts;
}

} // namespace

QString WorkflowGenerationPromptBuilder::systemPrompt() const
{
    return skeletonSystemPrompt();
}

QString WorkflowGenerationPromptBuilder::buildUserPrompt(const QString& requirement) const
{
    return QStringLiteral("User requirement:\n%1").arg(requirement.trimmed());
}

QString WorkflowGenerationPromptBuilder::buildCopyablePrompt(const QString& requirementPlaceholder) const
{
    return QStringLiteral("%1\n\n%2").arg(skeletonSystemPrompt(), buildUserPrompt(requirementPlaceholder));
}

QString WorkflowGenerationPromptBuilder::skeletonSystemPrompt() const
{
    return QStringLiteral(
        "You are designing a workflow skeleton for Visual Workflow Studio.\n"
        "\n"
        "Return ONLY one JSON object. Return ONLY valid JSON. Do not use markdown. Do not include explanations.\n"
        "\n"
        "You are not writing node code yet.\n"
        "Your job is only to choose nodes, choose templates, define dependencies, and design a clear DAG layout.\n"
        "\n"
        "Supported node type literal set: starter|function|agent.\n"
        "\n"
        "Available node types:\n"
        "- starter: starts the workflow, has no input port, has output port \"output\".\n"
        "- function: runs Python code, has input port \"input\" and output port \"output\".\n"
        "- agent: runs a Python-backed agent template, has input port \"input\" and output port \"output\".\n"
        "\n"
        "Current template families:\n"
        "- Starter templates: starter_empty_output, starter_data_output, starter_file_output.\n"
        "- Function templates: data_to_data, data_to_file, file_to_data, file_to_file.\n"
        "- Agent templates: agent_data_to_data, agent_data_to_file, agent_file_to_data, agent_file_to_file.\n"
        "- Data templates pass small JSON-serializable business data through outputs.output.\n"
        "- File templates pass a JSON object containing file_path, format, and size_bytes through outputs.output, and also register artifacts.\n"
        "- Agent templates are Python nodes that call an OpenAI-compatible chat endpoint; they still follow the same data/file input-output contracts.\n"
        "\n"
        "Multi-dimensional IO model:\n"
        "- Every node has logical port \"input\" and/or \"output\". Dimension controls how many visible circles that logical port has.\n"
        "- expected_input_dimension / expected_output_dimension define the circle count and must match input_items / output_items label counts.\n"
        "- Use dimension=1 for ordinary whole-port transfer.\n"
        "- Use dimension>1 only when one node should split several independent values/files to separate downstream branches, or when a node must merge several separately labeled upstream values/files.\n"
        "- Slot indexes are 0-based. from_slot=0 sends outputs.output[0]. to_slot=0 is read as inputs[\"input\"][0].\n"
        "- A whole-port edge uses from_slot=-1 and to_slot=-1. A slot-level edge must set both indexes explicitly.\n"
        "\n"
        "Rules:\n"
        "1. Use only templates from the provided template catalog.\n"
        "2. Every node must reference one template_id from the catalog.\n"
        "3. Every workflow must contain at least one starter node.\n"
        "4. Starter nodes must not depend on other nodes.\n"
        "5. Every non-starter node must depend on at least one previous node.\n"
        "6. The graph must be a DAG. No cycles.\n"
        "7. Every edge must use from_port = \"output\" and to_port = \"input\".\n"
        "8. For slot-level connections, set from_slot and to_slot as 0-based indexes. Use -1 for whole-port connections.\n"
        "9. Do not mix whole-port and slot-level edges for the same target node input port.\n"
        "10. node_id and edge_id must be unique, lowercase, and contain only letters, numbers, underscore, or hyphen.\n"
        "11. Keep the workflow as small as practical.\n"
        "12. Prefer clear left-to-right layout. Use layer numbers from left to right and row numbers for branches.\n"
        "13. Describe node IO dimensions: expected_input_dimension is 0 for starter nodes and 1-12 for other nodes; expected_output_dimension is 1-12.\n"
        "14. If a node should pass multiple parallel items through one logical port, set the dimension and provide input_items/output_items labels with exactly that many labels.\n"
        "15. If you choose expected_output_dimension > 1, design edges that consume individual from_slot indexes where appropriate.\n"
        "16. If you choose expected_input_dimension > 1, design slot-level incoming edges to distinct to_slot indexes. Do not rely on runtime auto-detection.\n"
        "17. Do not write Python code.\n"
        "18. Do not output final workflow JSON yet.\n"
        "19. Output only the skeleton JSON.");
}

QString WorkflowGenerationPromptBuilder::buildSkeletonUserPrompt(
    const QString& requirement,
    const QList<NodeTemplateDescriptor>& descriptors,
    const QStringList& previousErrors) const
{
    QString prompt = QStringLiteral(
        "User requirement:\n%1\n\n"
        "Template catalog:\n%2\n\n"
        "Return this exact JSON shape:\n"
        "{\n"
        "  \"name\": \"workflow name\",\n"
        "  \"description\": \"workflow description\",\n"
        "  \"nodes\": [\n"
        "    {\n"
        "      \"node_id\": \"unique_node_id\",\n"
        "      \"template_id\": \"one_template_id_from_catalog\",\n"
        "      \"type\": \"starter|function|agent\",\n"
        "      \"name\": \"node name\",\n"
        "      \"purpose\": \"what this node should do\",\n"
        "      \"input_contract\": \"what input this node expects, or none\",\n"
        "      \"output_contract\": \"what output this node should produce\",\n"
        "      \"expected_input_dimension\": 1,\n"
        "      \"expected_output_dimension\": 1,\n"
        "      \"input_items\": [\"item 1 label\"],\n"
        "      \"output_items\": [\"item 1 label\"],\n"
        "      \"depends_on_node_ids\": [],\n"
        "      \"layer\": 0,\n"
        "      \"row\": 0\n"
        "    }\n"
        "  ],\n"
        "  \"edges\": [\n"
        "    {\n"
        "      \"edge_id\": \"unique_edge_id\",\n"
        "      \"from_node\": \"source_node_id\",\n"
        "      \"from_port\": \"output\",\n"
        "      \"from_slot\": -1,\n"
        "      \"to_node\": \"target_node_id\",\n"
        "      \"to_port\": \"input\",\n"
        "      \"to_slot\": -1\n"
        "    }\n"
        "  ]\n"
        "}")
        .arg(requirement.trimmed(), descriptorCatalogText(descriptors));

    if (!previousErrors.isEmpty()) {
        prompt.append(QStringLiteral(
            "\n\nYour previous skeleton was invalid.\n"
            "Validation errors:\n%1\n\n"
            "Return a corrected skeleton JSON only. Do not add explanations.")
            .arg(previousErrors.join("\n")));
    }

    return prompt;
}

QString WorkflowGenerationPromptBuilder::buildCopyableSkeletonPrompt(
    const QString& requirement,
    const QList<NodeTemplateDescriptor>& descriptors) const
{
    return QStringLiteral("%1\n\n%2")
        .arg(skeletonSystemPrompt(), buildSkeletonUserPrompt(requirement, descriptors));
}

QString WorkflowGenerationPromptBuilder::nodeImplementationSystemPrompt() const
{
    return QStringLiteral(
        "You are implementing exactly one Visual Workflow Studio node.\n"
        "\n"
        "Return ONLY valid JSON. Do not use markdown. Do not include explanations outside JSON.\n"
        "\n"
        "You must strictly follow the provided node template.\n"
        "You must not change node_id.\n"
        "You must not change template_id.\n"
        "You must not add edges or nodes.\n"
        "You must write only this node's implementation.\n"
        "\n"
        "Strict current template rules:\n"
        "- Use the provided Template full spec as the source of truth. Keep its imports, variable names, return shape, and comments unless a line must be filled with business logic.\n"
        "- The generated code must include the current VWS IO comments at the top. IO comments are the only source of visible port dimensions; runtime data never changes dimensions.\n"
        "- For starter nodes, include only # vws:output. Starter nodes have no input and must not read inputs.\n"
        "- For function and agent nodes, include # vws:input and # vws:output with dimensions and labels matching the skeleton.\n"
        "- Use exactly logical port names \"input\" and \"output\".\n"
        "- For dimension=1 input, input_data = inputs.get(\"input\", {}) is usually a dict.\n"
        "- For dimension>1 input, input_data = inputs.get(\"input\", []) is a list; read slot 0 as input_data[0], slot 1 as input_data[1], etc. Missing slots may be None/null.\n"
        "- For dimension>1 output, outputs[\"output\"] must be a list with exactly expected_output_dimension items.\n"
        "- For file input templates, support both shapes by using file_input = input_data[0] if isinstance(input_data, list) and input_data else input_data before file_input.get(\"file_path\").\n"
        "- For file output templates, keep the assignment line output_file_path = \"output.csv\" and write output_path = artifact_dir / output_file_path. Do not replace it with another variable name.\n"
        "- File outputs must return outputs.output.file_path, outputs.output.format, outputs.output.size_bytes and must add an artifacts entry for the same path.\n"
        "- Agent templates must keep these editable assignment lines: base_url, model_name, api_key, max_retries, background_prompt, task_prompt. Do not remove or rename them.\n"
        "- Agent config_patch may set agent_url, agent_model, agent_background_prompt, agent_task_prompt, agent_max_retries. agent_api_key must be empty or omitted.\n"
        "\n"
        "The Python code must define exactly:\n"
        "def run(inputs, context):\n"
        "\n"
        "The function must return:\n"
        "{\n"
        "  \"outputs\": {\n"
        "    \"output\": <json-serializable value>\n"
        "  },\n"
        "  \"artifacts\": []\n"
        "}\n"
        "\n"
        "Add vws IO comments at the top of the code, for example:\n"
        "# vws:input input dimension=2 labels=raw,config\n"
        "# vws:output output dimension=2 labels=summary,file\n"
        "For a single-output starter, use only:\n"
        "# vws:output output dimension=1 labels=1\n"
        "\n"
        "Do not include real API keys, passwords, or secrets.\n"
        "Do not use dangerous filesystem operations unless explicitly required by the user.\n"
        "Do not run shell commands.\n"
        "Do not import os/subprocess unless explicitly required and justified.");
}

QString WorkflowGenerationPromptBuilder::buildNodeImplementationUserPrompt(
    const QString& requirement,
    const WorkflowSkeleton& skeleton,
    const WorkflowSkeletonNode& node,
    const QJsonObject& upstreamContracts,
    const NodeTemplateFullSpec& fullSpec,
    const QStringList& previousErrors) const
{
    QString prompt = QStringLiteral(
        "Original user requirement:\n%1\n\n"
        "Workflow skeleton:\n%2\n\n"
        "Current node:\n%3\n\n"
        "Upstream output contracts:\n%4\n\n"
        "Required output contract:\n%5\n\n"
        "Template full spec:\n%6\n\n"
        "Return this exact JSON shape:\n"
        "{\n"
        "  \"node_id\": \"%7\",\n"
        "  \"code\": \"def run(inputs, context):\\n    ...\",\n"
        "  \"config_patch\": {\n"
        "    \"language\": \"python\",\n"
        "    \"entry\": \"run\"\n"
        "  },\n"
        "  \"io_spec_patch\": {\n"
        "    \"inputs\": [{\"port_name\": \"input\", \"dimension\": %8, \"source\": \"llm\", \"item_labels\": []}],\n"
        "    \"outputs\": [{\"port_name\": \"output\", \"dimension\": %9, \"source\": \"llm\", \"item_labels\": []}]\n"
        "  },\n"
        "  \"timeout_ms\": 300000,\n"
        "  \"notes\": \"short note\"\n"
        "}")
        .arg(requirement.trimmed(),
            jsonObjectToText(skeleton.toJson()),
            jsonObjectToText(node.toJson()),
            jsonObjectToText(upstreamContractsForPrompt(upstreamContracts)),
            node.outputContract,
            jsonObjectToText(fullSpec.toJson()),
            node.nodeId,
            QString::number(node.expectedInputDimension),
            QString::number(node.expectedOutputDimension));

    if (!previousErrors.isEmpty()) {
        prompt.append(QStringLiteral(
            "\n\nYour previous implementation for node %1 was invalid.\n"
            "Validation errors:\n%2\n\n"
            "Return corrected NodeImplementation JSON only.")
            .arg(node.nodeId, previousErrors.join("\n")));
    }

    return prompt;
}

QString WorkflowGenerationPromptBuilder::buildCopyableNodePrompt(
    const QString& requirement,
    const WorkflowSkeleton& skeleton,
    const WorkflowSkeletonNode& node,
    const QJsonObject& upstreamContracts,
    const NodeTemplateFullSpec& fullSpec) const
{
    return QStringLiteral("%1\n\n%2").arg(
        nodeImplementationSystemPrompt(),
        buildNodeImplementationUserPrompt(requirement, skeleton, node, upstreamContracts, fullSpec));
}

} // namespace vws::application
