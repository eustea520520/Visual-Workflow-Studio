#include "application/generation/WorkflowGenerationValidator.h"

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

QString validWorkflowJson()
{
    return QStringLiteral(R"({
  "schema_version": 1,
  "workflow_id": "",
  "workspace_id": "",
  "name": "Generated",
  "description": "",
  "created_at": "",
  "updated_at": "",
  "version": 1,
  "nodes": [
    {
      "node_id": "starter_input",
      "template_id": null,
      "type": "starter",
      "name": "Input",
      "description": "",
      "position": {"x": 80, "y": 80},
      "rotation_degrees": 0,
      "input_ports": [],
      "output_ports": ["output"],
      "config": {"language": "python", "entry": "run", "code": "def run(inputs, context):\n    return {\"outputs\": {\"output\": {}}, \"artifacts\": []}"},
      "runtime": {"timeout_ms": 300000, "retry_count": 0, "max_memory_mb": 1024, "concurrency_group": "default"}
    },
    {
      "node_id": "work_step",
      "template_id": null,
      "type": "function",
      "name": "Step",
      "description": "",
      "position": {"x": 400, "y": 80},
      "rotation_degrees": 0,
      "input_ports": ["input"],
      "output_ports": ["output"],
      "config": {"language": "python", "entry": "run", "code": "def run(inputs, context):\n    return {\"outputs\": {\"output\": inputs.get(\"input\", {})}, \"artifacts\": []}"},
      "runtime": {"timeout_ms": 300000, "retry_count": 0, "max_memory_mb": 1024, "concurrency_group": "default"}
    }
  ],
  "edges": [
    {"edge_id": "edge_1", "from_node": "starter_input", "from_port": "output", "to_node": "work_step", "to_port": "input"}
  ]
})");
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    vws::application::WorkflowGenerationValidator validator;
    const auto valid = validator.validateJsonText(validWorkflowJson());
    if (const auto check = expect(valid.valid, QString("Valid generated workflow should pass: %1").arg(valid.errors.join("; ")))) {
        return check;
    }

    auto invalid = validWorkflowJson();
    invalid.replace("\"type\": \"starter\"", "\"type\": \"function\"");
    const auto noStarter = validator.validateJsonText(invalid);
    if (const auto check = expect(!noStarter.valid, "Workflow without starter should fail")) {
        return check;
    }

    auto cyclic = validWorkflowJson();
    cyclic.replace("  ]\n}", "    ,{\"edge_id\": \"edge_2\", \"from_node\": \"work_step\", \"from_port\": \"output\", \"to_node\": \"starter_input\", \"to_port\": \"input\"}\n  ]\n}");
    const auto cycleResult = validator.validateJsonText(cyclic);
    if (const auto check = expect(!cycleResult.valid, "Workflow with a cycle should fail")) {
        return check;
    }

    QTextStream(stdout) << "workflow generation validator tests passed" << Qt::endl;
    return 0;
}
