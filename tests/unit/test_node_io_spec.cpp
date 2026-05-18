#include "application/io/NodeIoSpecUtils.h"
#include "domain/NodeIoSpec.h"

#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}

int expect(bool condition, const QString& message)
{
    return condition ? 0 : fail(message);
}

} // namespace

int main()
{
    vws::domain::PortDimensionSpec output;
    output.portName = "output";
    output.dimension = 3;
    output.source = "test";
    output.itemLabels = {"a", "b", "c"};

    vws::domain::NodeIoSpec spec;
    spec.outputs.append(output);

    const auto restored = vws::domain::NodeIoSpec::fromJson(spec.toJson());
    if (const auto check = expect(restored.outputs.size() == 1, "NodeIoSpec should restore output specs")) {
        return check;
    }
    if (const auto check = expect(restored.outputs.first().dimension == 3, "NodeIoSpec should persist dimension")) {
        return check;
    }
    if (const auto check = expect(restored.outputs.first().itemLabels == QStringList({"a", "b", "c"}),
            "NodeIoSpec should persist labels")) {
        return check;
    }

    vws::domain::NodeIoSpec base;
    base.outputs.append(vws::application::NodeIoSpecUtils::makePortSpec(
        "output",
        3,
        "code-comment",
        {"raw", "clean", "summary"}));

    vws::domain::NodeIoSpec runtimePatch;
    runtimePatch.outputs.append(vws::application::NodeIoSpecUtils::makePortSpec(
        "output",
        3,
        "runtime",
        {"1", "2", "3"}));

    const auto merged = vws::application::NodeIoSpecUtils::merged(base, runtimePatch);
    if (const auto check = expect(merged.outputs.first().itemLabels == QStringList({"raw", "clean", "summary"}),
            "Runtime IO merge should preserve user-defined labels")) {
        return check;
    }

    QTextStream(stdout) << "node io spec tests passed" << Qt::endl;
    return 0;
}
