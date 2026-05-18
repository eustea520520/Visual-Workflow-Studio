#pragma once

#include "domain/Node.h"
#include "domain/NodeIoSpec.h"
#include "domain/Workflow.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace vws::application {

struct NodeTemplateDescriptor {
    QString templateId;
    QString type;
    QString displayName;
    QString description;
    QString ioKind;
    QStringList inputPorts;
    QStringList outputPorts;
    QString usageHint;

    QJsonObject toJson() const;
};

struct NodeTemplateFullSpec {
    QString templateId;
    QString type;
    QString displayName;
    QString description;
    QString ioKind;
    QStringList inputPorts;
    QStringList outputPorts;
    QJsonObject defaultConfig;
    domain::NodeIoSpec defaultIoSpec;
    domain::NodeRuntime defaultRuntime;
    QString codeTemplate;
    QString programmingInstructions;

    QJsonObject toJson() const;
};

struct WorkflowSkeletonNode {
    QString nodeId;
    QString templateId;
    QString type;
    QString name;
    QString purpose;
    QString inputContract;
    QString outputContract;
    int expectedInputDimension = 1;
    int expectedOutputDimension = 1;
    QStringList inputItems;
    QStringList outputItems;
    QStringList dependsOnNodeIds;
    int layer = 0;
    int row = 0;

    QJsonObject toJson() const;
    static WorkflowSkeletonNode fromJson(const QJsonObject& object);
};

struct WorkflowSkeletonEdge {
    QString edgeId;
    QString fromNode;
    QString fromPort;
    int fromSlot = -1;
    QString toNode;
    QString toPort;
    int toSlot = -1;

    QJsonObject toJson() const;
    static WorkflowSkeletonEdge fromJson(const QJsonObject& object);
};

struct WorkflowSkeleton {
    QString name;
    QString description;
    QList<WorkflowSkeletonNode> nodes;
    QList<WorkflowSkeletonEdge> edges;

    QJsonObject toJson() const;
    static WorkflowSkeleton fromJson(const QJsonObject& object);
};

struct NodeImplementation {
    QString nodeId;
    QString code;
    QJsonObject configPatch;
    domain::NodeIoSpec ioSpecPatch;
    int timeoutMs = 300000;
    QString notes;

    QJsonObject toJson() const;
    static NodeImplementation fromJson(const QJsonObject& object);
};

enum class WorkflowGenerationStage {
    Idle,
    PlanningSkeleton,
    ValidatingSkeleton,
    GeneratingNode,
    ValidatingNode,
    AssemblingWorkflow,
    SavingWorkflow,
    Finished,
    Failed,
    Cancelled,
};

struct WorkflowGenerationSession {
    QString sessionId;
    QString userRequirement;
    QList<NodeTemplateDescriptor> availableTemplates;
    WorkflowSkeleton skeleton;
    QHash<QString, NodeImplementation> implementationsByNodeId;
    QStringList warnings;
    QStringList errors;
    WorkflowGenerationStage stage = WorkflowGenerationStage::Idle;
    int currentNodeIndex = -1;
};

struct WorkflowGenerationValidationResult {
    bool valid = true;
    domain::Workflow workflow;
    QJsonObject json;
    QStringList errors;
    QStringList warnings;

    void addError(const QString& error)
    {
        valid = false;
        errors.append(error);
    }

    void addWarning(const QString& warning)
    {
        warnings.append(warning);
    }
};

struct WorkflowGenerationImportResult {
    bool success = false;
    domain::Workflow workflow;
    QStringList warnings;
    QString errorMessage;
};

} // namespace vws::application
