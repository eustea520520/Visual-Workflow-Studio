#pragma once

#include "application/generation/WorkflowGenerationTypes.h"

namespace vws::application {

class WorkflowNodeImplementationValidator final {
public:
    bool validateJsonText(
        const QString& jsonText,
        const WorkflowSkeletonNode& node,
        const NodeTemplateFullSpec& spec,
        NodeImplementation& implementation,
        QStringList& errors) const;

    bool validate(
        const NodeImplementation& implementation,
        const WorkflowSkeletonNode& node,
        const NodeTemplateFullSpec& spec,
        QStringList& errors) const;

private:
    QString extractJsonObjectText(const QString& text) const;
};

} // namespace vws::application
