#pragma once

#include "application/generation/WorkflowGenerationTypes.h"

namespace vws::application {

class WorkflowGenerationValidator final {
public:
    WorkflowGenerationValidationResult validateJsonText(const QString& jsonText) const;
    WorkflowGenerationValidationResult validateWorkflow(const domain::Workflow& workflow) const;

private:
    QString extractJsonObjectText(const QString& text) const;
    void validateNodes(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const;
    void validateEdges(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const;
    void validateGraph(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const;
    void validatePythonCode(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const;
    void validateSecrets(const QJsonObject& json, WorkflowGenerationValidationResult& result) const;
};

} // namespace vws::application
