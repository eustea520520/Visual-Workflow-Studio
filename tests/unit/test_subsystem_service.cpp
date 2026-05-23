#include "application/subsystem/SubsystemService.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"

#include <QCoreApplication>
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

vws::domain::Node makeFunctionNode(const QString& id, const QString& name)
{
    vws::domain::Node node;
    node.nodeId = id;
    node.name = name;
    node.type = vws::domain::NodeTypes::Function;
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    vws::domain::PortDimensionSpec input;
    input.portName = "input";
    input.dimension = 3;
    input.itemLabels = {"a", "b", "c"};
    node.ioSpec.inputs.append(input);
    vws::domain::PortDimensionSpec output;
    output.portName = "output";
    output.dimension = 2;
    output.itemLabels = {"x", "y"};
    node.ioSpec.outputs.append(output);
    return node;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    vws::application::SubsystemService service;
    vws::domain::NodePosition position;
    position.x = 10;
    position.y = 20;
    auto subsystem = service.createSubsystemNode("workspace-id", "Subsystem A", position);

    if (const auto result = expect(subsystem.type == vws::domain::NodeTypes::Subsystem,
            "createSubsystemNode should create subsystem type")) {
        return result;
    }
    if (const auto result = expect(subsystem.inputPorts.isEmpty() && subsystem.outputPorts.isEmpty(),
            "New empty subsystem should have no external ports")) {
        return result;
    }
    if (const auto result = expect(subsystem.config.contains(vws::domain::NodeConfigKeys::SubsystemWorkflow),
            "New subsystem should contain embedded workflow JSON")) {
        return result;
    }

    vws::domain::Workflow child;
    QString errorMessage;
    if (!service.loadSubsystemWorkflow(subsystem, child, &errorMessage)) {
        return fail(errorMessage);
    }
    child.nodes.append(makeFunctionNode("inner", "Inner Node"));
    if (!service.saveSubsystemWorkflow(subsystem, child, &errorMessage)) {
        return fail(errorMessage);
    }

    if (const auto result = expect(subsystem.inputPorts == QStringList{"Inner Node(input)"},
            "Boundary inference should expose entry node input port")) {
        return result;
    }
    if (const auto result = expect(subsystem.outputPorts == QStringList{"Inner Node(output)"},
            "Boundary inference should expose exit node output port")) {
        return result;
    }
    if (const auto result = expect(subsystem.ioSpec.inputs.first().dimension == 3
            && subsystem.ioSpec.outputs.first().dimension == 2,
            "Boundary inference should inherit internal port dimensions")) {
        return result;
    }

    vws::domain::Workflow reloadedChild;
    if (!service.loadSubsystemWorkflow(subsystem, reloadedChild, &errorMessage)) {
        return fail(errorMessage);
    }
    if (const auto result = expect(reloadedChild.nodes.size() == 1 && reloadedChild.nodes.first().nodeId == "inner",
            "Embedded subsystem workflow should survive save/load through node config")) {
        return result;
    }

    QTextStream(stdout) << "subsystem service tests passed" << Qt::endl;
    return 0;
}
