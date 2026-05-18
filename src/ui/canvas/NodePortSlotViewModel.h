#pragma once

#include <QList>
#include <QString>

namespace vws::ui {

struct NodePortSlotViewModel {
    QString portName;
    int slotIndex = 0;
    QString label;
};

struct NodePortSlots {
    QList<NodePortSlotViewModel> inputs;
    QList<NodePortSlotViewModel> outputs;
};

} // namespace vws::ui
