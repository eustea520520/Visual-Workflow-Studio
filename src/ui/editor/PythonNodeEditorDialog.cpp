#include "ui/editor/PythonNodeEditorDialog.h"

#include "ui/editor/PythonCodeEditor.h"
#include "application/PythonCodeTemplates.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeConfigView.h"
#include "domain/NodeTypes.h"
#include "ui/theme/StyleReloader.h"

#include <QCloseEvent>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>

namespace vws::ui {

using application::PythonCodeTemplates;
namespace ConfigKeys = domain::NodeConfigKeys;
namespace NodeTypes = domain::NodeTypes;

PythonNodeEditorDialog::PythonNodeEditorDialog(
    const QString& nodeName,
    const QString& nodeDescription,
    int timeoutMs,
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
    m_timeoutEdit->setText(QString::number(timeoutMs));
    m_editor->setCode(initialCode.trimmed().isEmpty() ? defaultCode : initialCode);
    m_editor->setReadOnly(false);
    setDirty(false);

    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_descriptionEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
    connect(m_timeoutEdit, &QLineEdit::textChanged, this, [this]() { setDirty(true); });
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
    const bool isAgent = m_nodeType == NodeTypes::Agent;

    setWindowTitle(tr("Python Node Editor - %1").arg(nodeName));
    setObjectName(QStringLiteral("pythonNodeEditorDialog"));
    if (isAgent) {
        resize(1280, 760);
        setMinimumSize(1100, 680);
    } else {
        resize(920, 680);
    }

    auto* title = new QLabel(tr("Python Node: %1").arg(nodeName), this);
    title->setObjectName("dialogTitle");

    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setObjectName(QStringLiteral("saveNodeButton"));
    m_saveButton->setProperty("buttonRole", "primary");
    auto* header = new QHBoxLayout();
    header->addWidget(title, 1);
    header->addWidget(m_saveButton);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("Node title"));

    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("Describe what this node does"));
    m_descriptionEdit->setMinimumHeight(28);
    m_descriptionEdit->setMaximumHeight(36);

    m_timeoutEdit = new QLineEdit(this);
    m_timeoutEdit->setPlaceholderText(tr("300000"));
    m_timeoutEdit->setValidator(new QIntValidator(1, 86400000, m_timeoutEdit));

    auto* metadataForm = new QFormLayout();
    metadataForm->addRow(tr("Title"), m_titleEdit);
    metadataForm->addRow(tr("Description"), m_descriptionEdit);
    metadataForm->addRow(tr("Timeout (ms)"), m_timeoutEdit);

    m_editor = new PythonCodeEditor(this);
    m_editor->setPlaceholderText(PythonCodeTemplates::defaultFunctionCode());

    m_statusLabel = new QLabel(tr("Line 1, Column 1"), this);
    m_dirtyLabel = new QLabel(this);
    m_dirtyLabel->setObjectName(QStringLiteral("dirtyStateLabel"));
    auto* footer = new QHBoxLayout();
    footer->addWidget(m_statusLabel);
    footer->addStretch(1);
    footer->addWidget(m_dirtyLabel);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(header);

    if (isAgent) {
        const domain::NodeConfigView config(m_nodeConfig);
        // Create agent-specific widgets inline so the single QFormLayout owns all widths.
        m_agentUrlEdit = new QLineEdit(this);
        m_agentUrlEdit->setPlaceholderText(PythonCodeTemplates::agentUrlPlaceholder());
        m_agentUrlEdit->setText(config.agentUrl());

        m_agentModelEdit = new QLineEdit(this);
        m_agentModelEdit->setPlaceholderText(PythonCodeTemplates::agentModelPlaceholder());
        m_agentModelEdit->setText(config.agentModel());

        m_agentApiKeyEdit = new QLineEdit(this);
        m_agentApiKeyEdit->setEchoMode(QLineEdit::Password);
        m_agentApiKeyEdit->setPlaceholderText(PythonCodeTemplates::agentApiKeyPlaceholder());
        m_agentApiKeyEdit->setText(config.agentApiKey());

        m_agentMaxRetriesEdit = new QLineEdit(this);
        m_agentMaxRetriesEdit->setPlaceholderText(QString::number(PythonCodeTemplates::defaultAgentMaxRetries()));
        m_agentMaxRetriesEdit->setText(QString::number(
            config.agentMaxRetries(PythonCodeTemplates::defaultAgentMaxRetries())));
        m_agentMaxRetriesEdit->setValidator(new QIntValidator(1, 100, m_agentMaxRetriesEdit));

        m_agentBackgroundPromptEdit = new QPlainTextEdit(this);
        m_agentBackgroundPromptEdit->setMinimumHeight(110);
        m_agentBackgroundPromptEdit->setPlainText(config.agentBackgroundPrompt(
            PythonCodeTemplates::defaultAgentBackgroundPrompt()));

        m_agentTaskPromptEdit = new QPlainTextEdit(this);
        m_agentTaskPromptEdit->setMinimumHeight(150);
        m_agentTaskPromptEdit->setPlainText(config.agentTaskPrompt(
            PythonCodeTemplates::defaultAgentTaskPrompt()));

        m_loadAgentTemplateButton = new QPushButton(tr("Load Agent Template"), this);
        m_loadAgentTemplateButton->setProperty("buttonRole", "secondary");
        connect(m_loadAgentTemplateButton, &QPushButton::clicked, this, &PythonNodeEditorDialog::loadAgentTemplate);

        connect(m_agentUrlEdit, &QLineEdit::textChanged, this, [this]() { markAgentTemplateNeedsRefresh(); });
        connect(m_agentModelEdit, &QLineEdit::textChanged, this, [this]() { markAgentTemplateNeedsRefresh(); });
        connect(m_agentApiKeyEdit, &QLineEdit::textChanged, this, [this]() { markAgentTemplateNeedsRefresh(); });
        connect(m_agentMaxRetriesEdit, &QLineEdit::textChanged, this, [this]() { markAgentTemplateNeedsRefresh(); });
        connect(m_agentBackgroundPromptEdit, &QPlainTextEdit::textChanged, this, [this]() { markAgentTemplateNeedsRefresh(); });
        connect(m_agentTaskPromptEdit, &QPlainTextEdit::textChanged, this, [this]() { markAgentTemplateNeedsRefresh(); });

        auto* leftPanel = new QWidget(this);
        leftPanel->setObjectName(QStringLiteral("agentEditorLeftPanel"));
        leftPanel->setAutoFillBackground(false);
        auto* leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(0);

        auto* leftForm = new QFormLayout();
        leftForm->setContentsMargins(0, 0, 0, 0);
        leftForm->setHorizontalSpacing(10);
        leftForm->setVerticalSpacing(8);
        leftForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        leftForm->addRow(tr("Title"), m_titleEdit);
        leftForm->addRow(tr("Description"), m_descriptionEdit);
        leftForm->addRow(tr("Timeout (ms)"), m_timeoutEdit);
        leftForm->addRow(tr("URL"), m_agentUrlEdit);
        leftForm->addRow(tr("Model name"), m_agentModelEdit);
        leftForm->addRow(tr("API key"), m_agentApiKeyEdit);
        leftForm->addRow(tr("Max request retries"), m_agentMaxRetriesEdit);
        leftLayout->addLayout(leftForm);

        auto* bgLabel = new QLabel(tr("Background prompt"), leftPanel);
        leftLayout->addSpacing(8);
        leftLayout->addWidget(bgLabel);
        leftLayout->addWidget(m_agentBackgroundPromptEdit);

        auto* taskLabel = new QLabel(tr("Task goal prompt"), leftPanel);
        leftLayout->addSpacing(8);
        leftLayout->addWidget(taskLabel);
        leftLayout->addWidget(m_agentTaskPromptEdit);

        leftLayout->addSpacing(8);
        leftLayout->addWidget(m_loadAgentTemplateButton);
        leftLayout->addStretch(1);

        auto* leftScrollArea = new QScrollArea(this);
        leftScrollArea->setObjectName(QStringLiteral("agentEditorLeftScrollArea"));
        leftScrollArea->setWidgetResizable(true);
        leftScrollArea->setFrameShape(QFrame::NoFrame);
        leftScrollArea->setAutoFillBackground(false);
        leftScrollArea->setWidget(leftPanel);

        auto* rightPanel = new QWidget(this);
        auto* rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->addWidget(m_editor, 1);
        m_editor->setMinimumWidth(620);

        auto* bodySplitter = new QSplitter(Qt::Horizontal, this);
        bodySplitter->setObjectName(QStringLiteral("agentEditorSplitter"));
        bodySplitter->addWidget(leftScrollArea);
        bodySplitter->addWidget(rightPanel);
        bodySplitter->setStretchFactor(0, 0);
        bodySplitter->setStretchFactor(1, 1);
        bodySplitter->setSizes({390, 850});

        layout->addWidget(bodySplitter, 1);
    } else {
        layout->addLayout(metadataForm);
        layout->addWidget(m_editor, 1);
    }

    layout->addLayout(footer);
}

void PythonNodeEditorDialog::buildAgentSettings(QVBoxLayout* layout)
{
    Q_UNUSED(layout);
    // Widget creation moved inline into buildUi() for uniform form layout widths.
}

void PythonNodeEditorDialog::loadAgentTemplate()
{
    if (m_nodeType != NodeTypes::Agent) {
        return;
    }

    applyAgentTemplateToEditor();
    setDirty(true);
}

QJsonObject PythonNodeEditorDialog::agentConfigPatch() const
{
    if (m_nodeType != NodeTypes::Agent) {
        return {};
    }

    return {
        {ConfigKeys::AgentUrl, agentUrl()},
        {ConfigKeys::AgentModel, agentModel()},
        {ConfigKeys::AgentApiKey, agentApiKey()},
        {ConfigKeys::AgentMaxRetries, agentMaxRetries()},
        {ConfigKeys::AgentBackgroundPrompt, agentBackgroundPrompt()},
        {ConfigKeys::AgentTaskPrompt, agentTaskPrompt()},
        {ConfigKeys::IoTemplate, PythonCodeTemplates::templateKey(agentTransferTemplate())},
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
        domain::NodeConfigView(m_nodeConfig).ioTemplate(),
        DataTransferTemplate::DataToData);
}

int PythonNodeEditorDialog::timeoutMs() const
{
    bool ok = false;
    const int value = m_timeoutEdit != nullptr
        ? m_timeoutEdit->text().trimmed().toInt(&ok)
        : 300000;

    return ok ? value : 300000;
}

bool PythonNodeEditorDialog::validateTimeout(QString* errorMessage) const
{
    if (m_timeoutEdit == nullptr) {
        return true;
    }

    const auto text = m_timeoutEdit->text().trimmed();
    if (text.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Timeout is required.");
        }
        return false;
    }

    bool ok = false;
    const int value = text.toInt(&ok);

    if (!ok) {
        if (errorMessage) {
            *errorMessage = tr("Timeout must be an integer number of milliseconds.");
        }
        return false;
    }

    if (value <= 0) {
        if (errorMessage) {
            *errorMessage = tr("Timeout must be greater than 0 ms.");
        }
        return false;
    }

    if (value > 86400000) {
        if (errorMessage) {
            *errorMessage = tr("Timeout must not exceed 86400000 ms.");
        }
        return false;
    }

    return true;
}

int PythonNodeEditorDialog::agentMaxRetries() const
{
    if (m_agentMaxRetriesEdit == nullptr) {
        return PythonCodeTemplates::defaultAgentMaxRetries();
    }

    bool ok = false;
    const int value = m_agentMaxRetriesEdit->text().trimmed().toInt(&ok);
    if (!ok || value < 1) {
        return PythonCodeTemplates::defaultAgentMaxRetries();
    }

    return value;
}

bool PythonNodeEditorDialog::validateAgentMaxRetries(QString* errorMessage) const
{
    if (m_nodeType != "agent") {
        return true;
    }

    if (m_agentMaxRetriesEdit == nullptr) {
        return true;
    }

    const auto text = m_agentMaxRetriesEdit->text().trimmed();
    if (text.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Max request retries is required.");
        }
        return false;
    }

    bool ok = false;
    const int value = text.toInt(&ok);

    if (!ok) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Max request retries must be an integer.");
        }
        return false;
    }

    if (value < 1) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Max request retries must be at least 1.");
        }
        return false;
    }

    if (value > 100) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Max request retries must not exceed 100.");
        }
        return false;
    }

    return true;
}

void PythonNodeEditorDialog::save()
{
    if (m_saveInProgress) {
        return;
    }

    QString errorMessage;
    if (!validateTimeout(&errorMessage)) {
        QMessageBox::warning(this, tr("Invalid Timeout"), errorMessage);
        return;
    }

    if (!validateAgentMaxRetries(&errorMessage)) {
        QMessageBox::warning(this, tr("Invalid Agent Retries"), errorMessage);
        return;
    }

    // Requirement: when Agent settings change, saving should behave like
    // clicking "Load Agent Template" first, so the generated Python code
    // always contains the latest Agent settings.
    if (m_nodeType == "agent" && m_agentTemplateNeedsRefresh) {
        applyAgentTemplateToEditor();
    }

    m_saveInProgress = true;
    if (m_saveButton != nullptr) {
        m_saveButton->setEnabled(false);
    }
    emit nodeSaved(nodeName(), nodeDescription(), timeoutMs(), m_editor->code(), agentConfigPatch());
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
    m_dirtyLabel->setProperty("dirty", dirty);
    StyleReloader::refresh(m_dirtyLabel);
    m_saveButton->setEnabled(dirty);
    m_saveButton->setToolTip(dirty
        ? tr("Save changes")
        : tr("No unsaved changes"));
}

void PythonNodeEditorDialog::updateCursorStatus(int line, int column)
{
    m_statusLabel->setText(tr("Line %1, Column %2").arg(line).arg(column));
}

void PythonNodeEditorDialog::markAgentTemplateNeedsRefresh()
{
    if (m_nodeType != "agent") {
        return;
    }

    m_agentTemplateNeedsRefresh = true;
    setDirty(true);
}

void PythonNodeEditorDialog::applyAgentTemplateToEditor()
{
    if (m_nodeType != "agent" || m_editor == nullptr) {
        return;
    }

    m_editor->setCode(PythonCodeTemplates::agentCode(
        agentUrl(),
        agentModel(),
        agentApiKey(),
        agentMaxRetries(),
        agentBackgroundPrompt(),
        agentTaskPrompt(),
        agentTransferTemplate()));

    m_agentTemplateNeedsRefresh = false;
}

} // namespace vws::ui
