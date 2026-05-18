#include "ui/generation/WorkflowGenerationDialog.h"

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
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
    QApplication app(argc, argv);
    vws::ui::WorkflowGenerationDialog dialog;
    dialog.render({"https://api.openai.com/v1", "model", "prompt", "requirement", "{}", "", false});

    if (const auto check = expect(dialog.findChild<QLineEdit*>("llmUrlEdit") != nullptr, "Dialog should expose URL field")) {
        return check;
    }
    if (const auto check = expect(dialog.findChild<QLineEdit*>("llmModelEdit") != nullptr, "Dialog should expose model field")) {
        return check;
    }
    if (const auto check = expect(dialog.findChild<QLineEdit*>("llmApiKeyEdit") != nullptr, "Dialog should expose API key field")) {
        return check;
    }
    if (const auto check = expect(dialog.findChild<QPlainTextEdit*>("presetPromptPreview") != nullptr, "Dialog should expose preset prompt preview")) {
        return check;
    }
    if (const auto check = expect(dialog.findChild<QPlainTextEdit*>("generatedJsonPreview") != nullptr, "Dialog should expose generated JSON preview")) {
        return check;
    }
    if (const auto check = expect(dialog.findChild<QPushButton*>("generateWorkflowButton") != nullptr, "Dialog should expose generate button")) {
        return check;
    }

    QTextStream(stdout) << "workflow generation dialog tests passed" << Qt::endl;
    return 0;
}
