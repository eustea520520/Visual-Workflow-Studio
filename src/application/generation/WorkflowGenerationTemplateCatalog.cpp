#include "application/generation/WorkflowGenerationTemplateCatalog.h"

#include "application/PythonCodeTemplates.h"
#include "application/io/NodeIoSpecUtils.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"

#include <QJsonArray>

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

QJsonObject subsystemConfig()
{
    return {
        {ConfigKeys::SubsystemSchemaVersion, 1},
        {ConfigKeys::SubsystemBoundary, QJsonObject{{"inputs", QJsonArray{}}, {"outputs", QJsonArray{}}}},
    };
}

QJsonObject loopConfig(const QString& code)
{
    auto config = pythonConfig(QStringLiteral("loop"), code);
    config.insert(ConfigKeys::LoopIterations, 1);
    return config;
}

QString baseInstructions()
{
    return QStringLiteral(
        "Strictly follow the current code_template and current VWS slot IO contract. "
        "The generated Python code must define exactly def run(inputs, context):. "
        "Keep # vws:input/# vws:output comments and set their dimensions/labels to match the skeleton. "
        "inputs.get(\"input\", []) is always a slot list and code must read input_data[0], input_data[1], etc. "
        "outputs[\"output\"] must always be a slot list with exactly the output dimension count, even for dimension=1. "
        "Return {\"outputs\": {\"output\": [<slot values>]}, \"artifacts\": []}. "
        "Never return a bare dict as outputs[\"output\"]. "
        "Do not wrap code in markdown fences. Do not include secrets. "
        "Do not use subprocess, os.system, eval, exec, socket, or destructive file deletion.");
}

QString loopInstructions()
{
    return QStringLiteral(
        "Loop is a Python-backed node that runs once per iteration and feeds exactly one direct body node. "
        "Its upstream inputs[\"input\"] slot list is captured once and remains unchanged for every iteration. "
        "Read iteration data from context.get(\"loop\", {}), never from inputs. "
        "The Loop node must keep only business data in outputs[\"output\"] as a list; loop metadata stays in context/metadata. "
        "A Loop node must have exactly one direct downstream body node. "
        "Use a Subsystem as the direct body node when the loop body has multiple steps.");
}

QString fileOutputInstructions()
{
    return QStringLiteral(
        "For file output, write under context.get(\"artifact_path\") or context.get(\"run_path\"). "
        "Keep the editable assignment line output_file_path = \"output.csv\" and derive output_path from it. "
        "Return outputs[\"output\"] as a list containing a file descriptor object with file_path, format, and size_bytes; register the file in artifacts too. "
        "Prefer CSV for tabular outputs unless the user explicitly asks for another type.");
}

QString fileInputInstructions()
{
    return QStringLiteral(
        "For file input, use the slot list shape. "
        "Use file_input = input_data[0] if input_data else {}, "
        "then read file_input.get(\"file_path\", \"\"). If multiple file input slots are requested, read input_data[1], input_data[2], etc. explicitly.");
}

QString agentInstructions()
{
    return QStringLiteral(
        "Agent code is still Python code. Keep editable assignment lines base_url, model_name, api_key, "
        "max_retries, background_prompt, and task_prompt because UI fields update those lines by regex. "
        "Agent input/output data still uses inputs[\"input\"] and outputs[\"output\"] lists exactly like Function nodes. "
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
    const auto loopCode = PythonCodeTemplates::loopCode();

    return {
        makeSpec("starter_empty_output", NodeTypes::Starter, "Starter Empty Output",
            "Start the workflow without upstream input and emit one empty business object as outputs.output[0].", "starter_empty_output",
            {}, {"output"}, pythonConfig("starter_empty_output", emptyCode), emptyCode,
            baseInstructions() + " Starter nodes must ignore upstream inputs and must not create # vws:input comments."),
        makeSpec("starter_data_output", NodeTypes::Starter, "Starter Data Output",
            "Start the workflow and create initial small JSON business data as outputs.output slot values.", "starter_data_output",
            {}, {"output"}, pythonConfig("starter_data_output", starterDataCode), starterDataCode,
            baseInstructions() + " Starter nodes must create outputs[\"output\"] from scratch and must not read inputs."),
        makeSpec("starter_file_output", NodeTypes::Starter, "Starter File Output",
            "Start the workflow, create a CSV/file artifact, and pass its file descriptor downstream as outputs.output[0].", "starter_file_output",
            {}, {"output"}, pythonConfig("starter_file_output", starterFileCode), starterFileCode,
            baseInstructions() + " Starter nodes must not read inputs. " + fileOutputInstructions()),

        makeSpec("data_to_data", NodeTypes::Function, "Function Data to Data",
            "Read JSON-compatible slot data from inputs.input and return JSON-compatible slot data.", "data_to_data",
            {"input"}, {"output"}, pythonConfig("data_to_data", dataToDataCode), dataToDataCode,
            baseInstructions() + " Read upstream data from input_data[0], input_data[1], etc. according to expected_input_dimension."),
        makeSpec("data_to_file", NodeTypes::Function, "Function Data to File",
            "Read JSON-compatible slot data and write a CSV/file artifact descriptor downstream.", "data_to_file",
            {"input"}, {"output"}, pythonConfig("data_to_file", dataToFileCode), dataToFileCode,
            baseInstructions() + " Read upstream data from input_data[0], input_data[1], etc. " + fileOutputInstructions()),
        makeSpec("file_to_data", NodeTypes::Function, "Function File to Data",
            "Read an upstream file descriptor object, open the file, and return extracted JSON-compatible data.", "file_to_data",
            {"input"}, {"output"}, pythonConfig("file_to_data", fileToDataCode), fileToDataCode,
            baseInstructions() + " " + fileInputInstructions()),
        makeSpec("file_to_file", NodeTypes::Function, "Function File to File",
            "Read an upstream file descriptor object and produce another CSV/file artifact descriptor.", "file_to_file",
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
        makeSpec("subsystem_basic", NodeTypes::Subsystem, "Subsystem",
            "A nested workflow node used to group a multi-step sub-process inside one node; use it as the body of Loop when a loop body needs more than one internal step.", "subworkflow",
            {}, {}, subsystemConfig(), QString(),
            "Subsystem nodes do not contain direct Python code. In a skeleton, use subsystem_basic when a composite step is needed. The app infers boundary ports after the internal workflow is designed; a zero-input subsystem can act as a composite source."),

        makeSpec("loop", NodeTypes::Loop, "Loop",
            "Run fixed iterations; each iteration executes this Loop node's Python code, then exactly one direct body node.", "loop",
            {"input"}, {"output"}, loopConfig(loopCode), loopCode,
            "Strictly follow the current code_template. Do not wrap code in markdown fences. " + loopInstructions()),
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
