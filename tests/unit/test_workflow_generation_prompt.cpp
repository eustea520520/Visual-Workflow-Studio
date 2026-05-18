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
    if (const auto check = expect(prompt.contains("starter|function|agent"), "Prompt should list supported node types")) {
        return check;
    }
    if (const auto check = expect(prompt.contains("Multi-dimensional IO model"),
            "Skeleton prompt should explain multi-dimensional IO planning")) {
        return check;
    }
    if (const auto check = expect(prompt.contains("agent_file_to_file"),
            "Skeleton prompt should list current agent/file template families")) {
        return check;
    }
    const auto nodePrompt = builder.nodeImplementationSystemPrompt();
    if (const auto check = expect(nodePrompt.contains("output_file_path")
            && nodePrompt.contains("file_input = input_data[0]")
            && nodePrompt.contains("background_prompt"),
            "Node implementation prompt should force current file and agent template conventions")) {
        return check;
    }
    QTextStream(stdout) << "workflow generation prompt tests passed" << Qt::endl;
    return 0;
}
