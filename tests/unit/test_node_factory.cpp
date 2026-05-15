#include "application/NodeFactory.h"
#include "domain/NodeConfigView.h"
#include "domain/NodeTypes.h"

#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}

int expect(bool condition, const QString& message)
{
    return condition ? 0 : fail(message);
}

} // namespace

int main()
{
    using vws::application::DataTransferTemplate;
    using vws::application::NodeFactory;
    namespace NodeTypes = vws::domain::NodeTypes;

    const auto starter = NodeFactory::createStarterNode(
        QPointF(10, 20),
        0,
        NodeFactory::StarterTemplateKind::FileOutput);
    const vws::domain::NodeConfigView starterConfig(starter.config);
    if (const auto check = expect(starter.type == NodeTypes::Starter, "Starter factory should create starter node")) {
        return check;
    }
    if (const auto check = expect(starter.inputPorts.isEmpty() && starter.outputPorts == QStringList{"output"},
            "Starter node should have no inputs and one output")) {
        return check;
    }
    if (const auto check = expect(starterConfig.ioTemplate() == "starter_file_output",
            "Starter file template key should be stored in config")) {
        return check;
    }
    if (const auto check = expect(starter.position.x == 10 && starter.position.y == 20,
            "Factory-created nodes should preserve requested canvas position")) {
        return check;
    }

    const auto function = NodeFactory::createFunctionNode(
        QPointF(30, 40),
        4,
        DataTransferTemplate::FileToData);
    if (const auto check = expect(function.name == "Function Node 5",
            "Function node name should use existing node count as ordinal")) {
        return check;
    }
    const vws::domain::NodeConfigView functionConfig(function.config);
    if (const auto check = expect(functionConfig.ioTemplate() == "file_to_data",
            "Function template key should be stored in config")) {
        return check;
    }

    const auto agent = NodeFactory::createAgentNode(
        QPointF(50, 60),
        6,
        DataTransferTemplate::DataToFile);
    const vws::domain::NodeConfigView agentConfig(agent.config);
    if (const auto check = expect(agent.type == NodeTypes::Agent, "Agent factory should create agent node")) {
        return check;
    }
    if (const auto check = expect(agentConfig.agentMaxRetries(0) > 0
            && agentConfig.ioTemplate() == "data_to_file",
            "Agent node should contain Agent defaults and selected transfer template")) {
        return check;
    }
    if (const auto check = expect(agentConfig.code().contains("/chat/completions"),
            "Agent node should be initialized with executable Python Agent code")) {
        return check;
    }

    QTextStream(stdout) << "node factory tests passed" << Qt::endl;
    return 0;
}
