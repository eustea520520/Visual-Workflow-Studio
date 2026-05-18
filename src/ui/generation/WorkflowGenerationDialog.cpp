#include "ui/generation/WorkflowGenerationDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace vws::ui {

WorkflowGenerationDialog::WorkflowGenerationDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Generate Workflow by LLM"));
    setMinimumSize(880, 720);
    buildUi();
}

void WorkflowGenerationDialog::render(const WorkflowGenerationDialogViewModel& viewModel)
{
    m_urlEdit->setText(viewModel.url);
    m_modelEdit->setText(viewModel.modelName);
    m_presetPromptEdit->setPlainText(viewModel.presetPrompt);
    m_requirementEdit->setPlainText(viewModel.userRequirement);
    m_generatedJsonEdit->setPlainText(viewModel.generatedJson);
    setStatusMessage(viewModel.statusMessage);
    setLoading(viewModel.loading);
}

void WorkflowGenerationDialog::setGeneratedJson(const QString& jsonText)
{
    m_generatedJsonEdit->setPlainText(jsonText);
}

void WorkflowGenerationDialog::setStatusMessage(const QString& message, bool error)
{
    m_statusLabel->setText(message);
    m_statusLabel->setProperty("error", error);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

void WorkflowGenerationDialog::setLoading(bool loading)
{
    m_generateButton->setEnabled(!loading);
    m_importClipboardButton->setEnabled(!loading);
    m_copyPromptButton->setEnabled(!loading);
}

QString WorkflowGenerationDialog::userRequirement() const
{
    return m_requirementEdit->toPlainText();
}

QString WorkflowGenerationDialog::generatedJson() const
{
    return m_generatedJsonEdit->toPlainText();
}

infrastructure::LlmProviderSettings WorkflowGenerationDialog::providerSettings() const
{
    infrastructure::LlmProviderSettings settings;
    settings.url = m_urlEdit->text();
    settings.modelName = m_modelEdit->text();
    settings.apiKey = m_apiKeyEdit->text();
    return settings;
}

void WorkflowGenerationDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    auto* providerGroup = new QGroupBox(tr("Provider Settings"), this);
    auto* providerLayout = new QFormLayout(providerGroup);
    m_urlEdit = new QLineEdit(providerGroup);
    m_urlEdit->setObjectName("llmUrlEdit");
    m_urlEdit->setPlaceholderText(tr("Example: https://api.openai.com/v1 or full /chat/completions URL"));
    m_modelEdit = new QLineEdit(providerGroup);
    m_modelEdit->setObjectName("llmModelEdit");
    m_modelEdit->setPlaceholderText(tr("model name"));
    m_apiKeyEdit = new QLineEdit(providerGroup);
    m_apiKeyEdit->setObjectName("llmApiKeyEdit");
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    providerLayout->addRow(tr("LLM URL"), m_urlEdit);
    providerLayout->addRow(tr("Model name"), m_modelEdit);
    providerLayout->addRow(tr("API key"), m_apiKeyEdit);
    root->addWidget(providerGroup);

    auto* promptGroup = new QGroupBox(tr("Preset Prompt Preview"), this);
    auto* promptLayout = new QVBoxLayout(promptGroup);
    m_presetPromptEdit = new QPlainTextEdit(promptGroup);
    m_presetPromptEdit->setObjectName("presetPromptPreview");
    m_presetPromptEdit->setReadOnly(true);
    m_presetPromptEdit->setMaximumBlockCount(1000);
    promptLayout->addWidget(m_presetPromptEdit);
    m_copyPromptButton = new QPushButton(tr("Copy Skeleton Prompt"), promptGroup);
    promptLayout->addWidget(m_copyPromptButton, 0, Qt::AlignRight);
    root->addWidget(promptGroup, 1);

    auto* requirementGroup = new QGroupBox(tr("User Requirement"), this);
    auto* requirementLayout = new QVBoxLayout(requirementGroup);
    m_requirementEdit = new QPlainTextEdit(requirementGroup);
    m_requirementEdit->setObjectName("userRequirementEdit");
    requirementLayout->addWidget(m_requirementEdit);
    root->addWidget(requirementGroup, 1);

    auto* jsonGroup = new QGroupBox(tr("Generated JSON Preview"), this);
    auto* jsonLayout = new QVBoxLayout(jsonGroup);
    m_generatedJsonEdit = new QPlainTextEdit(jsonGroup);
    m_generatedJsonEdit->setObjectName("generatedJsonPreview");
    jsonLayout->addWidget(m_generatedJsonEdit);
    root->addWidget(jsonGroup, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("generationStatus");
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    m_generateButton = new QPushButton(tr("Generate"), this);
    m_generateButton->setObjectName("generateWorkflowButton");
    m_importClipboardButton = new QPushButton(tr("Import JSON from Clipboard"), this);
    m_importClipboardButton->setObjectName("importGeneratedJsonButton");
    m_closeButton = new QPushButton(tr("Close"), this);
    buttons->addWidget(m_generateButton);
    buttons->addWidget(m_importClipboardButton);
    buttons->addWidget(m_closeButton);
    root->addLayout(buttons);

    connect(m_copyPromptButton, &QPushButton::clicked, this, [this]() {
        emit copyPromptRequested(userRequirement());
    });
    connect(m_generateButton, &QPushButton::clicked, this, [this]() {
        emit generateRequested(providerSettings(), userRequirement());
    });
    connect(m_importClipboardButton, &QPushButton::clicked, this, [this]() {
        const auto text = QApplication::clipboard()->text();
        m_generatedJsonEdit->setPlainText(text);
        emit importJsonRequested(text);
    });
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);
}

} // namespace vws::ui
