#include "ui/widgets/CommandBar.h"
#include "ui/widgets/IconSquareButton.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

namespace vws::ui {

CommandBar::CommandBar(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("CommandBar"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(10);

    // Left: icon buttons
    m_rightLayout = new QHBoxLayout();
    m_rightLayout->setSpacing(8);
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(m_rightLayout);

    // Center: stretch
    layout->addStretch(1);

    m_workspaceLabel = new QLabel(this);
    m_workspaceLabel->hide();
    m_workflowLabel = new QLabel(this);
    m_workflowLabel->hide();
}

void CommandBar::setWorkspaceInfo(const QString& text)
{
    m_workspaceLabel->setText(text);
}

void CommandBar::setWorkflowInfo(const QString& text)
{
    m_workflowLabel->setText(text);
}

IconSquareButton* CommandBar::addButton(const QIcon& icon, const QString& tooltip,
                                         IconSquareButton::Role role)
{
    auto* button = new IconSquareButton(icon, tooltip, this);
    button->setRole(role);
    m_rightLayout->addWidget(button);
    return button;
}

IconSquareButton* CommandBar::addActionButton(const QIcon& icon, QAction* action,
                                               IconSquareButton::Role role)
{
    const auto tooltip = action != nullptr
        ? action->toolTip().isEmpty() ? action->text() : action->toolTip()
        : QString{};

    auto* button = addButton(icon, tooltip, role);

    if (action != nullptr) {
        connect(button, &QPushButton::clicked, action, &QAction::trigger);
        connect(action, &QAction::changed, button, [action, button]() {
            const auto tip = action->toolTip().isEmpty()
                ? action->text() : action->toolTip();
            button->setToolTip(tip);
        });
    }

    return button;
}

void CommandBar::addSeparator()
{
    auto* separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("commandBarSeparator"));
    separator->setFrameShape(QFrame::NoFrame);
    separator->setFixedWidth(1);
    m_rightLayout->addWidget(separator);
}

} // namespace vws::ui
