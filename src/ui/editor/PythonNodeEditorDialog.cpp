#include "ui/editor/PythonNodeEditorDialog.h"

#include "ui/editor/PythonCodeEditor.h"
#include "ui/editor/PythonCodeTemplates.h"

#include <QCloseEvent>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace vws::ui {

PythonNodeEditorDialog::PythonNodeEditorDialog(
    const QString& nodeName,
    const QString& nodeDescription,
    const QString& nodeType,
    const QJsonObject& nodeConfig,
    const QString& initialCode,
    const QString& defaultCode,
    QWidget* parent)
    : QDialog(parent)
    , m_nodeType(nodeType.trimmed().toLower())
    , m_nodeConfig(nodeConfig)
{
    buildUi(nodeName);
    m_titleEdit->setText(nodeName);
    m_descriptionEdit->setText(nodeDescription);
    m_editor->setCode(initialCode.trimmed().isEmpty() ? defaultCode : initialCode);
    m_editor->setReadOnly(m_nodeType == "agent");
    setDirty(false);

    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_descriptionEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_editor, &PythonCodeEditor::cursorPositionInfoChanged, this, &PythonNodeEditorDialog::updateCursorStatus);
    connect(m_saveButton, &QPushButton::clicked, this, &PythonNodeEditorDialog::save);

    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &PythonNodeEditorDialog::save);
}

QString PythonNodeEditorDialog::code() const
{
    return m_editor->code();
}

QString PythonNodeEditorDialog::nodeName() const
{
    return m_titleEdit != nullptr ? m_titleEdit->text().trimmed() : QString();
}

QString PythonNodeEditorDialog::nodeDescription() const
{
    return m_descriptionEdit != nullptr ? m_descriptionEdit->text() : QString();
}

void PythonNodeEditorDialog::closeEvent(QCloseEvent* event)
{
    if (confirmCloseIfDirty()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void PythonNodeEditorDialog::buildUi(const QString& nodeName)
{
    setWindowTitle(tr("Python Node Editor - %1").arg(nodeName));
    resize(920, 680);

    auto* title = new QLabel(tr("Python Node: %1").arg(nodeName), this);
    title->setObjectName("panelTitle");

    m_saveButton = new QPushButton(tr("Save"), this);
    auto* header = new QHBoxLayout();
    header->addWidget(title, 1);
    header->addWidget(m_saveButton);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("Node title"));

    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("Describe what this node does"));
    m_descriptionEdit->setFixedHeight(28);

    auto* metadataForm = new QFormLayout();
    metadataForm->addRow(tr("Title"), m_titleEdit);
    metadataForm->addRow(tr("Description"), m_descriptionEdit);

    m_editor = new PythonCodeEditor(this);
    m_editor->setPlaceholderText(PythonCodeTemplates::defaultFunctionCode());

    m_statusLabel = new QLabel(tr("Line 1, Column 1"), this);
    m_dirtyLabel = new QLabel(this);
    auto* footer = new QHBoxLayout();
    footer->addWidget(m_statusLabel);
    footer->addStretch(1);
    footer->addWidget(m_dirtyLabel);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addLayout(metadataForm);
    if (m_nodeType == "agent") {
        buildAgentSettings(layout);
    }
    layout->addWidget(m_editor, 1);
    layout->addLayout(footer);
}

void PythonNodeEditorDialog::buildAgentSettings(QVBoxLayout* layout)
{
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    m_agentUrlEdit = new QLineEdit(this);
    m_agentUrlEdit->setText(m_nodeConfig.value("agent_url").toString(PythonCodeTemplates::defaultAgentUrl()));

    m_agentModelEdit = new QLineEdit(this);
    m_agentModelEdit->setText(m_nodeConfig.value("agent_model").toString(PythonCodeTemplates::defaultAgentModel()));

    m_agentApiKeyEdit = new QLineEdit(this);
    m_agentApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_agentApiKeyEdit->setText(m_nodeConfig.value("agent_api_key").toString());

    m_agentBackgroundPromptEdit = new QPlainTextEdit(this);
    m_agentBackgroundPromptEdit->setFixedHeight(70);
    m_agentBackgroundPromptEdit->setPlainText(m_nodeConfig.value("agent_background_prompt").toString(
        PythonCodeTemplates::defaultAgentBackgroundPrompt()));

    m_agentTaskPromptEdit = new QPlainTextEdit(this);
    m_agentTaskPromptEdit->setFixedHeight(70);
    m_agentTaskPromptEdit->setPlainText(m_nodeConfig.value("agent_task_prompt").toString(
        PythonCodeTemplates::defaultAgentTaskPrompt()));

    m_loadAgentTemplateButton = new QPushButton(tr("Load Agent Template"), this);
    connect(m_loadAgentTemplateButton, &QPushButton::clicked, this, &PythonNodeEditorDialog::loadAgentTemplate);

    auto* agentForm = new QFormLayout();
    agentForm->addRow(tr("URL"), m_agentUrlEdit);
    agentForm->addRow(tr("Model name"), m_agentModelEdit);
    agentForm->addRow(tr("API key"), m_agentApiKeyEdit);
    agentForm->addRow(tr("Background prompt"), m_agentBackgroundPromptEdit);
    agentForm->addRow(tr("Task goal prompt"), m_agentTaskPromptEdit);
    agentForm->addRow(QString(), m_loadAgentTemplateButton);
    layout->addLayout(agentForm);

    connect(m_agentUrlEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_agentModelEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_agentApiKeyEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_agentBackgroundPromptEdit, &QPlainTextEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_agentTaskPromptEdit, &QPlainTextEdit::textChanged, this, [this]() { setDirty(true); });
}

void PythonNodeEditorDialog::loadAgentTemplate()
{
    if (m_nodeType != "agent") {
        return;
    }

    m_editor->setCode(PythonCodeTemplates::agentCode(
        agentUrl(),
        agentModel(),
        agentApiKey(),
        agentBackgroundPrompt(),
        agentTaskPrompt(),
        agentTransferTemplate()));
    setDirty(true);
}

QJsonObject PythonNodeEditorDialog::agentConfigPatch() const
{
    if (m_nodeType != "agent") {
        return {};
    }

    return {
        {"agent_url", agentUrl()},
        {"agent_model", agentModel()},
        {"agent_api_key", agentApiKey()},
        {"agent_background_prompt", agentBackgroundPrompt()},
        {"agent_task_prompt", agentTaskPrompt()},
        {"io_template", PythonCodeTemplates::templateKey(agentTransferTemplate())},
    };
}

QString PythonNodeEditorDialog::agentUrl() const
{
    return m_agentUrlEdit != nullptr ? m_agentUrlEdit->text().trimmed() : QString();
}

QString PythonNodeEditorDialog::agentModel() const
{
    return m_agentModelEdit != nullptr ? m_agentModelEdit->text().trimmed() : QString();
}

QString PythonNodeEditorDialog::agentApiKey() const
{
    return m_agentApiKeyEdit != nullptr ? m_agentApiKeyEdit->text().trimmed() : QString();
}

QString PythonNodeEditorDialog::agentBackgroundPrompt() const
{
    return m_agentBackgroundPromptEdit != nullptr ? m_agentBackgroundPromptEdit->toPlainText() : QString();
}

QString PythonNodeEditorDialog::agentTaskPrompt() const
{
    return m_agentTaskPromptEdit != nullptr ? m_agentTaskPromptEdit->toPlainText() : QString();
}

DataTransferTemplate PythonNodeEditorDialog::agentTransferTemplate() const
{
    return PythonCodeTemplates::transferTemplateFromKey(
        m_nodeConfig.value("io_template").toString(),
        DataTransferTemplate::DataToData);
}

void PythonNodeEditorDialog::save()
{
    if (m_saveInProgress) {
        return;
    }

    m_saveInProgress = true;
    if (m_saveButton != nullptr) {
        m_saveButton->setEnabled(false);
    }
    if (m_nodeType == "agent") {
        loadAgentTemplate();
    }
    emit nodeSaved(nodeName(), nodeDescription(), m_editor->code(), agentConfigPatch());
    setDirty(false);
    m_saveInProgress = false;
}

bool PythonNodeEditorDialog::confirmCloseIfDirty()
{
    if (!m_dirty) {
        return true;
    }

    const auto choice = QMessageBox::warning(
        this,
        tr("Unsaved Python Code"),
        tr("The Python code has unsaved changes."),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (choice == QMessageBox::Save) {
        save();
        return true;
    }
    if (choice == QMessageBox::Discard) {
        return true;
    }
    return false;
}

void PythonNodeEditorDialog::setDirty(bool dirty)
{
    m_dirty = dirty;
    m_dirtyLabel->setText(dirty ? tr("Unsaved") : tr("Saved"));
    m_saveButton->setEnabled(dirty);
}

void PythonNodeEditorDialog::updateCursorStatus(int line, int column)
{
    m_statusLabel->setText(tr("Line %1, Column %2").arg(line).arg(column));
}

} // namespace vws::ui
