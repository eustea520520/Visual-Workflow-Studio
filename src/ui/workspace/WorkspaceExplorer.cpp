#include "ui/workspace/WorkspaceExplorer.h"

#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QtAlgorithms>

namespace vws::ui {

WorkspaceExplorer::WorkspaceExplorer(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("workspaceExplorer"));
    setProperty("panel", true);
    buildUi();
}

void WorkspaceExplorer::setWorkspaceData(
    const QString& workspaceName,
    const QStringList& workflowNames,
    const QStringList& workflowIds,
    const QStringList& templateNames,
    const QStringList& runNames)
{
    m_workspaceLabel->setText(workspaceName.trimmed().isEmpty()
            ? tr("Workspace Explorer")
            : tr("Workspace: %1").arg(workspaceName));

    rebuildWorkflowCategory(workflowNames, workflowIds);
    rebuildCategory(m_templatesItem, templateNames, tr("No node templates"));
    rebuildCategory(m_runsItem, runNames, tr("No run history"));
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

    m_workflowsItem = new QTreeWidgetItem(m_tree, {tr("Workflows")});
    m_templatesItem = new QTreeWidgetItem(m_tree, {tr("Node Templates")});
    m_runsItem = new QTreeWidgetItem(m_tree, {tr("Runs")});

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item == nullptr || item->parent() != m_workflowsItem) {
            return;
        }

        const auto workflowId = item->data(0, Qt::UserRole).toString();
        if (!workflowId.isEmpty()) {
            emit workflowActivated(workflowId);
        }
    });

    setWorkspaceData({}, {}, {}, {tr("Function Node"), tr("Agent Node")}, {});

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(m_workspaceLabel);
    layout->addWidget(m_tree, 1);
}

void WorkspaceExplorer::rebuildCategory(QTreeWidgetItem* rootItem, const QStringList& names, const QString& emptyText)
{
    // QTreeWidgetItem::takeChildren 只会把子项从树上摘下来，不会替我们释放内存。
    // 工作区内容刷新会频繁调用这个函数，所以这里主动删除旧子项，避免长期运行时泄漏。
    qDeleteAll(rootItem->takeChildren());

    if (names.isEmpty()) {
        rootItem->addChild(new QTreeWidgetItem({emptyText}));
        return;
    }

    for (const auto& name : names) {
        rootItem->addChild(new QTreeWidgetItem({name}));
    }
}

void WorkspaceExplorer::rebuildWorkflowCategory(const QStringList& names, const QStringList& workflowIds)
{
    qDeleteAll(m_workflowsItem->takeChildren());

    if (names.isEmpty()) {
        m_workflowsItem->addChild(new QTreeWidgetItem({tr("No workflow loaded")}));
        return;
    }

    for (int index = 0; index < names.size(); ++index) {
        auto* item = new QTreeWidgetItem({names.at(index)});
        if (index < workflowIds.size()) {
            item->setData(0, Qt::UserRole, workflowIds.at(index));
        }
        m_workflowsItem->addChild(item);
    }
}

} // namespace vws::ui
