#include "ui/inspector/NodeInspector.h"

#include "ui/editor/PythonCodeEditor.h"
#include "ui/editor/PythonCodeTemplates.h"

#include <QColor>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vws::ui {

NodeInspector::NodeInspector(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("inspector"));
    setProperty("panel", true);
    buildUi();
}

void NodeInspector::displayNode(const domain::Node& node)
{
    displayNode(node, {});
}

void NodeInspector::displayNode(const domain::Node& node, const QJsonObject& selectedNodeOutput)
{
    m_currentNodeId = node.nodeId;

    if (m_pythonEditor != nullptr) {
        const auto nextCode = node.config.value("code").toString();
        if (m_pythonEditor->code() != nextCode) {
            m_pythonEditor->setCode(nextCode);
        }
    }

    if (node.type.trimmed().toLower() != "agent") {
        if (m_tabs != nullptr) {
            m_tabs->setTabEnabled(1, false);
            m_tabs->setCurrentIndex(0);
        }
    } else {
        if (m_tabs != nullptr) {
            m_tabs->setTabEnabled(1, true);
            m_tabs->setCurrentIndex(1);
        }
        m_agentTitleEdit->setText(node.name);
        m_agentDescriptionEdit->setText(node.description);
        m_agentTimeoutEdit->setText(QString::number(node.runtime.timeoutMs));
        m_agentTemplateEdit->setText(node.config.value("io_template").toString("data_to_data"));
        m_agentUrlEdit->setText(node.config.value("agent_url").toString());
        m_agentModelEdit->setText(node.config.value("agent_model").toString());
        m_agentApiKeyEdit->setText(node.config.value("agent_api_key").toString());
        m_agentMaxRetriesEdit->setText(QString::number(
            node.config.value("agent_max_retries").toInt(PythonCodeTemplates::defaultAgentMaxRetries())));
        m_agentBackgroundPromptEdit->setPlainText(node.config.value("agent_background_prompt").toString(
            PythonCodeTemplates::defaultAgentBackgroundPrompt()));
        m_agentTaskPromptEdit->setPlainText(node.config.value("agent_task_prompt").toString(
            PythonCodeTemplates::defaultAgentTaskPrompt()));
    }

    if (selectedNodeOutput.isEmpty()) {
        m_outputJsonEditor->setPlainText(
            tr("No Output JSON is available for this selected node yet."));
    } else {
        m_outputJsonEditor->setPlainText(
            QString::fromUtf8(
                QJsonDocument(selectedNodeOutput).toJson(QJsonDocument::Indented)));
    }
}

void NodeInspector::clearSelectedNodeOutput()
{
    m_currentNodeId.clear();
    if (m_outputJsonEditor != nullptr) {
        m_outputJsonEditor->setPlaceholderText(tr("JSON output here"));
        m_outputJsonEditor->setCode(QString());
    }
}

void NodeInspector::clear()
{
    m_currentNodeId.clear();

    if (m_pythonEditor != nullptr) {
        m_pythonEditor->setPlaceholderText(tr("Python code here"));
        m_pythonEditor->setCode(QString());
    }

    if (m_outputJsonEditor != nullptr) {
        m_outputJsonEditor->setPlaceholderText(tr("JSON output here"));
        m_outputJsonEditor->setCode(QString());
    }

    if (m_tabs != nullptr) {
        m_tabs->setTabEnabled(1, false);
        m_tabs->setCurrentIndex(0);
    }

    if (m_agentTitleEdit != nullptr) m_agentTitleEdit->clear();
    if (m_agentDescriptionEdit != nullptr) m_agentDescriptionEdit->clear();
    if (m_agentTimeoutEdit != nullptr) m_agentTimeoutEdit->clear();
    if (m_agentTemplateEdit != nullptr) m_agentTemplateEdit->clear();
    if (m_agentUrlEdit != nullptr) m_agentUrlEdit->clear();
    if (m_agentModelEdit != nullptr) m_agentModelEdit->clear();
    if (m_agentApiKeyEdit != nullptr) m_agentApiKeyEdit->clear();
    if (m_agentMaxRetriesEdit != nullptr) m_agentMaxRetriesEdit->clear();
    if (m_agentBackgroundPromptEdit != nullptr) m_agentBackgroundPromptEdit->clear();
    if (m_agentTaskPromptEdit != nullptr) m_agentTaskPromptEdit->clear();
}

void NodeInspector::buildUi()
{
    auto* title = new QLabel(tr("Inspector"), this);
    title->setObjectName("panelTitle");

    m_pythonEditor = new PythonCodeEditor(this);
    m_pythonEditor->setReadOnly(true);
    m_pythonEditor->setProperty("readOnly", true);
    m_pythonEditor->setPlaceholderText(tr("Python code here"));
    auto pythonPalette = m_pythonEditor->palette();
    pythonPalette.setColor(QPalette::PlaceholderText, QColor("#94A3B8"));
    m_pythonEditor->setPalette(pythonPalette);

    auto* agentPanel = new QWidget(this);
    agentPanel->setObjectName(QStringLiteral("agentInspectorPanel"));
    auto* agentLayout = new QFormLayout(agentPanel);
    agentLayout->setContentsMargins(8, 10, 8, 8);
    agentLayout->setHorizontalSpacing(10);
    agentLayout->setVerticalSpacing(8);
    agentLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_agentTitleEdit = new QLineEdit(agentPanel);
    m_agentDescriptionEdit = new QLineEdit(agentPanel);
    m_agentTimeoutEdit = new QLineEdit(agentPanel);
    m_agentTemplateEdit = new QLineEdit(agentPanel);
    m_agentUrlEdit = new QLineEdit(agentPanel);
    m_agentModelEdit = new QLineEdit(agentPanel);
    m_agentApiKeyEdit = new QLineEdit(agentPanel);
    m_agentApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_agentMaxRetriesEdit = new QLineEdit(agentPanel);
    m_agentBackgroundPromptEdit = new QPlainTextEdit(agentPanel);
    m_agentBackgroundPromptEdit->setFixedHeight(72);
    m_agentTaskPromptEdit = new QPlainTextEdit(agentPanel);
    m_agentTaskPromptEdit->setFixedHeight(72);

    setReadOnly(m_agentTitleEdit);
    setReadOnly(m_agentDescriptionEdit);
    setReadOnly(m_agentTimeoutEdit);
    setReadOnly(m_agentTemplateEdit);
    setReadOnly(m_agentUrlEdit);
    setReadOnly(m_agentModelEdit);
    setReadOnly(m_agentApiKeyEdit);
    setReadOnly(m_agentMaxRetriesEdit);
    setReadOnly(m_agentBackgroundPromptEdit);
    setReadOnly(m_agentTaskPromptEdit);

    agentLayout->addRow(tr("Title"), m_agentTitleEdit);
    agentLayout->addRow(tr("Description"), m_agentDescriptionEdit);
    agentLayout->addRow(tr("Timeout (ms)"), m_agentTimeoutEdit);
    agentLayout->addRow(tr("Template"), m_agentTemplateEdit);
    agentLayout->addRow(tr("URL"), m_agentUrlEdit);
    agentLayout->addRow(tr("Model name"), m_agentModelEdit);
    agentLayout->addRow(tr("API key"), m_agentApiKeyEdit);
    agentLayout->addRow(tr("Max request retries"), m_agentMaxRetriesEdit);
    agentLayout->addRow(tr("Background prompt"), m_agentBackgroundPromptEdit);
    agentLayout->addRow(tr("Task goal prompt"), m_agentTaskPromptEdit);

    m_outputJsonEditor = new PythonCodeEditor(this);
    m_outputJsonEditor->setObjectName(QStringLiteral("inspectorOutputJsonView"));
    m_outputJsonEditor->setReadOnly(true);
    m_outputJsonEditor->setProperty("readOnly", true);
    m_outputJsonEditor->setPlaceholderText(tr("JSON output here"));
    auto outputPalette = m_outputJsonEditor->palette();
    outputPalette.setColor(QPalette::PlaceholderText, QColor("#94A3B8"));
    m_outputJsonEditor->setPalette(outputPalette);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("inspectorTabs"));
    m_pythonEditor->setObjectName(QStringLiteral("inspectorCodePreview"));
    m_tabs->addTab(m_pythonEditor, tr("Python"));
    m_tabs->addTab(agentPanel, tr("Agent"));
    m_tabs->addTab(m_outputJsonEditor, tr("Output JSON"));
    m_tabs->setTabEnabled(1, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(title);
    layout->addWidget(m_tabs, 1);
}

void NodeInspector::setReadOnly(QLineEdit* edit)
{
    if (edit == nullptr) {
        return;
    }
    edit->setReadOnly(true);
    edit->setProperty("readOnly", true);
    edit->setFocusPolicy(Qt::NoFocus);
}

void NodeInspector::setReadOnly(QPlainTextEdit* edit)
{
    if (edit == nullptr) {
        return;
    }
    edit->setReadOnly(true);
    edit->setProperty("readOnly", true);
    edit->setFocusPolicy(Qt::NoFocus);
}

} // namespace vws::ui
