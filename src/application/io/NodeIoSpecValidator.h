#pragma once

#include "domain/NodeIoSpec.h"

#include <QStringList>

namespace vws::application {

class NodeIoSpecValidator final {
public:
    bool validate(const domain::NodeIoSpec& spec, QStringList& errors) const;
};

} // namespace vws::application
