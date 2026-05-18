#pragma once

#include "application/generation/WorkflowGenerationTypes.h"

namespace vws::application {

class WorkflowGenerationTemplateCatalog;

class WorkflowSkeletonValidator final {
public:
    bool validateJsonText(
        const QString& jsonText,
        const WorkflowGenerationTemplateCatalog& catalog,
        WorkflowSkeleton& skeleton,
        QStringList& errors) const;

    bool validate(
        const WorkflowSkeleton& skeleton,
        const WorkflowGenerationTemplateCatalog& catalog,
        QStringList& errors) const;

private:
    QString extractJsonObjectText(const QString& text) const;
};

} // namespace vws::application
