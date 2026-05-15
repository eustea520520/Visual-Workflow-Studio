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
    using StarterTemplateKind = application::NodeFactory::StarterTemplateKind;
    using DataTransferTemplate = application::DataTransferTemplate;

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
        action.starterTemplate = StarterTemplateKind::EmptyOutput;
    } else if (selectedAction == addStarterDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddStarter;
        action.starterTemplate = StarterTemplateKind::DataOutput;
    } else if (selectedAction == addStarterFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddStarter;
        action.starterTemplate = StarterTemplateKind::FileOutput;
    } else if (selectedAction == addFunctionDataToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferTemplate::DataToData;
    } else if (selectedAction == addFunctionDataToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferTemplate::DataToFile;
    } else if (selectedAction == addFunctionFileToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferTemplate::FileToData;
    } else if (selectedAction == addFunctionFileToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddFunction;
        action.dataTransferTemplate = DataTransferTemplate::FileToFile;
    } else if (selectedAction == addAgentDataToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferTemplate::DataToData;
    } else if (selectedAction == addAgentDataToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferTemplate::DataToFile;
    } else if (selectedAction == addAgentFileToDataAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferTemplate::FileToData;
    } else if (selectedAction == addAgentFileToFileAction) {
        action.type = WorkflowCanvasContextAction::Type::AddAgent;
        action.dataTransferTemplate = DataTransferTemplate::FileToFile;
    } else if (selectedAction == connectAction) {
        action.type = WorkflowCanvasContextAction::Type::ConnectSelected;
    } else if (selectedAction == deleteAction) {
        action.type = WorkflowCanvasContextAction::Type::DeleteSelected;
    }

    return action;
}

} // namespace vws::ui
