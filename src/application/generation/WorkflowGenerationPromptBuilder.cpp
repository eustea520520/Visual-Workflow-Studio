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

QString currentRuntimeModelText()
{
    return QStringLiteral(
        "Current VWS runtime data model, mandatory:\n"
        "- The workflow JSON schema_version must be 2. Older schema versions and missing schema_version are rejected.\n"
        "- The app uses slot-level connections only. Every edge must set from_slot and to_slot to 0 or greater.\n"
        "- Logical port names are stable: normal executable nodes use input port \"input\" and output port \"output\"; starter nodes have no input port.\n"
        "- A node's visible circles are IO slots. expected_input_dimension / expected_output_dimension and # vws comments define the circle count. Runtime data never auto-changes dimensions.\n"
        "- Python receives inputs[\"input\"] as a list of slot values. Slot 0 is input_data[0], slot 1 is input_data[1], etc.\n"
        "- Python must return outputs[\"output\"] as a list of slot values. Slot 0 is outputs[\"output\"][0], slot 1 is outputs[\"output\"][1], etc.\n"
        "- Even a one-output node must return a one-item list, for example {\"outputs\": {\"output\": [result]}, \"artifacts\": []}.\n"
        "- Do not return outputs[\"output\"] as a bare dict/string/number. Wrap it in a list.\n"
        "- Small business data should be JSON-serializable dict/list/string/number/bool/null values.\n"
        "- Large data should be passed by file descriptor objects such as {\"file_path\": path, \"format\": \"csv\", \"size_bytes\": n}; also register the same file in artifacts for UI display.\n"
        "- artifacts are for UI/run record visibility of generated files; artifacts do not replace business outputs.\n");
}

QString currentNodeTypeGuideText()
{
    return QStringLiteral(
        "Current node type guide:\n"
        "- starter: Python-backed source node. It has no input port and starts the workflow. It emits outputs[\"output\"] as a list. Use starter_empty_output, starter_data_output, or starter_file_output.\n"
        "- function: Python-backed business node. It reads inputs[\"input\"] list and returns outputs[\"output\"] list. Use data_to_data, data_to_file, file_to_data, or file_to_file.\n"
        "- agent: Python-backed LLM/API node. It follows the same input/output list contract as function, but keeps editable assignment lines base_url, model_name, api_key, max_retries, background_prompt, task_prompt.\n"
        "- subsystem: nested workflow node. It has no direct Python code. Use it to hide a multi-step workflow behind one node, especially as the single body node after a Loop.\n"
        "- loop: Python-backed control/data node. It receives its upstream input once, runs its Python code once per iteration, and feeds exactly one direct body node per iteration. Its direct body may be function, agent, or subsystem. Use subsystem as the body when the loop body needs multiple internal nodes.\n"
        "- Loop does not create graph cycles in JSON. The outer graph remains a DAG.\n"
        "- Loop iteration values live in context[\"loop\"] only: iter, index, iteration_count, previous_loop_output, previous_body_output, history. Do not put loop-control metadata into outputs[\"output\"] unless the user's business data explicitly needs it.\n"
        "- After all loop iterations finish, downstream nodes after the body receive only the last body output; full loop history is stored in metadata/debug, not business output.\n");
}

QString currentTemplateFamilyGuideText()
{
    return QStringLiteral(
        "Current template families:\n"
        "- Starter templates:\n"
        "  - starter_empty_output: trigger-only starter; outputs [{}].\n"
        "  - starter_data_output: creates small JSON business data; outputs [data].\n"
        "  - starter_file_output: creates a CSV/file artifact; outputs [{file_path, format, size_bytes}] and artifacts.\n"
        "- Function templates:\n"
        "  - data_to_data: reads JSON slot data and returns JSON slot data.\n"
        "  - data_to_file: reads JSON slot data and writes a CSV/file artifact.\n"
        "  - file_to_data: reads file descriptor slot data, opens the file, and returns extracted JSON.\n"
        "  - file_to_file: reads file descriptor slot data, writes another CSV/file artifact.\n"
        "- Agent templates:\n"
        "  - agent_data_to_data, agent_data_to_file, agent_file_to_data, agent_file_to_file. Same data/file contracts as Function, but call an OpenAI-compatible chat endpoint.\n"
        "- Subsystem template:\n"
        "  - subsystem_basic: no direct code; internal workflow defines actual behavior and boundary ports.\n"
        "- Loop template:\n"
        "  - loop: fixed iteration count via loop_iterations. It must connect to exactly one direct body node. For multi-node loop bodies, generate a subsystem node as that one body.\n");
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
        "Supported node type literal set: starter|function|agent|subsystem|loop.\n"
        "\n%1\n"
        "\n%2\n"
        "\n%3\n"
        "Multi-dimensional IO planning:\n"
        "- expected_input_dimension / expected_output_dimension define the circle count and must match input_items / output_items label counts exactly.\n"
        "- Use dimension=1 for ordinary one-slot transfer.\n"
        "- Use dimension>1 only when one node should split several independent slot values/files to downstream branches, or when a node must merge several separately labeled upstream slot values/files.\n"
        "- Labels should describe slot meaning, for example raw_rows, config, summary, report_file.\n"
        "\n"
        "Rules:\n"
        "1. Use only templates from the provided template catalog.\n"
        "2. Every node must reference one template_id from the catalog.\n"
        "3. Every workflow must contain at least one starter node, or one subsystem node with expected_input_dimension=0 that acts as a composite source.\n"
        "4. Starter nodes and zero-input subsystem source nodes must not depend on other nodes.\n"
        "5. Every non-starter node except zero-input subsystem source nodes must depend on at least one previous node.\n"
        "6. The graph must be a DAG. No cycles.\n"
        "7. All ordinary edges, including loop-to-body and body-to-downstream edges, use from_port = \"output\" and to_port = \"input\".\n"
        "8. Set from_slot and to_slot as 0-based indexes on every edge. Never use -1.\n"
        "9. Do not write more than one edge into the same target node input slot.\n"
        "10. node_id and edge_id must be unique, lowercase, and contain only letters, numbers, underscore, or hyphen.\n"
        "11. Keep the workflow as small as practical.\n"
        "12. Prefer clear left-to-right layout. Use layer numbers from left to right and row numbers for branches.\n"
        "13. Describe node IO dimensions: expected_input_dimension is 0 for starter nodes and 1-12 for other nodes; expected_output_dimension is 1-12 for non-subsystem executable nodes.\n"
        "14. If a node should pass multiple parallel items through one logical port, set the dimension and provide input_items/output_items labels with exactly that many labels.\n"
        "15. If you choose expected_output_dimension > 1, design edges that consume individual from_slot indexes where appropriate.\n"
        "16. If you choose expected_input_dimension > 1, design slot-level incoming edges to distinct to_slot indexes. Do not rely on runtime auto-detection.\n"
        "17. Do not write Python code.\n"
        "18. Subsystem nodes may have expected_input_dimension=0 and expected_output_dimension=1 with empty input_items/output_items when acting as a source; do not create code for them.\n"
        "19. Loop rules: every loop node must have exactly one direct downstream body node; the body node must not receive input from any other node; nested loop body nodes are not supported.\n"
        "20. For loop set loop_iterations to a positive integer. The loop node code runs once per iteration and feeds the body node. Use subsystem as the body for multi-node loops.\n"
        "21. Do not output final workflow JSON yet.\n"
        "22. Output only the skeleton JSON.")
        .arg(currentRuntimeModelText(), currentNodeTypeGuideText(), currentTemplateFamilyGuideText());
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
        "      \"type\": \"starter|function|agent|subsystem|loop\",\n"
        "      \"name\": \"node name\",\n"
        "      \"purpose\": \"what this node should do\",\n"
        "      \"input_contract\": \"what input this node expects, or none\",\n"
        "      \"output_contract\": \"what output this node should produce\",\n"
        "      \"expected_input_dimension\": 1,\n"
        "      \"expected_output_dimension\": 1,\n"
        "      \"loop_iterations\": 0,\n"
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
        "      \"from_slot\": 0,\n"
        "      \"to_node\": \"target_node_id\",\n"
        "      \"to_port\": \"input\",\n"
        "      \"to_slot\": 0\n"
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
        "%1\n"
        "\n%2\n"
        "\n%3\n"
        "\n"
        "Strict current template rules:\n"
        "- Use the provided Template full spec and code_template as the source of truth. Keep its imports, variable names, return shape, and comments unless a line must be filled with business logic.\n"
        "- The generated code must include the current VWS IO comments at the top. IO comments are the only source of visible port dimensions; runtime data never changes dimensions.\n"
        "- For starter nodes, include only # vws:output. Starter nodes have no input and must not read inputs.\n"
        "- For function and agent nodes, include # vws:input and # vws:output with dimensions and labels matching the skeleton.\n"
        "- For loop nodes, include # vws:input input and # vws:output output. Read iteration data from loop = context.get(\"loop\", {}), including iter, index, iteration_count, previous_loop_output, previous_body_output, and history. Do not wrap loop control fields into outputs[\"output\"] unless the user explicitly asks for them as business data.\n"
        "- Loop node must keep only business data in outputs[\"output\"]; loop iteration metadata belongs to context[\"loop\"] and result metadata/debug.\n"
        "- Use exactly logical port names \"input\" and \"output\".\n"
        "- input_data = inputs.get(\"input\", []) is always a list; read slot 0 as input_data[0], slot 1 as input_data[1], etc. Missing slots may be None/null.\n"
        "- outputs[\"output\"] must always be a list with exactly expected_output_dimension items, even when expected_output_dimension=1.\n"
        "- If expected_output_dimension is 1, return \"output\": [result]. If expected_output_dimension is 3, return \"output\": [slot0, slot1, slot2].\n"
        "- For file input templates, read the file slot with file_input = input_data[0] if input_data else {} before file_input.get(\"file_path\").\n"
        "- For file output templates, keep the assignment line output_file_path = \"output.csv\" and write output_path = artifact_dir / output_file_path. Do not replace it with another variable name.\n"
        "- File outputs must return outputs.output.file_path, outputs.output.format, outputs.output.size_bytes and must add an artifacts entry for the same path.\n"
        "- CSV is the default file format unless the user explicitly requests another format.\n"
        "- Agent templates must keep these editable assignment lines: base_url, model_name, api_key, max_retries, background_prompt, task_prompt. Do not remove or rename them.\n"
        "- Agent config_patch may set agent_url, agent_model, agent_background_prompt, agent_task_prompt, agent_max_retries. agent_api_key must be empty or omitted.\n"
        "- Subsystem nodes have no code; return no node implementation for subsystem nodes unless the orchestration layer explicitly asks for one.\n"
        "\n"
        "The Python code must define exactly:\n"
        "def run(inputs, context):\n"
        "\n"
        "The function must return:\n"
        "{\n"
        "  \"outputs\": {\n"
        "    \"output\": [<slot 0 value>, <slot 1 value>, ...]\n"
        "  },\n"
        "  \"artifacts\": []\n"
        "}\n"
        "For loop, keep outputs.output for business data only; iteration details come from context.loop.\n"
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
        "Do not import os/subprocess unless explicitly required and justified.")
        .arg(currentRuntimeModelText(), currentNodeTypeGuideText(), currentTemplateFamilyGuideText());
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
        "    \"outputs\": [{\"port_name\": \"%9\", \"dimension\": %10, \"source\": \"llm\", \"item_labels\": []}]\n"
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
            QStringLiteral("output"),
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
