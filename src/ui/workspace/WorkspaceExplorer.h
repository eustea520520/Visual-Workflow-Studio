#pragma once

#include <QSet>
#include <QStringList>
#include <QWidget>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace vws::ui {

// 左侧工作区资源树。
//
// 它只负责展示，不直接扫描磁盘，也不直接读数据库。
// application service 负责拿到工作流/模板/运行历史后，再调用 setWorkspaceData 刷新这里。
class WorkspaceExplorer final : public QWidget {
    Q_OBJECT

public:
    explicit WorkspaceExplorer(QWidget* parent = nullptr);

    void setWorkspaceData(
        const QString& workspaceName,
        const QStringList& workflowNames,
        const QStringList& workflowIds,
        const QStringList& templateNames,
        const QStringList& templateIds = {},
        const QStringList& runNames = {},
        const QStringList& runIds = {},
        const QSet<QString>& runningWorkflowIds = {});

signals:
    void workflowActivated(const QString& workflowId);
    void runActivated(const QString& runId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void rebuildCategory(QTreeWidgetItem* rootItem, const QStringList& names, const QString& emptyText);
    void rebuildWorkflowCategory(const QStringList& names, const QStringList& workflowIds, const QSet<QString>& runningWorkflowIds = {});
    void rebuildTemplateCategory(const QStringList& names, const QStringList& templateIds);
    void rebuildRunCategory(const QStringList& names, const QStringList& runIds);

    QLabel* m_workspaceLabel = nullptr;
    QTreeWidget* m_tree = nullptr;
    QTreeWidgetItem* m_workflowsItem = nullptr;
    QTreeWidgetItem* m_templatesItem = nullptr;
    QTreeWidgetItem* m_runsItem = nullptr;
    QPoint m_dragStartPosition;
};

} // namespace vws::ui
