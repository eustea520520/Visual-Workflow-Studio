#pragma once

#include "application/WorkflowDocument.h"
#include "domain/Workspace.h"

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>

namespace vws::presentation {

// AppState is the single in-memory owner of application-level state that is not
// owned by a widget. Widgets render this state; controllers mutate it.
struct AppState {
    domain::Workspace currentWorkspace;
    application::WorkflowDocument workflowDocument;
    bool workflowRunning = false;
    QHash<QString, QJsonObject> nodeOutputsByNodeId;
    QString selectedNodeId;
    QHash<QString, QHash<QString, QString>> nodeStatusesByWorkflowId;
    QHash<QString, QString> workflowIdByRunId;
    QSet<QString> runningWorkflowIds;
    QString activeRunWorkflowId;
};

} // namespace vws::presentation
