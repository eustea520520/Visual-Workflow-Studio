#pragma once

#include "ui/canvas/CanvasBreadcrumbViewModel.h"

#include <QFrame>
#include <QList>

class QHBoxLayout;
class QPushButton;

namespace vws::ui {

class CanvasHeader final : public QFrame {
    Q_OBJECT

public:
    explicit CanvasHeader(QWidget* parent = nullptr);

    void render(const CanvasBreadcrumbViewModel& viewModel);

signals:
    void breadcrumbClicked(int depth);

private:
    void clearItems();

    QHBoxLayout* m_layout = nullptr;
    QList<QWidget*> m_items;
};

} // namespace vws::ui
