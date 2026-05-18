#pragma once

#include "application/generation/WorkflowGenerationTypes.h"
#include "domain/Workflow.h"
#include "infrastructure/llm/LlmChatTypes.h"

#include <QObject>
#include <QStringList>

#include <functional>

namespace vws::application {
class WorkflowGenerationPromptBuilder;
class WorkflowGenerationService;
}

namespace vws::infrastructure {
class LlmChatClient;
}

namespace vws::presentation {

class AppStore;

struct WorkflowGenerationUiResult {
    bool success = false;
    domain::Workflow workflow;
    QStringList warnings;
    QString errorMessage;
    QString rawJson;
    QString sessionId;
};

class WorkflowGenerationController final : public QObject {
    Q_OBJECT

public:
    WorkflowGenerationController(
        application::WorkflowGenerationPromptBuilder& promptBuilder,
        application::WorkflowGenerationService& generationService,
        infrastructure::LlmChatClient& llmClient,
        AppStore& store,
        QObject* parent = nullptr);

    QString presetPrompt() const;
    QString presetPromptForCopy(const QString& requirementPlaceholder) const;

    void generateWorkflow(
        const infrastructure::LlmProviderSettings& provider,
        const QString& requirement,
        QObject* receiver,
        std::function<void(QString)> onProgress,
        std::function<void(WorkflowGenerationUiResult)> callback);

    bool importGeneratedJson(const QString& jsonText, WorkflowGenerationUiResult& result);
    QString copyableSkeletonPrompt(const QString& requirement) const;

private:
    bool validateProvider(
        const infrastructure::LlmProviderSettings& provider,
        const QString& requirement,
        QString* errorMessage) const;
    QJsonObject upstreamContracts(
        const application::WorkflowSkeleton& skeleton,
        const application::WorkflowSkeletonNode& node) const;
    void generateSkeletonAttempt(
        const infrastructure::LlmProviderSettings& provider,
        const QString& sessionId,
        QObject* receiver,
        std::function<void(QString)> onProgress,
        std::function<void(WorkflowGenerationUiResult)> callback,
        int attempt,
        const QStringList& previousErrors);
    void generateNodeAtIndex(
        const infrastructure::LlmProviderSettings& provider,
        const QString& sessionId,
        QObject* receiver,
        std::function<void(QString)> onProgress,
        std::function<void(WorkflowGenerationUiResult)> callback,
        int nodeIndex);
    void generateNodeAttempt(
        const infrastructure::LlmProviderSettings& provider,
        const QString& sessionId,
        QObject* receiver,
        std::function<void(QString)> onProgress,
        std::function<void(WorkflowGenerationUiResult)> callback,
        int nodeIndex,
        int attempt,
        const QStringList& previousErrors);
    void assembleAndFinish(
        const QString& sessionId,
        std::function<void(QString)> onProgress,
        std::function<void(WorkflowGenerationUiResult)> callback);

    application::WorkflowGenerationPromptBuilder& m_promptBuilder;
    application::WorkflowGenerationService& m_generationService;
    infrastructure::LlmChatClient& m_llmClient;
    AppStore& m_store;
    QHash<QString, application::WorkflowGenerationSession> m_sessions;
};

} // namespace vws::presentation
