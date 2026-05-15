#pragma once

class QWidget;

namespace vws::ui {

class StyleReloader final {
public:
    static void refresh(QWidget* widget);
};

} // namespace vws::ui
