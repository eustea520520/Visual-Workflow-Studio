#include "ui/canvas/WorkflowCanvasContextMenu.h"

#include <QAction>
#include <QMenu>

namespace vws::ui {

WorkflowCanvasContextAction WorkflowCanvasContextMenu::exec(
    QWidget* parent,
    const QPoint& globalPos,
    bool canConnectSelected,
    bool hasSelection,
    bool canRotateSelected,
    bool canRetitleSubsystem)
{
    QMenu menu(parent);
    auto* starterMenu = menu.addMenu(QObject::tr("Add Starter Node"));
    auto* addStarterEmptyAction = starterMenu->addAction(QObject::tr("Empty Output"));
    auto* addStarterDataAction = starterMenu->addAction(QObject::tr("Business Data Output"));
    auto* addStarterFileAction = starterMenu->addAction(QObject::tr("File Output"));

    auto* functionMenu = menu.addMenu(QObject::tr("Add Function Node"));
    auto* addFunctionDataToDataAction = functionMenu->addAction(QObject::tr("Data to Data"));
    auto* addFunctionDataToFileAction = functionMenu->addAction(QObject::tr("Data to File"));
    auto* addFunctionFileToDataAction = functionMenu->addAction(QObject::tr("File to Data"));
    auto* addFunctionFileToFileAction = functionMenu->addAction(QObject::tr("File to File"));

    auto* agentMenu = menu.addMenu(QObject::tr("Add Agent Node"));
    auto* addAgentDataToDataAction = agentMenu->addAction(QObject::tr("Data to Data"));
    auto* addAgentDataToFileAction = agentMenu->addAction(QObject::tr("Data to File"));
    auto* addAgentFileToDataAction = agentMenu->addAction(QObject::tr("File to Data"));
    auto* addAgentFileToFileAction = agentMenu->addAction(QObject::tr("File to File"));

    auto* addSubsystemAction = menu.addAction(QObject::tr("Add Subsystem Node"));
    auto* addLoopAction = menu.addAction(QObject::tr("Add Loop Node"));
    menu.addSeparator();

    auto* connectAction = menu.addAction(QObject::tr("Connect Selected Nodes"));
    connectAction->setEnabled(canConnectSelected);

    auto* deleteAction = menu.addAction(QObject::tr("Delete Selected"));
    deleteAction->setEnabled(hasSelection);

    auto* retitleSubsystemAction = menu.addAction(QObject::tr("Retitle Node"));
    retitleSubsystemAction->setEnabled(canRetitleSubsystem);

    auto* rotateMenu = menu.addMenu(QObject::tr("Rotate"));
    auto* rotateClockwise90Action = rotateMenu->addAction(QObject::tr("Clockwise 90°"));
    auto* rotateClockwise180Action = rotateMenu->addAction(QObject::tr("Clockwise 180°"));
    auto* rotateCounterclockwise90Action = rotateMenu->addAction(QObject::tr("Counterclockwise 90°"));
    auto* rotateCounterclockwise180Action = rotateMenu->addAction(QObject::tr("Counterclockwise 180°"));
    rotateMenu->setEnabled(canRotateSelected);

    const auto* selectedAction = menu.exec(globalPos);
    WorkflowCanvasContextAction action;
    if (selectedAction == nullptr) {
        return action;
    }

    if (selectedAction == addStarterEmptyAction) {
        action.type = WorkflowCanvasContextAction::Type::AddStarter;
        action.starterTemplate = StarterNodeTemplate::EmptyOutput;
    } else if (selectedAction == addStarterDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddStarter;
        action.starterTemplate = StarterNodeTemplate::DataOutput;
    } else if (selectedAction == addStarterFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddStarter;
        action.starterTemplate = StarterNodeTemplate::FileOutput;
    } else if (selectedAction == addFunctionDataToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferNodeTemplate::DataToData;
    } else if (selectedAction == addFunctionDataToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferNodeTemplate::DataToFile;
    } else if (selectedAction == addFunctionFileToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferNodeTemplate::FileToData;
    } else if (selectedAction == addFunctionFileToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferNodeTemplate::FileToFile;
    } else if (selectedAction == addAgentDataToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferNodeTemplate::DataToData;
    } else if (selectedAction == addAgentDataToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferNodeTemplate::DataToFile;
    } else if (selectedAction == addAgentFileToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferNodeTemplate::FileToData;
    } else if (selectedAction == addAgentFileToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferNodeTemplate::FileToFile;
    } else if (selectedAction == addSubsystemAction) {
        action.type = WorkflowCanvasContextAction::Type::AddSubsystem;
    } else if (selectedAction == addLoopAction) {
        action.type = WorkflowCanvasContextAction::Type::AddLoop;
    } else if (selectedAction == connectAction) {
        action.type = WorkflowCanvasContextAction::Type::ConnectSelected;
    } else if (selectedAction == deleteAction) {
        action.type = WorkflowCanvasContextAction::Type::DeleteSelected;
    } else if (selectedAction == retitleSubsystemAction) {
        action.type = WorkflowCanvasContextAction::Type::RetitleSubsystem;
    } else if (selectedAction == rotateClockwise90Action) {
        action.type = WorkflowCanvasContextAction::Type::RotateSelected;
        action.rotationDeltaDegrees = 90;
    } else if (selectedAction == rotateClockwise180Action) {
        action.type = WorkflowCanvasContextAction::Type::RotateSelected;
        action.rotationDeltaDegrees = 180;
    } else if (selectedAction == rotateCounterclockwise90Action) {
        action.type = WorkflowCanvasContextAction::Type::RotateSelected;
        action.rotationDeltaDegrees = -90;
    } else if (selectedAction == rotateCounterclockwise180Action) {
        action.type = WorkflowCanvasContextAction::Type::RotateSelected;
        action.rotationDeltaDegrees = -180;
    }

    return action;
}

} // namespace vws::ui
