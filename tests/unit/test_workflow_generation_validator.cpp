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
      "config": {"language": "python", "entry": "run", "code": "def run(inputs, context):\n    return {\"outputs\": {\"output\": [{}]}, \"artifacts\": []}"},
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
      "config": {"language": "python", "entry": "run", "code": "def run(inputs, context):\n    input_data = inputs.get(\"input\", [])\n    return {\"outputs\": {\"output\": [input_data[0] if input_data else {}]}, \"artifacts\": []}"},
      "runtime": {"timeout_ms": 300000, "retry_count": 0, "max_memory_mb": 1024, "concurrency_group": "default"}
    }
  ],
  "edges": [
    {"edge_id": "edge_1", "from_node": "starter_input", "from_port": "output", "from_slot": 0, "to_node": "work_step", "to_port": "input", "to_slot": 0}
  ]
})");
}

QString zeroInputSubsystemWorkflowJson()
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
      "node_id": "source_subsystem",
      "template_id": "subsystem_basic",
      "type": "subsystem",
      "name": "Source Subsystem",
      "description": "",
      "position": {"x": 80, "y": 80},
      "rotation_degrees": 0,
      "input_ports": [],
      "output_ports": ["output"],
      "config": {"subsystem_schema_version": 1, "subsystem_workflow": {}, "subsystem_boundary": {"inputs": [], "outputs": []}},
      "runtime": {"timeout_ms": 300000, "retry_count": 0, "max_memory_mb": 1024, "concurrency_group": "default"}
    }
  ],
  "edges": []
})");
}

QString validLoopWorkflowJson()
{
    return QStringLiteral(R"({
  "schema_version": 1,
  "workflow_id": "",
  "workspace_id": "",
  "name": "Generated Loop",
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
      "config": {"language": "python", "entry": "run", "code": "def run(inputs, context):\n    return {\"outputs\": {\"output\": [{}]}, \"artifacts\": []}"},
      "runtime": {"timeout_ms": 300000, "retry_count": 0, "max_memory_mb": 1024, "concurrency_group": "default"}
    },
    {
      "node_id": "loop",
      "template_id": "loop",
      "type": "loop",
      "name": "Loop",
      "description": "",
      "position": {"x": 300, "y": 80},
      "rotation_degrees": 0,
      "input_ports": ["input"],
      "output_ports": ["output"],
      "config": {"language": "python", "entry": "run", "loop_iterations": 3, "code": "def run(inputs, context):\n    loop = context.get(\"loop\", {})\n    input_data = inputs.get(\"input\", [])\n    return {\"outputs\": {\"output\": [input_data[0] if input_data else {}]}, \"artifacts\": []}"},
      "runtime": {"timeout_ms": 300000, "retry_count": 0, "max_memory_mb": 1024, "concurrency_group": "default"}
    },
    {
      "node_id": "body",
      "template_id": null,
      "type": "function",
      "name": "Body",
      "description": "",
      "position": {"x": 520, "y": 80},
      "rotation_degrees": 0,
      "input_ports": ["input"],
      "output_ports": ["output"],
      "config": {"language": "python", "entry": "run", "code": "def run(inputs, context):\n    input_data = inputs.get(\"input\", [])\n    return {\"outputs\": {\"output\": [input_data[0] if input_data else {}]}, \"artifacts\": []}"},
      "runtime": {"timeout_ms": 300000, "retry_count": 0, "max_memory_mb": 1024, "concurrency_group": "default"}
    }
  ],
  "edges": [
    {"edge_id": "edge_1", "from_node": "starter_input", "from_port": "output", "from_slot": 0, "to_node": "loop", "to_port": "input", "to_slot": 0},
    {"edge_id": "edge_2", "from_node": "loop", "from_port": "output", "from_slot": 0, "to_node": "body", "to_port": "input", "to_slot": 0}
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

    const auto zeroInputSubsystem = validator.validateJsonText(zeroInputSubsystemWorkflowJson());
    if (const auto check = expect(zeroInputSubsystem.valid,
            QString("Zero-input subsystem generated workflow should pass without a Starter: %1")
                .arg(zeroInputSubsystem.errors.join("; ")))) {
        return check;
    }

    const auto validLoop = validator.validateJsonText(validLoopWorkflowJson());
    if (const auto check = expect(validLoop.valid,
            QString("Generated single loop workflow should pass: %1").arg(validLoop.errors.join("; ")))) {
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
