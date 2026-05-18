#include "application/generation/WorkflowGenerationTemplateCatalog.h"

#include "application/PythonCodeTemplates.h"
#include "application/io/NodeIoSpecUtils.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"

namespace vws::application {

namespace ConfigKeys = domain::NodeConfigKeys;
namespace NodeTypes = domain::NodeTypes;

namespace {

domain::NodeRuntime defaultRuntime()
{
    domain::NodeRuntime runtime;
    runtime.timeoutMs = 300000;
    runtime.retryCount = 0;
    runtime.maxMemoryMb = 1024;
    runtime.concurrencyGroup = QStringLiteral("default");
    return runtime;
}

QJsonObject pythonConfig(const QString& ioTemplate, const QString& code)
{
    return {
        {ConfigKeys::Language, QStringLiteral("python")},
        {ConfigKeys::Entry, QStringLiteral("run")},
        {ConfigKeys::IoTemplate, ioTemplate},
        {ConfigKeys::Code, code},
    };
}

QJsonObject agentConfig(const QString& ioTemplate, const QString& code)
{
    auto config = pythonConfig(ioTemplate, code);
    config.insert(ConfigKeys::AgentUrl, QString());
    config.insert(ConfigKeys::AgentModel, QString());
    config.insert(ConfigKeys::AgentApiKey, QString());
    config.insert(ConfigKeys::AgentMaxRetries, PythonCodeTemplates::defaultAgentMaxRetries());
    config.insert(ConfigKeys::AgentBackgroundPrompt, PythonCodeTemplates::defaultAgentBackgroundPrompt());
    config.insert(ConfigKeys::AgentTaskPrompt, PythonCodeTemplates::defaultAgentTaskPrompt());
    return config;
}

QString baseInstructions()
{
    return QStringLiteral(
        "Strictly follow the current code_template. "
        "The generated Python code must define exactly def run(inputs, context):. "
        "Keep # vws:input/# vws:output comments and set their dimensions/labels to match the skeleton. "
        "If dimension=1, inputs.get(\"input\", {}) is usually a dict. "
        "If dimension>1, inputs.get(\"input\", []) is a list and code must read input_data[0], input_data[1], etc. "
        "If output dimension>1, outputs.output must be a list with exactly that many items. "
        "Return {\"outputs\": {\"output\": <json-serializable value>}, \"artifacts\": []}. "
        "Do not wrap code in markdown fences. Do not include secrets. "
        "Do not use subprocess, os.system, eval, exec, socket, or destructive file deletion.");
}

QString fileOutputInstructions()
{
    return QStringLiteral(
        "For file output, write under context.get(\"artifact_path\") or context.get(\"run_path\"). "
        "Keep the editable assignment line output_file_path = \"output.csv\" and derive output_path from it. "
        "Return outputs.output.file_path, outputs.output.format, and register the file in artifacts. "
        "Prefer CSV for tabular outputs unless the user explicitly asks for another type.");
}

QString fileInputInstructions()
{
    return QStringLiteral(
        "For file input, support both single-input dict and multi-input list shapes. "
        "Use file_input = input_data[0] if isinstance(input_data, list) and input_data else input_data, "
        "then read file_input.get(\"file_path\", \"\").");
}

QString agentInstructions()
{
    return QStringLiteral(
        "Agent code is still Python code. Keep editable assignment lines base_url, model_name, api_key, "
        "max_retries, background_prompt, and task_prompt because UI fields update those lines by regex. "
        "config_patch may include agent_background_prompt, agent_task_prompt, agent_model, agent_url, agent_max_retries. "
        "agent_api_key must be empty or absent.");
}

NodeTemplateFullSpec makeSpec(
    const QString& templateId,
    const QString& type,
    const QString& displayName,
    const QString& description,
    const QString& ioKind,
    QStringList inputPorts,
    QStringList outputPorts,
    const QJsonObject& config,
    const QString& code,
    const QString& instructions)
{
    NodeTemplateFullSpec spec;
    spec.templateId = templateId;
    spec.type = type;
    spec.displayName = displayName;
    spec.description = description;
    spec.ioKind = ioKind;
    spec.inputPorts = std::move(inputPorts);
    spec.outputPorts = std::move(outputPorts);
    spec.defaultConfig = config;
    domain::Node prototype;
    prototype.inputPorts = spec.inputPorts;
    prototype.outputPorts = spec.outputPorts;
    spec.defaultIoSpec = NodeIoSpecUtils::defaultSpecForNode(prototype);
    spec.defaultRuntime = defaultRuntime();
    spec.codeTemplate = code;
    spec.programmingInstructions = instructions;
    return spec;
}

} // namespace

QList<NodeTemplateFullSpec> WorkflowGenerationTemplateCatalog::fullSpecs() const
{
    const auto emptyCode = PythonCodeTemplates::starterEmptyOutputCode();
    const auto starterDataCode = PythonCodeTemplates::starterDataOutputCode();
    const auto starterFileCode = PythonCodeTemplates::starterFileOutputCode();
    const auto dataToDataCode = PythonCodeTemplates::functionDataToDataCode();
    const auto dataToFileCode = PythonCodeTemplates::functionDataToFileCode();
    const auto fileToDataCode = PythonCodeTemplates::functionFileToDataCode();
    const auto fileToFileCode = PythonCodeTemplates::functionFileToFileCode();

    return {
        makeSpec("starter_empty_output", NodeTypes::Starter, "Starter Empty Output",
            "Start the workflow and emit an empty output object.", "starter_empty_output",
            {}, {"output"}, pythonConfig("starter_empty_output", emptyCode), emptyCode,
            baseInstructions() + " Starter nodes must ignore upstream inputs."),
        makeSpec("starter_data_output", NodeTypes::Starter, "Starter Data Output",
            "Create initial small JSON business data.", "starter_data_output",
            {}, {"output"}, pythonConfig("starter_data_output", starterDataCode), starterDataCode,
            baseInstructions() + " Starter nodes must create outputs.output from scratch."),
        makeSpec("starter_file_output", NodeTypes::Starter, "Starter File Output",
            "Create an initial file artifact and pass its path downstream.", "starter_file_output",
            {}, {"output"}, pythonConfig("starter_file_output", starterFileCode), starterFileCode,
            baseInstructions() + " " + fileOutputInstructions()),

        makeSpec("data_to_data", NodeTypes::Function, "Function Data to Data",
            "Transform structured input data into structured output data.", "data_to_data",
            {"input"}, {"output"}, pythonConfig("data_to_data", dataToDataCode), dataToDataCode,
            baseInstructions() + " Read upstream data from inputs.get(\"input\", {})."),
        makeSpec("data_to_file", NodeTypes::Function, "Function Data to File",
            "Transform structured input data into a file artifact.", "data_to_file",
            {"input"}, {"output"}, pythonConfig("data_to_file", dataToFileCode), dataToFileCode,
            baseInstructions() + " Read upstream data from inputs.get(\"input\", {}). " + fileOutputInstructions()),
        makeSpec("file_to_data", NodeTypes::Function, "Function File to Data",
            "Read an upstream file path and return extracted structured data.", "file_to_data",
            {"input"}, {"output"}, pythonConfig("file_to_data", fileToDataCode), fileToDataCode,
            baseInstructions() + " " + fileInputInstructions()),
        makeSpec("file_to_file", NodeTypes::Function, "Function File to File",
            "Read an upstream file and produce another file artifact.", "file_to_file",
            {"input"}, {"output"}, pythonConfig("file_to_file", fileToFileCode), fileToFileCode,
            baseInstructions() + " " + fileInputInstructions() + " " + fileOutputInstructions()),

        makeSpec("agent_data_to_data", NodeTypes::Agent, "Agent Data to Data",
            "Use an LLM agent to transform structured input data into structured output data.", "data_to_data",
            {"input"}, {"output"}, agentConfig("data_to_data", PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::DataToData)),
            PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::DataToData),
            baseInstructions() + " " + agentInstructions()),
        makeSpec("agent_data_to_file", NodeTypes::Agent, "Agent Data to File",
            "Use an LLM agent to produce a file artifact from structured data.", "data_to_file",
            {"input"}, {"output"}, agentConfig("data_to_file", PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::DataToFile)),
            PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::DataToFile),
            baseInstructions() + " " + agentInstructions() + " " + fileOutputInstructions()),
        makeSpec("agent_file_to_data", NodeTypes::Agent, "Agent File to Data",
            "Use an LLM agent to read a file and return structured data.", "file_to_data",
            {"input"}, {"output"}, agentConfig("file_to_data", PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::FileToData)),
            PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::FileToData),
            baseInstructions() + " " + agentInstructions() + " " + fileInputInstructions()),
        makeSpec("agent_file_to_file", NodeTypes::Agent, "Agent File to File",
            "Use an LLM agent to transform one file artifact into another.", "file_to_file",
            {"input"}, {"output"}, agentConfig("file_to_file", PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::FileToFile)),
            PythonCodeTemplates::agentCode({}, {}, {}, PythonCodeTemplates::defaultAgentMaxRetries(), PythonCodeTemplates::defaultAgentBackgroundPrompt(), PythonCodeTemplates::defaultAgentTaskPrompt(), DataTransferTemplate::FileToFile),
            baseInstructions() + " " + agentInstructions() + " " + fileInputInstructions() + " " + fileOutputInstructions()),
    };
}

QList<NodeTemplateDescriptor> WorkflowGenerationTemplateCatalog::descriptors() const
{
    QList<NodeTemplateDescriptor> result;
    for (const auto& spec : fullSpecs()) {
        result.append({
            spec.templateId,
            spec.type,
            spec.displayName,
            spec.description,
            spec.ioKind,
            spec.inputPorts,
            spec.outputPorts,
            spec.programmingInstructions,
        });
    }
    return result;
}

std::optional<NodeTemplateFullSpec> WorkflowGenerationTemplateCatalog::fullSpec(const QString& templateId) const
{
    for (const auto& spec : fullSpecs()) {
        if (spec.templateId == templateId) {
            return spec;
        }
    }
    return std::nullopt;
}

bool WorkflowGenerationTemplateCatalog::contains(const QString& templateId) const
{
    return fullSpec(templateId).has_value();
}

} // namespace vws::application
