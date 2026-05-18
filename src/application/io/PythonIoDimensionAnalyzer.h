#pragma once

#include "domain/Node.h"
#include "domain/NodeIoSpec.h"

namespace vws::application {

class PythonIoDimensionAnalyzer final {
public:
    domain::NodeIoSpec analyze(const domain::Node& node) const;
};

} // namespace vws::application
