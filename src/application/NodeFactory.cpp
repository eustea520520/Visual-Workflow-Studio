#include "application/NodeFactory.h"

#include "application/io/NodeIoSpecUtils.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"

#include <QUuid>

namespace vws::application {

namespace ConfigKeys = domain::NodeConfigKeys;
namespace NodeTypes = domain::NodeTypes;

namespace {

QString nodeOrdinal(qsizetype existingNodeCount)
{
    return QString::number(existingNodeCount + 1);
}

DataTransferTemplate starterCodeTemplate(NodeFactory::StarterTemplateKind templateKind)
{
    switch (templateKind) {
    case NodeFactory::StarterTemplateKind::EmptyOutput:
        return DataTransferTemplate::EmptyOutput;
    case NodeFactory::StarterTemplateKind::DataOutput:
        return DataTransferTemplate::DataOutput;
    case NodeFactory::StarterTemplateKind::FileOutput:
        return DataTransferTemplate::FileOutput;
    }
    return DataTransferTemplate::DataOutput;
}

} // namespace

domain::Node NodeFactory::createStarterNode(
    const QPointF& scenePos,
    qsizetype existingNodeCount,
    StarterTemplateKind templateKind)
{
    const auto codeTemplate = starterCodeTemplate(templateKind);

    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = NodeTypes::Starter;
    node.name = QStringLiteral("Starter Node %1").arg(nodeOrdinal(existingNodeCount));
    node.description = QStringLiteral("Output-only starter node created from the canvas context menu.");
    node.position.x = scenePos.x();
    node.position.y = scenePos.y();
    node.inputPorts = {};
    node.outputPorts = {"output"};
    node.config = {
        {ConfigKeys::Language, "python"},
        {ConfigKeys::Entry, "run"},
        {ConfigKeys::IoTemplate, PythonCodeTemplates::templateKey(codeTemplate)},
        {ConfigKeys::Code, PythonCodeTemplates::codeForTemplate(codeTemplate)},
    };
    node.ioSpec = NodeIoSpecUtils::defaultSpecForNode(node);
    return node;
}

domain::Node NodeFactory::createFunctionNode(
    const QPointF& scenePos,
    qsizetype existingNodeCount,
    DataTransferTemplate templateKind)
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = NodeTypes::Function;
    node.name = QStringLiteral("Function Node %1").arg(nodeOrdinal(existingNodeCount));
    node.description = QStringLiteral("Python function node created from the canvas context menu.");
    node.position.x = scenePos.x();
    node.position.y = scenePos.y();
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.config = {
        {ConfigKeys::Language, "python"},
        {ConfigKeys::Entry, "run"},
        {ConfigKeys::IoTemplate, PythonCodeTemplates::templateKey(templateKind)},
        {ConfigKeys::Code, PythonCodeTemplates::codeForTemplate(templateKind)},
    };
    node.ioSpec = NodeIoSpecUtils::defaultSpecForNode(node);
    return node;
}

domain::Node NodeFactory::createAgentNode(
    const QPointF& scenePos,
    qsizetype existingNodeCount,
    DataTransferTemplate templateKind)
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = NodeTypes::Agent;
    node.name = QStringLiteral("Agent Node %1").arg(nodeOrdinal(existingNodeCount));
    node.description = QStringLiteral("Agent node created from the canvas context menu.");
    node.position.x = scenePos.x();
    node.position.y = scenePos.y();
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.config = {
        {ConfigKeys::Language, "python"},
        {ConfigKeys::Entry, "run"},
        {ConfigKeys::AgentUrl, ""},
        {ConfigKeys::AgentModel, ""},
        {ConfigKeys::AgentApiKey, ""},
        {ConfigKeys::AgentMaxRetries, PythonCodeTemplates::defaultAgentMaxRetries()},
        {ConfigKeys::AgentBackgroundPrompt, PythonCodeTemplates::defaultAgentBackgroundPrompt()},
        {ConfigKeys::AgentTaskPrompt, PythonCodeTemplates::defaultAgentTaskPrompt()},
        {ConfigKeys::IoTemplate, PythonCodeTemplates::templateKey(templateKind)},
        {ConfigKeys::Code, PythonCodeTemplates::agentCode(
                QString(),
                QString(),
                QString(),
                PythonCodeTemplates::defaultAgentMaxRetries(),
                PythonCodeTemplates::defaultAgentBackgroundPrompt(),
                PythonCodeTemplates::defaultAgentTaskPrompt(),
                templateKind)},
    };
    node.ioSpec = NodeIoSpecUtils::defaultSpecForNode(node);
    return node;
}

domain::Node NodeFactory::createLoopNode(
    const QPointF& scenePos,
    qsizetype existingNodeCount,
    int iterations)
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = NodeTypes::Loop;
    node.name = QStringLiteral("Loop Node %1").arg(nodeOrdinal(existingNodeCount));
    node.description = QStringLiteral("Runs the next node repeatedly with generated per-iteration input.");
    node.position.x = scenePos.x();
    node.position.y = scenePos.y();
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.config = {
        {ConfigKeys::Language, "python"},
        {ConfigKeys::Entry, "run"},
        {ConfigKeys::IoTemplate, "loop"},
        {ConfigKeys::LoopIterations, iterations},
        {ConfigKeys::Code, PythonCodeTemplates::loopCode()},
    };
    node.ioSpec = NodeIoSpecUtils::defaultSpecForNode(node);
    return node;
}

} // namespace vws::application
