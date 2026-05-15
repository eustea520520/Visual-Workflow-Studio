#include "ui/widgets/EmptyStateOverlay.h"

#include "ui/theme/UiMetrics.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace vws::ui {

EmptyStateOverlay::EmptyStateOverlay(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    render(Mode::Hidden);
}

void EmptyStateOverlay::render(Mode mode)
{
    m_mode = mode;

    switch (m_mode) {
    case Mode::NoWorkspace:
        m_title->setText(tr("No workspace is open"));
        m_primaryButton->setText(tr("New Workspace"));
        m_secondaryButton->setText(tr("Open Workspace"));
        show();
        raise();
        break;
    case Mode::NoWorkflow:
        m_title->setText(tr("No workflow is open"));
        m_primaryButton->setText(tr("New Workflow"));
        m_secondaryButton->setText(tr("Open Workflow"));
        show();
        raise();
        break;
    case Mode::Hidden:
        hide();
        break;
    }
}

EmptyStateOverlay::Mode EmptyStateOverlay::mode() const
{
    return m_mode;
}

void EmptyStateOverlay::buildUi()
{
    setObjectName(QStringLiteral("canvasOverlay"));
    setAutoFillBackground(true);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("canvasOverlayTitle"));
    m_title->setAlignment(Qt::AlignCenter);

    m_primaryButton = new QPushButton(this);
    m_primaryButton->setObjectName(QStringLiteral("overlayPrimaryButton"));
    m_primaryButton->setProperty("buttonRole", "primary");

    m_secondaryButton = new QPushButton(this);
    m_secondaryButton->setObjectName(QStringLiteral("overlaySecondaryButton"));
    m_secondaryButton->setProperty("buttonRole", "secondary");

    connect(m_primaryButton, &QPushButton::clicked, this, &EmptyStateOverlay::emitPrimaryIntent);
    connect(m_secondaryButton, &QPushButton::clicked, this, &EmptyStateOverlay::emitSecondaryIntent);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_primaryButton);
    buttonLayout->addWidget(m_secondaryButton);
    buttonLayout->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(m_title);
    layout->addSpacing(UiMetrics::OverlaySpacing);
    layout->addLayout(buttonLayout);
    layout->addStretch(1);
}

void EmptyStateOverlay::emitPrimaryIntent()
{
    if (m_mode == Mode::NoWorkspace) {
        emit createWorkspaceRequested();
    } else if (m_mode == Mode::NoWorkflow) {
        emit createWorkflowRequested();
    }
}

void EmptyStateOverlay::emitSecondaryIntent()
{
    if (m_mode == Mode::NoWorkspace) {
        emit openWorkspaceRequested();
    } else if (m_mode == Mode::NoWorkflow) {
        emit openWorkflowRequested();
    }
}

} // namespace vws::ui
