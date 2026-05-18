#pragma once

#include "domain/Node.h"
#include "domain/NodeIoSpec.h"
#include "ui/canvas/NodePortSlotViewModel.h"

namespace vws::ui {

class NodePortSlotViewModelBuilder final {
public:
    NodePortSlots build(const domain::Node& node, const domain::NodeIoSpec& spec) const;
};

} // namespace vws::ui
