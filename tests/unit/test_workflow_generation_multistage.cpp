#include "application/generation/WorkflowGenerationAssembler.h"
#include "application/generation/WorkflowGenerationTemplateCatalog.h"
#include "application/generation/WorkflowNodeImplementationValidator.h"
#include "application/generation/WorkflowSkeletonValidator.h"
#include "domain/Workspace.h"

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

QString validSkeletonJson()
{
    return QStringLiteral(R"({
  "name": "Data to File Test",
  "description": "Creates business data and writes it to a csv file.",
  "nodes": [
    {
      "node_id": "starter_data",
      "template_id": "starter_data_output",
      "type": "starter",
      "name": "Starter Data",
      "purpose": "Create initial values.",
      "input_contract": "none",
      "output_contract": "{\"a\": number, \"b\": number}",
      "depends_on_node_ids": [],
      "layer": 0,
      "row": 0
    },
    {
      "node_id": "write_csv",
      "template_id": "data_to_file",
      "type": "function",
      "name": "Write CSV",
      "purpose": "Write the incoming values to a CSV file.",
      "input_contract": "{\"a\": number, \"b\": number}",
      "output_contract": "{\"file_path\": string, \"format\": \"csv\"}",
      "depends_on_node_ids": ["starter_data"],
      "layer": 1,
      "row": 0
    }
  ],
  "edges": [
    {"edge_id": "edge_data_to_csv", "from_node": "starter_data", "from_port": "output", "to_node": "write_csv", "to_port": "input"}
  ]
})");
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    vws::application::WorkflowGenerationTemplateCatalog catalog;
    if (const auto check = expect(catalog.contains("data_to_file"), "Template catalog should expose data_to_file")) {
        return check;
    }
    if (const auto check = expect(catalog.contains("agent_file_to_file"), "Template catalog should expose agent_file_to_file")) {
        return check;
    }

    vws::application::WorkflowSkeleton skeleton;
    QStringList errors;
    vws::application::WorkflowSkeletonValidator skeletonValidator;
    if (const auto check = expect(skeletonValidator.validateJsonText(validSkeletonJson(), catalog, skeleton, errors),
            QString("Valid skeleton should pass: %1").arg(errors.join("; ")))) {
        return check;
    }

    vws::application::NodeImplementation starterImpl;
    QStringList implErrors;
    const auto starterSpec = catalog.fullSpec("starter_data_output").value();
    const auto starterJson = QStringLiteral(R"({
  "node_id": "starter_data",
  "code": "# vws:output output dimension=1 labels=1\ndef run(inputs, context):\n    return {\"outputs\": {\"output\": {\"a\": 1, \"b\": 2}}, \"artifacts\": []}",
  "config_patch": {"language": "python", "entry": "run"},
  "timeout_ms": 300000,
  "notes": "Create values."
})");
    vws::application::WorkflowNodeImplementationValidator implementationValidator;
    if (const auto check = expect(implementationValidator.validateJsonText(starterJson, skeleton.nodes.first(), starterSpec, starterImpl, implErrors),
            QString("Valid starter implementation should pass: %1").arg(implErrors.join("; ")))) {
        return check;
    }

    vws::application::NodeImplementation fileImpl;
    implErrors.clear();
    const auto fileSpec = catalog.fullSpec("data_to_file").value();
    const auto fileJson = QStringLiteral(R"({
  "node_id": "write_csv",
  "code": "# vws:input input dimension=1 labels=1\n# vws:output output dimension=1 labels=1\nfrom pathlib import Path\nimport csv\n\ndef run(inputs, context):\n    input_data = inputs.get(\"input\", {})\n    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n    artifact_dir.mkdir(parents=True, exist_ok=True)\n    output_file_path = \"output.csv\"\n    output_path = artifact_dir / output_file_path\n    with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as f:\n        writer = csv.DictWriter(f, fieldnames=[\"a\", \"b\"])\n        writer.writeheader()\n        writer.writerow(input_data)\n    return {\"outputs\": {\"output\": {\"file_path\": str(output_path), \"format\": output_path.suffix.lstrip(\".\"), \"size_bytes\": output_path.stat().st_size}}, \"artifacts\": [{\"type\": output_path.suffix.lstrip(\".\"), \"path\": str(output_path), \"metadata\": {\"size_bytes\": output_path.stat().st_size}}]}",
  "config_patch": {"language": "python", "entry": "run"},
  "timeout_ms": 300000,
  "notes": "Write CSV."
})");
    if (const auto check = expect(implementationValidator.validateJsonText(fileJson, skeleton.nodes.last(), fileSpec, fileImpl, implErrors),
            QString("Valid data_to_file implementation should pass: %1").arg(implErrors.join("; ")))) {
        return check;
    }

    vws::domain::Workspace workspace;
    workspace.id = "workspace-test";
    workspace.rootPath = "not-used";

    QHash<QString, vws::application::NodeImplementation> implementations;
    implementations.insert(starterImpl.nodeId, starterImpl);
    implementations.insert(fileImpl.nodeId, fileImpl);
    vws::domain::Workflow workflow;
    QStringList assembleErrors;
    vws::application::WorkflowGenerationAssembler assembler;
    if (const auto check = expect(assembler.assemble(skeleton, implementations, catalog, workspace, workflow, assembleErrors),
            QString("Assembler should create workflow: %1").arg(assembleErrors.join("; ")))) {
        return check;
    }
    if (const auto check = expect(workflow.nodes.size() == 2, "Assembled workflow should contain two nodes")) {
        return check;
    }
    if (const auto check = expect(workflow.nodes.last().config.value("io_template").toString() == "data_to_file",
            "Assembler should preserve strict data_to_file template")) {
        return check;
    }

    QTextStream(stdout) << "workflow generation multistage tests passed" << Qt::endl;
    return 0;
}
