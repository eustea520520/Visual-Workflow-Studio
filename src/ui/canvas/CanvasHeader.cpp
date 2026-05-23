#include "ui/canvas/CanvasHeader.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace vws::ui {

CanvasHeader::CanvasHeader(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("canvasHeader"));
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(12, 6, 12, 6);
    m_layout->setSpacing(6);
}

void CanvasHeader::render(const CanvasBreadcrumbViewModel& viewModel)
{
    clearItems();

    if (viewModel.items.isEmpty()) {
        auto* label = new QLabel(tr("No workspace / No workflow"), this);
        label->setObjectName(QStringLiteral("canvasBreadcrumbEmpty"));
        m_layout->addWidget(label);
        m_items.append(label);
        m_layout->addStretch(1);
        return;
    }

    for (int index = 0; index < viewModel.items.size(); ++index) {
        const auto item = viewModel.items.at(index);
        if (index > 0) {
            auto* separator = new QLabel(QStringLiteral("/"), this);
            separator->setObjectName(QStringLiteral("canvasBreadcrumbSeparator"));
            m_layout->addWidget(separator);
            m_items.append(separator);
        }

        auto* button = new QPushButton(item.label, this);
        button->setObjectName(QStringLiteral("canvasBreadcrumbItem"));
        button->setFlat(true);
        button->setEnabled(item.clickable);
        button->setCursor(item.clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
        connect(button, &QPushButton::clicked, this, [this, depth = item.depth]() {
            emit breadcrumbClicked(depth);
        });
        m_layout->addWidget(button);
        m_items.append(button);
    }
    m_layout->addStretch(1);
}

void CanvasHeader::clearItems()
{
    while (auto* item = m_layout->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_items.clear();
}

} // namespace vws::ui
