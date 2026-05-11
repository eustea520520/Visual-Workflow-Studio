#include "ui/inspector/NodeInspector.h"

#include "ui/editor/PythonCodeEditor.h"
#include "ui/editor/PythonCodeTemplates.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vws::ui {

NodeInspector::NodeInspector(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void NodeInspector::displayNode(const domain::Node& node)
{
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
        return;
    }

    if (m_tabs != nullptr) {
        m_tabs->setTabEnabled(1, true);
        m_tabs->setCurrentIndex(1);
    }
    m_agentTitleEdit->setText(node.name);
    m_agentDescriptionEdit->setText(node.description);
    m_agentTimeoutEdit->setText(QString::number(node.runtime.timeoutMs));
    m_agentTemplateEdit->setText(node.config.value("io_template").toString("data_to_data"));
    m_agentUrlEdit->setText(node.config.value("agent_url").toString(PythonCodeTemplates::defaultAgentUrl()));
    m_agentModelEdit->setText(node.config.value("agent_model").toString(PythonCodeTemplates::defaultAgentModel()));
    m_agentApiKeyEdit->setText(node.config.value("agent_api_key").toString());
    m_agentBackgroundPromptEdit->setPlainText(node.config.value("agent_background_prompt").toString(
        PythonCodeTemplates::defaultAgentBackgroundPrompt()));
    m_agentTaskPromptEdit->setPlainText(node.config.value("agent_task_prompt").toString(
        PythonCodeTemplates::defaultAgentTaskPrompt()));
}

void NodeInspector::buildUi()
{
    auto* title = new QLabel(tr("Inspector"), this);
    title->setObjectName("panelTitle");

    m_pythonEditor = new PythonCodeEditor(this);
    m_pythonEditor->setReadOnly(true);
    m_pythonEditor->setPlaceholderText(PythonCodeTemplates::defaultFunctionCode());

    auto* agentPanel = new QWidget(this);
    auto* agentLayout = new QFormLayout(agentPanel);

    m_agentTitleEdit = new QLineEdit(agentPanel);
    m_agentDescriptionEdit = new QLineEdit(agentPanel);
    m_agentTimeoutEdit = new QLineEdit(agentPanel);
    m_agentTemplateEdit = new QLineEdit(agentPanel);
    m_agentUrlEdit = new QLineEdit(agentPanel);
    m_agentModelEdit = new QLineEdit(agentPanel);
    m_agentApiKeyEdit = new QLineEdit(agentPanel);
    m_agentApiKeyEdit->setEchoMode(QLineEdit::Password);
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
    setReadOnly(m_agentBackgroundPromptEdit);
    setReadOnly(m_agentTaskPromptEdit);

    agentLayout->addRow(tr("Title"), m_agentTitleEdit);
    agentLayout->addRow(tr("Description"), m_agentDescriptionEdit);
    agentLayout->addRow(tr("Timeout (ms)"), m_agentTimeoutEdit);
    agentLayout->addRow(tr("Template"), m_agentTemplateEdit);
    agentLayout->addRow(tr("URL"), m_agentUrlEdit);
    agentLayout->addRow(tr("Model name"), m_agentModelEdit);
    agentLayout->addRow(tr("API key"), m_agentApiKeyEdit);
    agentLayout->addRow(tr("Background prompt"), m_agentBackgroundPromptEdit);
    agentLayout->addRow(tr("Task goal prompt"), m_agentTaskPromptEdit);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_pythonEditor, tr("Python"));
    m_tabs->addTab(agentPanel, tr("Agent"));
    m_tabs->setTabEnabled(1, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->addWidget(title);
    layout->addWidget(m_tabs, 1);
}

void NodeInspector::setReadOnly(QLineEdit* edit)
{
    if (edit == nullptr) {
        return;
    }
    edit->setReadOnly(true);
    edit->setFocusPolicy(Qt::NoFocus);
}

void NodeInspector::setReadOnly(QPlainTextEdit* edit)
{
    if (edit == nullptr) {
        return;
    }
    edit->setReadOnly(true);
    edit->setFocusPolicy(Qt::NoFocus);
}

} // namespace vws::ui
