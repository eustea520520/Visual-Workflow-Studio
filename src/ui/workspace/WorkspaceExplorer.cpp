#include "ui/workspace/WorkspaceExplorer.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDrag>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QtAlgorithms>

namespace vws::ui {

namespace {

constexpr const char* NodeTemplateMimeType = "application/x-vws-node-template-id";

} // namespace

WorkspaceExplorer::WorkspaceExplorer(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("workspaceExplorer"));
    setProperty("panel", true);
    buildUi();
}

void WorkspaceExplorer::render(const WorkspaceExplorerViewModel& viewModel)
{
    m_workspaceLabel->setText(viewModel.workspaceName.trimmed().isEmpty()
            ? tr("Workspace Explorer")
            : tr("Workspace: %1").arg(viewModel.workspaceName));

    rebuildWorkflowCategory(viewModel.workflows);
    rebuildTemplateCategory(viewModel.templates);
    rebuildRunCategory(viewModel.runs);
    m_tree->expandAll();
}

void WorkspaceExplorer::buildUi()
{
    m_workspaceLabel = new QLabel(tr("Workspace Explorer"), this);
    m_workspaceLabel->setObjectName("panelTitle");

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("workspaceTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    m_tree->setAnimated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setDragEnabled(false);
    m_tree->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_tree->viewport()->installEventFilter(this);

    m_workflowsItem = new QTreeWidgetItem(m_tree, {tr("Workflows")});
    m_templatesItem = new QTreeWidgetItem(m_tree, {tr("Node Templates")});
    m_runsItem = new QTreeWidgetItem(m_tree, {tr("Runs")});

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item == nullptr) {
            return;
        }

        if (item->parent() == m_workflowsItem) {
            const auto workflowId = item->data(0, Qt::UserRole).toString();
            if (!workflowId.isEmpty()) {
                emit workflowActivated(workflowId);
            }
            return;
        }

        if (item->parent() == m_runsItem) {
            const auto runId = item->data(0, Qt::UserRole).toString();
            if (!runId.isEmpty()) {
                emit runActivated(runId);
            }
            return;
        }
    });

    WorkspaceExplorerViewModel initialViewModel;
    initialViewModel.templates = {
        WorkspaceExplorerItemViewModel{QString(), tr("Function Node"), false},
        WorkspaceExplorerItemViewModel{QString(), tr("Agent Node"), false},
    };
    render(initialViewModel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(m_workspaceLabel);
    layout->addWidget(m_tree, 1);
}

void WorkspaceExplorer::rebuildCategory(QTreeWidgetItem* rootItem, const QStringList& names, const QString& emptyText)
{
    qDeleteAll(rootItem->takeChildren());

    if (names.isEmpty()) {
        rootItem->addChild(new QTreeWidgetItem({emptyText}));
        return;
    }

    for (const auto& name : names) {
        rootItem->addChild(new QTreeWidgetItem({name}));
    }
}

void WorkspaceExplorer::rebuildWorkflowCategory(const QList<WorkspaceExplorerItemViewModel>& workflows)
{
    qDeleteAll(m_workflowsItem->takeChildren());

    if (workflows.isEmpty()) {
        m_workflowsItem->addChild(new QTreeWidgetItem({tr("No workflow loaded")}));
        return;
    }

    for (const auto& workflow : workflows) {
        const QString displayName = workflow.running
            ? QStringLiteral("* %1").arg(workflow.name)
            : workflow.name;

        auto* item = new QTreeWidgetItem({displayName});
        if (!workflow.id.isEmpty()) {
            item->setData(0, Qt::UserRole, workflow.id);
        }
        m_workflowsItem->addChild(item);
    }
}

void WorkspaceExplorer::rebuildTemplateCategory(const QList<WorkspaceExplorerItemViewModel>& templates)
{
    qDeleteAll(m_templatesItem->takeChildren());

    if (templates.isEmpty()) {
        m_templatesItem->addChild(new QTreeWidgetItem({tr("No node templates")}));
        return;
    }

    for (const auto& nodeTemplate : templates) {
        auto* item = new QTreeWidgetItem({nodeTemplate.name});
        if (!nodeTemplate.id.isEmpty()) {
            item->setData(0, Qt::UserRole, nodeTemplate.id);
        }
        m_templatesItem->addChild(item);
    }
}

void WorkspaceExplorer::rebuildRunCategory(const QList<WorkspaceExplorerItemViewModel>& runs)
{
    qDeleteAll(m_runsItem->takeChildren());

    if (runs.isEmpty()) {
        m_runsItem->addChild(new QTreeWidgetItem({tr("No run history")}));
        return;
    }

    for (const auto& run : runs) {
        auto* item = new QTreeWidgetItem({run.name});
        if (!run.id.isEmpty()) {
            item->setData(0, Qt::UserRole, run.id);
        }
        m_runsItem->addChild(item);
    }
}

bool WorkspaceExplorer::eventFilter(QObject* watched, QEvent* event)
{
    if (m_tree == nullptr || watched != m_tree->viewport()) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            auto* item = m_tree->itemAt(mouseEvent->pos());
            if (item != nullptr && item->parent() == m_workflowsItem) {
                const auto workflowId = item->data(0, Qt::UserRole).toString();
                if (!workflowId.trimmed().isEmpty()) {
                    m_tree->setCurrentItem(item);
                    QMenu menu(this);
                    auto* deleteAction = menu.addAction(tr("Delete Workflow"));
                    const auto* selectedAction = menu.exec(m_tree->viewport()->mapToGlobal(mouseEvent->pos()));
                    if (selectedAction == deleteAction) {
                        emit workflowDeleteRequested(workflowId, item->text(0).remove(QStringLiteral("* ")));
                    }
                    return true;
                }
            }
        }

        if (mouseEvent->button() == Qt::LeftButton) {
            m_dragStartPosition = mouseEvent->pos();
        }
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        if (!(mouseEvent->buttons() & Qt::LeftButton)) {
            return QWidget::eventFilter(watched, event);
        }

        if ((mouseEvent->pos() - m_dragStartPosition).manhattanLength()
            < QApplication::startDragDistance()) {
            return QWidget::eventFilter(watched, event);
        }

        auto* item = m_tree->itemAt(m_dragStartPosition);
        if (item == nullptr || item->parent() != m_templatesItem) {
            return QWidget::eventFilter(watched, event);
        }

        const auto templateId = item->data(0, Qt::UserRole).toString();
        if (templateId.trimmed().isEmpty()) {
            return QWidget::eventFilter(watched, event);
        }

        auto* mimeData = new QMimeData();
        mimeData->setData(NodeTemplateMimeType, templateId.toUtf8());

        auto* drag = new QDrag(m_tree);
        drag->setMimeData(mimeData);
        drag->exec(Qt::CopyAction);

        return true;
    }

    return QWidget::eventFilter(watched, event);
}

} // namespace vws::ui
