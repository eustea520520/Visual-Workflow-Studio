#include "application/WorkflowService.h"
#include "ui/editor/PythonCodeTemplates.h"
#include "ui/editor/PythonCodeEditor.h"
#include "ui/editor/PythonNodeEditorDialog.h"
#include "ui/editor/PythonSyntaxHighlighter.h"

#include <QApplication>
#include <QFontInfo>
#include <QKeyEvent>
#include <QPushButton>
#include <QTextBlock>
#include <QTextDocument>
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    const auto templateCode = vws::ui::PythonCodeTemplates::defaultFunctionCode();
    if (const auto check = expect(templateCode.contains("input_data = inputs.get(\"input\", {})"),
            "Default Python template should show how to read the default input port")) {
        return check;
    }
    if (const auto check = expect(templateCode.contains("\"outputs\"") && templateCode.contains("\"output\""),
            "Default Python template should return outputs.output for downstream nodes")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::starterEmptyOutputCode().contains("\"output\": {}"),
            "Starter empty-output template should return an empty output object")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::defaultStarterCode().contains("output_data"),
            "Starter data-output template should create business data")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::defaultAgentCode().contains("urllib.request"),
            "Agent template should use the standard-library HTTP client")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::defaultAgentCode().contains("/chat/completions"),
            "Agent template should call an OpenAI-compatible chat completion endpoint")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                "background",
                "task",
                vws::ui::DataTransferTemplate::FileToFile).contains("\"https://example.test/v1\""),
            "Agent template should embed structured Agent settings")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                "background",
                "task",
                vws::ui::DataTransferTemplate::FileToFile).contains("input_mode = \"file\"")
            && vws::ui::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                "background",
                "task",
                vws::ui::DataTransferTemplate::FileToFile).contains("output_mode = \"file\""),
            "Agent file-to-file template should read a file and write a file artifact")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                "background",
                "task",
                vws::ui::DataTransferTemplate::FileToData).contains("file_text = file.read()")
            && vws::ui::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                "background",
                "task",
                vws::ui::DataTransferTemplate::FileToData).contains("\"content\": file_text"),
            "Agent file-input template should pass the full file content, not a preview")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::starterFileOutputCode().contains("\"artifacts\"")
            && vws::ui::PythonCodeTemplates::starterFileOutputCode().contains("\"file_path\""),
            "File starter template should register artifacts and pass file_path downstream")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::functionFileToFileCode().contains("source_path")
            && vws::ui::PythonCodeTemplates::functionFileToFileCode().contains("\"artifacts\""),
            "File function template should read an upstream path and register an artifact")) {
        return check;
    }
    if (const auto check = expect(vws::ui::PythonCodeTemplates::codeForTemplate(vws::ui::DataTransferTemplate::DataToFile).contains("output.csv")
            && vws::ui::PythonCodeTemplates::codeForTemplate(vws::ui::DataTransferTemplate::FileToData).contains("csv.DictReader"),
            "Function templates should cover data-to-file and file-to-data flows")) {
        return check;
    }

    vws::ui::PythonCodeEditor editor;
    app.setStyleSheet("QWidget { font-family: Arial; } QPlainTextEdit { font-family: Arial; }");
    QApplication::processEvents();
    if (const auto check = expect(QFontInfo(editor.font()).family().contains("Consolas", Qt::CaseInsensitive),
            "PythonCodeEditor should keep Consolas when the app stylesheet changes the default editor font")) {
        return check;
    }

    vws::ui::PythonNodeEditorDialog agentDialog(
        "Agent",
        "Calls a model",
        "agent",
        {{"io_template", "data_to_data"}},
        "def run(inputs, context):\n    return {\"outputs\": {\"output\": {\"custom\": True}}, \"artifacts\": []}\n",
        vws::ui::PythonCodeTemplates::defaultAgentCode());
    auto* agentEditor = agentDialog.findChild<vws::ui::PythonCodeEditor*>();
    if (const auto check = expect(agentEditor != nullptr && !agentEditor->isReadOnly(),
            "Agent Python editor should be editable")) {
        return check;
    }
    const QString customAgentCode = "def run(inputs, context):\n    return {\"outputs\": {\"output\": {\"edited\": True}}, \"artifacts\": []}\n";
    QString savedAgentCode;
    QObject::connect(
        &agentDialog,
        &vws::ui::PythonNodeEditorDialog::nodeSaved,
        &agentDialog,
        [&savedAgentCode](const QString&, const QString&, const QString& code, const QJsonObject&) {
            savedAgentCode = code;
        });
    agentEditor->setCode(customAgentCode);
    auto* saveButton = agentDialog.findChild<QPushButton*>("saveNodeButton");
    if (const auto check = expect(saveButton != nullptr, "Agent editor should expose the Save button")) {
        return check;
    }
    saveButton->click();
    if (const auto check = expect(savedAgentCode == customAgentCode,
            "Saving an Agent node should preserve manually edited Python code")) {
        return check;
    }

    editor.setCode({});
    QKeyEvent tabEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QApplication::sendEvent(&editor, &tabEvent);
    if (const auto check = expect(editor.code() == "    ", "Tab should insert four spaces")) {
        return check;
    }

    editor.setCode("if True:");
    auto cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::End);
    editor.setTextCursor(cursor);
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&editor, &enterEvent);
    if (const auto check = expect(editor.code().endsWith("\n    "), "Enter after colon should add one indent level")) {
        return check;
    }

    QTextDocument document;
    vws::ui::PythonSyntaxHighlighter highlighter(&document);
    document.setPlainText("\"\"\"\ndef should_not_highlight():\n# not a comment\n\"\"\"\nvalue = 1");
    highlighter.rehighlight();
    if (const auto check = expect(document.findBlockByNumber(0).userState() == 1
            && document.findBlockByNumber(1).userState() == 1
            && document.findBlockByNumber(2).userState() == 1
            && document.findBlockByNumber(3).userState() == 0,
            "Triple-quoted Python strings should keep multiline highlighter state until the closing delimiter")) {
        return check;
    }

    vws::application::WorkflowService workflowService;
    auto workflow = workflowService.createEmptyWorkflow("workspace", "workflow");
    vws::domain::Node node;
    node.nodeId = "node-1";
    node.type = "function";
    node.config = {{"language", "python"}, {"code", "old"}};
    workflow.nodes.append(node);

    QString errorMessage;
    if (const auto check = expect(workflowService.updateNodeDetails(workflow, "node-1", "Renamed", "Does work", "new-code", &errorMessage),
            "WorkflowService should update node title, description, and config.code")) {
        return check;
    }
    if (const auto check = expect(workflow.nodes.first().config.value("code").toString() == "new-code",
            "Updated node code should be stored in config.code")) {
        return check;
    }
    if (const auto check = expect(workflow.nodes.first().name == "Renamed" && workflow.nodes.first().description == "Does work",
            "Updated node metadata should be stored on the node")) {
        return check;
    }
    if (const auto check = expect(workflowService.updateNodeDetails(
                workflow,
                "node-1",
                "Agent",
                "Calls a model",
                "agent-code",
                {{"agent_url", "https://example.test/v1"}, {"agent_model", "model-x"}},
                &errorMessage),
            "WorkflowService should merge Agent config fields into node.config")) {
        return check;
    }
    if (const auto check = expect(workflow.nodes.first().config.value("agent_model").toString() == "model-x",
            "Agent config patch should persist model name")) {
        return check;
    }

    QTextStream(stdout) << "python editor tests passed" << Qt::endl;
    return 0;
}
