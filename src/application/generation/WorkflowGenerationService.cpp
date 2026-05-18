#include "application/generation/WorkflowGenerationService.h"

#include "application/WorkflowService.h"
#include "application/generation/WorkflowGenerationAssembler.h"
#include "application/generation/WorkflowGenerationNormalizer.h"
#include "application/generation/WorkflowGenerationTemplateCatalog.h"
#include "application/generation/WorkflowGenerationValidator.h"
#include "application/generation/WorkflowNodeImplementationValidator.h"
#include "application/generation/WorkflowSkeletonValidator.h"

namespace vws::application {

WorkflowGenerationService::WorkflowGenerationService(
    WorkflowService& workflowService,
    WorkflowGenerationValidator& validator,
    WorkflowGenerationNormalizer& normalizer,
    WorkflowGenerationTemplateCatalog& templateCatalog,
    WorkflowSkeletonValidator& skeletonValidator,
    WorkflowNodeImplementationValidator& nodeImplementationValidator,
    WorkflowGenerationAssembler& assembler)
    : m_workflowService(workflowService)
    , m_validator(validator)
    , m_normalizer(normalizer)
    , m_templateCatalog(templateCatalog)
    , m_skeletonValidator(skeletonValidator)
    , m_nodeImplementationValidator(nodeImplementationValidator)
    , m_assembler(assembler)
{
}

bool WorkflowGenerationService::importGeneratedJsonToWorkspace(
    const QString& jsonText,
    const domain::Workspace& workspace,
    domain::Workflow& savedWorkflow,
    QStringList& warnings,
    QString* errorMessage)
{
    if (workspace.rootPath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No workspace is open.");
        }
        return false;
    }

    const auto rawValidation = m_validator.validateJsonText(jsonText);
    if (!rawValidation.valid) {
        if (errorMessage != nullptr) {
            *errorMessage = rawValidation.errors.join("\n");
        }
        return false;
    }

    auto workflow = m_normalizer.normalize(rawValidation.workflow, workspace);
    const auto normalizedValidation = m_validator.validateWorkflow(workflow);
    if (!normalizedValidation.valid) {
        if (errorMessage != nullptr) {
            *errorMessage = normalizedValidation.errors.join("\n");
        }
        return false;
    }

    if (!m_workflowService.saveWorkflowToWorkspace(workspace.rootPath, workflow, errorMessage)) {
        return false;
    }

    warnings = rawValidation.warnings + normalizedValidation.warnings;
    savedWorkflow = workflow;
    return true;
}

const WorkflowGenerationTemplateCatalog& WorkflowGenerationService::templateCatalog() const
{
    return m_templateCatalog;
}

bool WorkflowGenerationService::validateSkeletonJson(const QString& jsonText, WorkflowSkeleton& skeleton, QStringList& errors) const
{
    return m_skeletonValidator.validateJsonText(jsonText, m_templateCatalog, skeleton, errors);
}

bool WorkflowGenerationService::validateNodeImplementationJson(
    const QString& jsonText,
    const WorkflowSkeletonNode& node,
    const NodeTemplateFullSpec& spec,
    NodeImplementation& implementation,
    QStringList& errors) const
{
    return m_nodeImplementationValidator.validateJsonText(jsonText, node, spec, implementation, errors);
}

bool WorkflowGenerationService::assembleAndSave(
    const WorkflowSkeleton& skeleton,
    const QHash<QString, NodeImplementation>& implementationsByNodeId,
    const domain::Workspace& workspace,
    domain::Workflow& savedWorkflow,
    QStringList& warnings,
    QString* errorMessage)
{
    if (workspace.rootPath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No workspace is open.");
        }
        return false;
    }

    QStringList assemblyErrors;
    domain::Workflow workflow;
    if (!m_assembler.assemble(skeleton, implementationsByNodeId, m_templateCatalog, workspace, workflow, assemblyErrors)) {
        if (errorMessage != nullptr) {
            *errorMessage = assemblyErrors.join("\n");
        }
        return false;
    }

    const auto validation = m_validator.validateWorkflow(workflow);
    if (!validation.valid) {
        if (errorMessage != nullptr) {
            *errorMessage = validation.errors.join("\n");
        }
        return false;
    }

    if (!m_workflowService.saveWorkflowToWorkspace(workspace.rootPath, workflow, errorMessage)) {
        return false;
    }

    warnings = validation.warnings;
    savedWorkflow = workflow;
    return true;
}

} // namespace vws::application
