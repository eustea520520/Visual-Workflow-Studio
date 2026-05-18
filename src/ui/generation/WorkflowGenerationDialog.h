#pragma once

#include "infrastructure/llm/LlmChatTypes.h"
#include "ui/generation/WorkflowGenerationDialogViewModel.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;

namespace vws::ui {

class WorkflowGenerationDialog final : public QDialog {
    Q_OBJECT

public:
    explicit WorkflowGenerationDialog(QWidget* parent = nullptr);

    void render(const WorkflowGenerationDialogViewModel& viewModel);
    void setGeneratedJson(const QString& jsonText);
    void setStatusMessage(const QString& message, bool error = false);
    void setLoading(bool loading);

    QString userRequirement() const;
    QString generatedJson() const;

signals:
    void generateRequested(
        const vws::infrastructure::LlmProviderSettings& provider,
        const QString& requirement);
    void importJsonRequested(const QString& jsonText);
    void copyPromptRequested(const QString& requirement);

private:
    vws::infrastructure::LlmProviderSettings providerSettings() const;
    void buildUi();

    QLineEdit* m_urlEdit = nullptr;
    QLineEdit* m_modelEdit = nullptr;
    QLineEdit* m_apiKeyEdit = nullptr;
    QPlainTextEdit* m_presetPromptEdit = nullptr;
    QPlainTextEdit* m_requirementEdit = nullptr;
    QPlainTextEdit* m_generatedJsonEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_copyPromptButton = nullptr;
    QPushButton* m_generateButton = nullptr;
    QPushButton* m_importClipboardButton = nullptr;
    QPushButton* m_closeButton = nullptr;
};

} // namespace vws::ui
