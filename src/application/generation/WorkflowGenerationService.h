#pragma once

#include "application/generation/WorkflowGenerationTypes.h"
#include "domain/Workspace.h"

namespace vws::application {

class WorkflowGenerationNormalizer;
class WorkflowGenerationValidator;
class WorkflowService;
class WorkflowGenerationAssembler;
class WorkflowGenerationTemplateCatalog;
class WorkflowNodeImplementationValidator;
class WorkflowSkeletonValidator;

class WorkflowGenerationService final {
public:
    WorkflowGenerationService(
        WorkflowService& workflowService,
        WorkflowGenerationValidator& validator,
        WorkflowGenerationNormalizer& normalizer,
        WorkflowGenerationTemplateCatalog& templateCatalog,
        WorkflowSkeletonValidator& skeletonValidator,
        WorkflowNodeImplementationValidator& nodeImplementationValidator,
        WorkflowGenerationAssembler& assembler);

    bool importGeneratedJsonToWorkspace(
        const QString& jsonText,
        const domain::Workspace& workspace,
        domain::Workflow& savedWorkflow,
        QStringList& warnings,
        QString* errorMessage = nullptr);

    const WorkflowGenerationTemplateCatalog& templateCatalog() const;
    bool validateSkeletonJson(const QString& jsonText, WorkflowSkeleton& skeleton, QStringList& errors) const;
    bool validateNodeImplementationJson(
        const QString& jsonText,
        const WorkflowSkeletonNode& node,
        const NodeTemplateFullSpec& spec,
        NodeImplementation& implementation,
        QStringList& errors) const;
    bool assembleAndSave(
        const WorkflowSkeleton& skeleton,
        const QHash<QString, NodeImplementation>& implementationsByNodeId,
        const domain::Workspace& workspace,
        domain::Workflow& savedWorkflow,
        QStringList& warnings,
        QString* errorMessage = nullptr);

private:
    WorkflowService& m_workflowService;
    WorkflowGenerationValidator& m_validator;
    WorkflowGenerationNormalizer& m_normalizer;
    WorkflowGenerationTemplateCatalog& m_templateCatalog;
    WorkflowSkeletonValidator& m_skeletonValidator;
    WorkflowNodeImplementationValidator& m_nodeImplementationValidator;
    WorkflowGenerationAssembler& m_assembler;
};

} // namespace vws::application
