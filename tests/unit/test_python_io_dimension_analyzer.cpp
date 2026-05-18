#include "application/io/PythonIoDimensionAnalyzer.h"
#include "domain/NodeConfigKeys.h"

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
    vws::domain::Node node;
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.config.insert(vws::domain::NodeConfigKeys::Code, QStringLiteral(
        "# vws:input input dimension=2 labels=raw,config\n"
        "# vws:output output dimension=3 labels=a,b,c\n"
        "def run(inputs, context):\n"
        "    return {\"outputs\": {\"output\": []}, \"artifacts\": []}\n"));

    const auto spec = vws::application::PythonIoDimensionAnalyzer().analyze(node);
    if (const auto check = expect(spec.inputs.size() == 1 && spec.inputs.first().dimension == 2,
            "Python analyzer should read input dimension comments")) {
        return check;
    }
    if (const auto check = expect(spec.outputs.size() == 1 && spec.outputs.first().dimension == 3,
            "Python analyzer should read output dimension comments")) {
        return check;
    }
    if (const auto check = expect(spec.outputs.first().itemLabels == QStringList({"a", "b", "c"}),
            "Python analyzer should read output labels")) {
        return check;
    }

    vws::domain::Node indexedOnlyNode;
    indexedOnlyNode.inputPorts = {"input"};
    indexedOnlyNode.outputPorts = {"output"};
    indexedOnlyNode.config.insert(vws::domain::NodeConfigKeys::Code, QStringLiteral(
        "def run(inputs, context):\n"
        "    input_data = inputs.get(\"input\", [])\n"
        "    value = input_data[2].get(\"name\")\n"
        "    return {\"outputs\": {\"output\": value}, \"artifacts\": []}\n"));

    const auto indexedOnlySpec = vws::application::PythonIoDimensionAnalyzer().analyze(indexedOnlyNode);
    if (const auto check = expect(indexedOnlySpec.inputs.isEmpty() && indexedOnlySpec.outputs.isEmpty(),
            "Python analyzer should not infer dimensions from indexed code access without vws comments")) {
        return check;
    }

    QTextStream(stdout) << "python io dimension analyzer tests passed" << Qt::endl;
    return 0;
}
