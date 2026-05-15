#pragma once

#include <QHash>
#include <QString>

namespace vws::ui {

struct OutputPanelViewModel {
    QString workflowName;
    QHash<QString, QString> nodeNamesById;
};

} // namespace vws::ui
