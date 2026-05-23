#pragma once

#include <QList>
#include <QString>

namespace vws::ui {

struct CanvasBreadcrumbItemViewModel {
    QString label;
    int depth = 0;
    bool clickable = true;
};

struct CanvasBreadcrumbViewModel {
    QList<CanvasBreadcrumbItemViewModel> items;
};

} // namespace vws::ui
