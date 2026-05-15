#pragma once

#include <QList>
#include <QString>

namespace vws::ui {

struct WorkspaceExplorerItemViewModel {
    QString id;
    QString name;
    bool running = false;
};

struct WorkspaceExplorerViewModel {
    QString workspaceName;
    QList<WorkspaceExplorerItemViewModel> workflows;
    QList<WorkspaceExplorerItemViewModel> templates;
    QList<WorkspaceExplorerItemViewModel> runs;
};

} // namespace vws::ui
