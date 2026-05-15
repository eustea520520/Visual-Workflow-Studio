#include "application/WorkflowService.h"
#include "application/PythonCodeTemplates.h"
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

    const auto templateCode = vws::application::PythonCodeTemplates::defaultFunctionCode();
    if (const auto check = expect(templateCode.contains("input_data = inputs.get(\"input\", {})"),
            "Default Python template should show how to read the default input port")) {
        return check;
    }
    if (const auto check = expect(templateCode.contains("\"outputs\"") && templateCode.contains("\"output\""),
            "Default Python template should return outputs.output for downstream nodes")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::starterEmptyOutputCode().contains("\"output\": {}"),
            "Starter empty-output template should return an empty output object")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::defaultStarterCode().contains("output_data"),
            "Starter data-output template should create business data")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::defaultAgentCode().contains("urllib.request"),
            "Agent template should use the standard-library HTTP client")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::defaultAgentCode().contains("/chat/completions"),
            "Agent template should call an OpenAI-compatible chat completion endpoint")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                3,
                "background",
                "task",
                vws::application::DataTransferTemplate::FileToFile).contains("\"https://example.test/v1\""),
            "Agent template should embed structured Agent settings")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                3,
                "background",
                "task",
                vws::application::DataTransferTemplate::FileToFile).contains("input_mode = \"file\"")
            && vws::application::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                3,
                "background",
                "task",
                vws::application::DataTransferTemplate::FileToFile).contains("output_mode = \"file\""),
            "Agent file-to-file template should read a file and write a file artifact")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                3,
                "background",
                "task",
                vws::application::DataTransferTemplate::FileToData).contains("file_text = file.read()")
            && vws::application::PythonCodeTemplates::agentCode(
                "https://example.test/v1",
                "model-x",
                "key-x",
                3,
                "background",
                "task",
                vws::application::DataTransferTemplate::FileToData).contains("\"content\": file_text"),
            "Agent file-input template should pass the full file content, not a preview")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::starterFileOutputCode().contains("\"artifacts\"")
            && vws::application::PythonCodeTemplates::starterFileOutputCode().contains("\"file_path\""),
            "File starter template should register artifacts and pass file_path downstream")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::functionFileToFileCode().contains("source_path")
            && vws::application::PythonCodeTemplates::functionFileToFileCode().contains("\"artifacts\""),
            "File function template should read an upstream path and register an artifact")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::codeForTemplate(vws::application::DataTransferTemplate::DataToFile).contains("file_type = \"\"")
            && vws::application::PythonCodeTemplates::codeForTemplate(vws::application::DataTransferTemplate::DataToFile).contains("file_name = \"output\"")
            && !vws::application::PythonCodeTemplates::codeForTemplate(vws::application::DataTransferTemplate::DataToFile).contains("output_path = artifact_dir / \"output.csv\""),
            "Data-to-file template should use generic file_name without .csv as default output path")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::codeForTemplate(vws::application::DataTransferTemplate::FileToData).contains("source_format")
            && !vws::application::PythonCodeTemplates::codeForTemplate(vws::application::DataTransferTemplate::FileToData).contains("\"format\": input_data.get(\"format\", \"csv\")"),
            "File-to-data template should not default to CSV format")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::defaultAgentUrl().isEmpty()
            && vws::application::PythonCodeTemplates::defaultAgentModel().isEmpty()
            && !vws::application::PythonCodeTemplates::agentUrlPlaceholder().isEmpty()
            && !vws::application::PythonCodeTemplates::agentModelPlaceholder().isEmpty(),
            "Agent URL/model defaults should be empty and placeholders non-empty")) {
        return check;
    }
    if (const auto check = expect(vws::application::PythonCodeTemplates::defaultAgentMaxRetries() == 3,
            "Default agent max retries should be 3")) {
        return check;
    }
    const auto agentCode = vws::application::PythonCodeTemplates::agentCode(
        "https://example.test/v1", "model-x", "key-x", 3,
        "background", "task", vws::application::DataTransferTemplate::DataToData);
    if (const auto check = expect(agentCode.contains("max_retries"),
            "Agent template should contain max_retries")) {
        return check;
    }
    if (const auto check = expect(agentCode.contains("retry_count"),
            "Agent template should contain retry_count")) {
        return check;
    }
    if (const auto check = expect(agentCode.contains("for attempt_index in range(max_retries + 1)"),
            "Agent template should contain retry loop")) {
        return check;
    }
    if (const auto check = expect(agentCode.contains("Agent request failed after"),
            "Agent template should have retry exhaustion error")) {
        return check;
    }
    if (const auto check = expect(agentCode.contains("_sleep_before_retry"),
            "Agent template should define _sleep_before_retry helper")) {
        return check;
    }
    if (const auto check = expect(agentCode.contains("_post_chat_completion"),
            "Agent template should define _post_chat_completion helper")) {
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
        300000,
        "agent",
        {{"io_template", "data_to_data"}},
        "def run(inputs, context):\n    return {\"outputs\": {\"output\": {\"custom\": True}}, \"artifacts\": []}\n",
        vws::application::PythonCodeTemplates::defaultAgentCode());
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
        [&savedAgentCode](const QString&, const QString&, int, const QString& code, const QJsonObject&) {
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
    if (const auto check = expect(workflowService.updateNodeDetails(
                workflow,
                "node-1",
                "Agent",
                "Calls a model",
                "agent-code",
                {{"agent_max_retries", 5}},
                &errorMessage),
            "WorkflowService should merge agent_max_retries config field")) {
        return check;
    }
    if (const auto check = expect(workflow.nodes.first().config.value("agent_max_retries").toInt() == 5,
            "Agent config patch should persist max request retries")) {
        return check;
    }

    QTextStream(stdout) << "python editor tests passed" << Qt::endl;
    return 0;
}
