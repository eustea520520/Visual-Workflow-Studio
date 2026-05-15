#include "ui/canvas/WorkflowCanvasContextMenu.h"

#include <QAction>
#include <QMenu>

namespace vws::ui {

WorkflowCanvasContextAction WorkflowCanvasContextMenu::exec(
    QWidget* parent,
    const QPoint& globalPos,
    bool canConnectSelected,
    bool hasSelection)
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
    menu.addSeparator();

    auto* connectAction = menu.addAction(QObject::tr("Connect Selected Nodes"));
    connectAction->setEnabled(canConnectSelected);

    auto* deleteAction = menu.addAction(QObject::tr("Delete Selected"));
    deleteAction->setEnabled(hasSelection);

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
    } else if (selectedAction == connectAction) {
        action.type = WorkflowCanvasContextAction::Type::ConnectSelected;
    } else if (selectedAction == deleteAction) {
        action.type = WorkflowCanvasContextAction::Type::DeleteSelected;
    }

    return action;
}

} // namespace vws::ui
