#include "application/generation/WorkflowGenerationPromptBuilder.h"

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
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    vws::application::WorkflowGenerationPromptBuilder builder;
    const auto prompt = builder.systemPrompt();
    if (const auto check = expect(prompt.contains("Return ONLY one JSON object"), "Prompt should force JSON-only output")) {
        return check;
    }
    if (const auto check = expect(prompt.contains("starter|function|agent|subsystem|loop"),
            "Prompt should list supported node types including loop nodes")) {
        return check;
    }
    if (const auto check = expect(prompt.contains("Multi-dimensional IO planning"),
            "Skeleton prompt should explain multi-dimensional IO planning")) {
        return check;
    }
    if (const auto check = expect(prompt.contains("agent_file_to_file")
            && prompt.contains("loop_iterations")
            && prompt.contains("exactly one direct body node"),
            "Skeleton prompt should list current agent/file/loop template families")) {
        return check;
    }
    if (const auto check = expect(prompt.contains("outputs[\"output\"] as a list")
            && prompt.contains("inputs[\"input\"] as a list")
            && prompt.contains("Use subsystem as the body for multi-node loops"),
            "Skeleton prompt should explain the current slot-list IO and loop/subsystem rules")) {
        return check;
    }
    const auto nodePrompt = builder.nodeImplementationSystemPrompt();
    if (const auto check = expect(nodePrompt.contains("output_file_path")
            && nodePrompt.contains("file_input = input_data[0]")
            && nodePrompt.contains("background_prompt"),
            "Node implementation prompt should force current file and agent template conventions")) {
        return check;
    }
    if (const auto check = expect(nodePrompt.contains("Do not return outputs[\"output\"] as a bare dict")
            && nodePrompt.contains("If expected_output_dimension is 3")
            && nodePrompt.contains("Loop node must keep only business data in outputs[\"output\"]"),
            "Node implementation prompt should prevent old output and loop data-shape mistakes")) {
        return check;
    }
    QTextStream(stdout) << "workflow generation prompt tests passed" << Qt::endl;
    return 0;
}
