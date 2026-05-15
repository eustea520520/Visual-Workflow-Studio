#pragma once

#include "application/NodeFactory.h"
#include "application/PythonCodeTemplates.h"

#include <QPoint>

class QWidget;

namespace vws::ui {

struct WorkflowCanvasContextAction {
    enum class Type {
        None,
        AddStarter,
        AddFunction,
        AddAgent,
        ConnectSelected,
        DeleteSelected,
    };

    Type type = Type::None;
    application::NodeFactory::StarterTemplateKind starterTemplate =
        application::NodeFactory::StarterTemplateKind::DataOutput;
    application::DataTransferTemplate dataTransferTemplate =
        application::DataTransferTemplate::DataToData;
};

// Builds the canvas context menu and translates QAction selection into a small value object.
class WorkflowCanvasContextMenu final {
public:
    static WorkflowCanvasContextAction exec(
        QWidget* parent,
        const QPoint& globalPos,
        bool canConnectSelected,
        bool hasSelection);
};

} // namespace vws::ui
