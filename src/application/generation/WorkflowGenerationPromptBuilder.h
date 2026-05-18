#pragma once

#include "application/generation/WorkflowGenerationTypes.h"

#include <QString>

namespace vws::application {

class WorkflowGenerationPromptBuilder final {
public:
    QString systemPrompt() const;
    QString buildUserPrompt(const QString& requirement) const;
    QString buildCopyablePrompt(const QString& requirementPlaceholder) const;

    QString skeletonSystemPrompt() const;
    QString buildSkeletonUserPrompt(
        const QString& requirement,
        const QList<NodeTemplateDescriptor>& descriptors,
        const QStringList& previousErrors = {}) const;
    QString buildCopyableSkeletonPrompt(
        const QString& requirement,
        const QList<NodeTemplateDescriptor>& descriptors) const;

    QString nodeImplementationSystemPrompt() const;
    QString buildNodeImplementationUserPrompt(
        const QString& requirement,
        const WorkflowSkeleton& skeleton,
        const WorkflowSkeletonNode& node,
        const QJsonObject& upstreamContracts,
        const NodeTemplateFullSpec& fullSpec,
        const QStringList& previousErrors = {}) const;
    QString buildCopyableNodePrompt(
        const QString& requirement,
        const WorkflowSkeleton& skeleton,
        const WorkflowSkeletonNode& node,
        const QJsonObject& upstreamContracts,
        const NodeTemplateFullSpec& fullSpec) const;
};

} // namespace vws::application
