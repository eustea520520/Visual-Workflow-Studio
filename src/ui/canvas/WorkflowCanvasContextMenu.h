#pragma once

#include <QPoint>

class QWidget;

namespace vws::ui {

enum class StarterNodeTemplate {
    EmptyOutput,
    DataOutput,
    FileOutput,
};

enum class DataTransferNodeTemplate {
    DataToData,
    DataToFile,
    FileToData,
    FileToFile,
};

struct WorkflowCanvasContextAction {
    enum class Type {
        None,
        AddStarter,
        AddFunction,
        AddAgent,
        ConnectSelected,
        DeleteSelected,
        RotateSelected,
    };

    Type type = Type::None;
    StarterNodeTemplate starterTemplate = StarterNodeTemplate::DataOutput;
    DataTransferNodeTemplate dataTransferTemplate = DataTransferNodeTemplate::DataToData;
    int rotationDeltaDegrees = 0;
};

// Builds the canvas context menu and translates QAction selection into a small value object.
class WorkflowCanvasContextMenu final {
public:
    static WorkflowCanvasContextAction exec(
        QWidget* parent,
        const QPoint& globalPos,
        bool canConnectSelected,
        bool hasSelection,
        bool canRotateSelected);
};

} // namespace vws::ui
