#include "presentation/controllers/WorkflowGenerationController.h"

#include "application/generation/WorkflowGenerationPromptBuilder.h"
#include "application/generation/WorkflowGenerationService.h"
#include "application/generation/WorkflowGenerationTemplateCatalog.h"
#include "domain/NodeTypes.h"
#include "infrastructure/llm/LlmChatClient.h"
#include "presentation/state/AppStore.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPointer>
#include <QUuid>

namespace vws::presentation {

namespace {
constexpr int MaxSkeletonRepairAttempts = 2;
constexpr int MaxNodeRepairAttempts = 2;
}

WorkflowGenerationController::WorkflowGenerationController(
    application::WorkflowGenerationPromptBuilder& promptBuilder,
    application::WorkflowGenerationService& generationService,
    infrastructure::LlmChatClient& llmClient,
    AppStore& store,
    QObject* parent)
    : QObject(parent)
    , m_promptBuilder(promptBuilder)
    , m_generationService(generationService)
    , m_llmClient(llmClient)
    , m_store(store)
{
}

QString WorkflowGenerationController::presetPrompt() const
{
    return m_promptBuilder.skeletonSystemPrompt();
}

QString WorkflowGenerationController::presetPromptForCopy(const QString& requirementPlaceholder) const
{
    return copyableSkeletonPrompt(requirementPlaceholder);
}

QString WorkflowGenerationController::copyableSkeletonPrompt(const QString& requirement) const
{
    return m_promptBuilder.buildCopyableSkeletonPrompt(
        requirement,
        m_generationService.templateCatalog().descriptors());
}

void WorkflowGenerationController::generateWorkflow(
    const infrastructure::LlmProviderSettings& provider,
    const QString& requirement,
    QObject* receiver,
    std::function<void(QString)> onProgress,
    std::function<void(WorkflowGenerationUiResult)> callback)
{
    WorkflowGenerationUiResult earlyResult;
    if (!validateProvider(provider, requirement, &earlyResult.errorMessage)) {
        callback(earlyResult);
        return;
    }

    application::WorkflowGenerationSession session;
    session.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.userRequirement = requirement;
    session.availableTemplates = m_generationService.templateCatalog().descriptors();
    session.stage = application::WorkflowGenerationStage::PlanningSkeleton;
    m_sessions.insert(session.sessionId, session);

    generateSkeletonAttempt(provider, session.sessionId, receiver, std::move(onProgress), std::move(callback), 0, {});
}

bool WorkflowGenerationController::importGeneratedJson(const QString& jsonText, WorkflowGenerationUiResult& result)
{
    result.rawJson = jsonText;
    domain::Workflow savedWorkflow;
    QStringList warnings;
    QString errorMessage;
    if (!m_generationService.importGeneratedJsonToWorkspace(
            jsonText,
            m_store.currentWorkspace(),
            savedWorkflow,
            warnings,
            &errorMessage)) {
        result.success = false;
        result.errorMessage = errorMessage;
        return false;
    }

    m_store.setCurrentWorkflow(savedWorkflow);
    m_store.resetForWorkflowChange();
    result.success = true;
    result.workflow = savedWorkflow;
    result.warnings = warnings;
    return true;
}

bool WorkflowGenerationController::validateProvider(
    const infrastructure::LlmProviderSettings& provider,
    const QString& requirement,
    QString* errorMessage) const
{
    if (m_store.currentWorkspace().rootPath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No workspace is open.");
        }
        return false;
    }
    if (provider.url.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("LLM URL is empty.");
        }
        return false;
    }
    if (provider.modelName.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Model name is empty.");
        }
        return false;
    }
    if (provider.apiKey.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("API key is empty.");
        }
        return false;
    }
    if (requirement.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Requirement is empty.");
        }
        return false;
    }
    return true;
}

QJsonObject WorkflowGenerationController::upstreamContracts(
    const application::WorkflowSkeleton& skeleton,
    const application::WorkflowSkeletonNode& node) const
{
    QJsonObject contracts;
    for (const auto& upstreamId : node.dependsOnNodeIds) {
        for (const auto& upstreamNode : skeleton.nodes) {
            if (upstreamNode.nodeId == upstreamId) {
                contracts.insert(upstreamId, QJsonObject{
                    {"output_contract", upstreamNode.outputContract},
                    {"expected_output_dimension", upstreamNode.expectedOutputDimension},
                    {"output_items", QJsonArray::fromStringList(upstreamNode.outputItems)},
                });
                break;
            }
        }
    }
    return contracts;
}

void WorkflowGenerationController::generateSkeletonAttempt(
    const infrastructure::LlmProviderSettings& provider,
    const QString& sessionId,
    QObject* receiver,
    std::function<void(QString)> onProgress,
    std::function<void(WorkflowGenerationUiResult)> callback,
    int attempt,
    const QStringList& previousErrors)
{
    auto session = m_sessions.value(sessionId);
    session.stage = application::WorkflowGenerationStage::PlanningSkeleton;
    m_sessions.insert(sessionId, session);
    onProgress(attempt == 0
            ? QStringLiteral("Planning workflow skeleton...")
            : QStringLiteral("Repairing workflow skeleton..."));

    infrastructure::LlmChatRequest request;
    request.url = provider.url;
    request.modelName = provider.modelName;
    request.apiKey = provider.apiKey;
    request.timeoutMs = provider.timeoutMs;
    request.systemPrompt = m_promptBuilder.skeletonSystemPrompt();
    request.userPrompt = m_promptBuilder.buildSkeletonUserPrompt(
        session.userRequirement,
        session.availableTemplates,
        previousErrors);

    QPointer<QObject> guardedReceiver(receiver);
    m_llmClient.sendChatCompletion(request, receiver,
        [this, guardedReceiver, provider, sessionId, onProgress = std::move(onProgress), callback = std::move(callback), attempt]
        (infrastructure::LlmChatResponse response) mutable {
            if (guardedReceiver.isNull()) {
                return;
            }
            if (!response.success) {
                callback({false, {}, {}, response.errorMessage, response.content, sessionId});
                return;
            }

            onProgress(QStringLiteral("Validating skeleton..."));
            application::WorkflowSkeleton skeleton;
            QStringList errors;
            if (!m_generationService.validateSkeletonJson(response.content, skeleton, errors)) {
                if (attempt < MaxSkeletonRepairAttempts) {
                    generateSkeletonAttempt(provider, sessionId, guardedReceiver, std::move(onProgress), std::move(callback), attempt + 1, errors);
                    return;
                }
                callback({false, {}, {}, QStringLiteral("Generated skeleton is invalid:\n%1").arg(errors.join("\n")), response.content, sessionId});
                return;
            }

            auto session = m_sessions.value(sessionId);
            session.skeleton = skeleton;
            session.stage = application::WorkflowGenerationStage::GeneratingNode;
            m_sessions.insert(sessionId, session);
            generateNodeAtIndex(provider, sessionId, guardedReceiver, std::move(onProgress), std::move(callback), 0);
        });
}

void WorkflowGenerationController::generateNodeAtIndex(
    const infrastructure::LlmProviderSettings& provider,
    const QString& sessionId,
    QObject* receiver,
    std::function<void(QString)> onProgress,
    std::function<void(WorkflowGenerationUiResult)> callback,
    int nodeIndex)
{
    const auto session = m_sessions.value(sessionId);
    if (nodeIndex >= session.skeleton.nodes.size()) {
        assembleAndFinish(sessionId, std::move(onProgress), std::move(callback));
        return;
    }

    generateNodeAttempt(provider, sessionId, receiver, std::move(onProgress), std::move(callback), nodeIndex, 0, {});
}

void WorkflowGenerationController::generateNodeAttempt(
    const infrastructure::LlmProviderSettings& provider,
    const QString& sessionId,
    QObject* receiver,
    std::function<void(QString)> onProgress,
    std::function<void(WorkflowGenerationUiResult)> callback,
    int nodeIndex,
    int attempt,
    const QStringList& previousErrors)
{
    auto session = m_sessions.value(sessionId);
    session.stage = application::WorkflowGenerationStage::GeneratingNode;
    session.currentNodeIndex = nodeIndex;
    m_sessions.insert(sessionId, session);

    const auto node = session.skeleton.nodes.at(nodeIndex);
    const auto spec = m_generationService.templateCatalog().fullSpec(node.templateId);
    if (!spec.has_value()) {
        callback({false, {}, {}, QStringLiteral("Missing template for node %1: %2").arg(node.nodeId, node.templateId), {}, sessionId});
        return;
    }
    if (spec->type == domain::NodeTypes::Subsystem) {
        session.implementationsByNodeId.insert(node.nodeId, application::NodeImplementation{});
        m_sessions.insert(sessionId, session);
        onProgress(QStringLiteral("Skipping code generation for non-Python node: %1").arg(node.name));
        generateNodeAtIndex(provider, sessionId, receiver, std::move(onProgress), std::move(callback), nodeIndex + 1);
        return;
    }

    onProgress(QStringLiteral("Generating node %1/%2: %3")
        .arg(nodeIndex + 1)
        .arg(session.skeleton.nodes.size())
        .arg(node.name));

    infrastructure::LlmChatRequest request;
    request.url = provider.url;
    request.modelName = provider.modelName;
    request.apiKey = provider.apiKey;
    request.timeoutMs = provider.timeoutMs;
    request.systemPrompt = m_promptBuilder.nodeImplementationSystemPrompt();
    request.userPrompt = m_promptBuilder.buildNodeImplementationUserPrompt(
        session.userRequirement,
        session.skeleton,
        node,
        upstreamContracts(session.skeleton, node),
        spec.value(),
        previousErrors);

    QPointer<QObject> guardedReceiver(receiver);
    m_llmClient.sendChatCompletion(request, receiver,
        [this, guardedReceiver, provider, sessionId, onProgress = std::move(onProgress), callback = std::move(callback), nodeIndex, attempt, node, spec]
        (infrastructure::LlmChatResponse response) mutable {
            if (guardedReceiver.isNull()) {
                return;
            }
            if (!response.success) {
                callback({false, {}, {}, response.errorMessage, response.content, sessionId});
                return;
            }

            onProgress(QStringLiteral("Validating node %1...").arg(node.name));
            application::NodeImplementation implementation;
            QStringList errors;
            if (!m_generationService.validateNodeImplementationJson(response.content, node, spec.value(), implementation, errors)) {
                if (attempt < MaxNodeRepairAttempts) {
                    generateNodeAttempt(provider, sessionId, guardedReceiver, std::move(onProgress), std::move(callback), nodeIndex, attempt + 1, errors);
                    return;
                }
                callback({false, {}, {}, QStringLiteral("Generated implementation for node %1 is invalid:\n%2").arg(node.nodeId, errors.join("\n")), response.content, sessionId});
                return;
            }

            auto session = m_sessions.value(sessionId);
            session.implementationsByNodeId.insert(node.nodeId, implementation);
            m_sessions.insert(sessionId, session);
            generateNodeAtIndex(provider, sessionId, guardedReceiver, std::move(onProgress), std::move(callback), nodeIndex + 1);
        });
}

void WorkflowGenerationController::assembleAndFinish(
    const QString& sessionId,
    std::function<void(QString)> onProgress,
    std::function<void(WorkflowGenerationUiResult)> callback)
{
    auto session = m_sessions.value(sessionId);
    onProgress(QStringLiteral("Assembling workflow..."));

    domain::Workflow savedWorkflow;
    QStringList warnings;
    QString errorMessage;
    if (!m_generationService.assembleAndSave(
            session.skeleton,
            session.implementationsByNodeId,
            m_store.currentWorkspace(),
            savedWorkflow,
            warnings,
            &errorMessage)) {
        callback({false, {}, warnings, errorMessage, {}, sessionId});
        return;
    }

    m_store.setCurrentWorkflow(savedWorkflow);
    m_store.resetForWorkflowChange();
    onProgress(QStringLiteral("Done"));
    callback({true, savedWorkflow, warnings, {}, QString::fromUtf8(QJsonDocument(savedWorkflow.toJson()).toJson(QJsonDocument::Indented)), sessionId});
}

} // namespace vws::presentation
