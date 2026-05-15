#include "ui/theme/StyleReloader.h"

#include <QStyle>
#include <QWidget>

namespace vws::ui {

void StyleReloader::refresh(QWidget* widget)
{
    if (widget == nullptr || widget->style() == nullptr) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace vws::ui
