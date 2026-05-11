#pragma once

#include "ui/canvas/EdgeRouteTypes.h"

namespace vws::ui {

class EdgeRouter final {
public:
    EdgeRouteResult route(const EdgeRouteRequest& request) const;
};

} // namespace vws::ui
