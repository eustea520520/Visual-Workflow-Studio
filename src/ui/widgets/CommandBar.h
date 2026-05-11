#pragma once

#include <QFrame>

#include "ui/widgets/IconSquareButton.h"

class QAction;
class QHBoxLayout;
class QLabel;

namespace vws::ui {

class CommandBar : public QFrame {
    Q_OBJECT

public:
    explicit CommandBar(QWidget* parent = nullptr);

    void setWorkspaceInfo(const QString& text);
    void setWorkflowInfo(const QString& text);

    IconSquareButton* addButton(const QIcon& icon, const QString& tooltip,
                                 IconSquareButton::Role role = IconSquareButton::Role::Secondary);
    IconSquareButton* addActionButton(const QIcon& icon, QAction* action,
                                       IconSquareButton::Role role = IconSquareButton::Role::Secondary);
    void addSeparator();

private:
    QLabel* m_workspaceLabel = nullptr;
    QLabel* m_workflowLabel = nullptr;
    QHBoxLayout* m_rightLayout = nullptr;
};

} // namespace vws::ui
