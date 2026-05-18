#include "application/io/PythonIoDimensionAnalyzer.h"

#include "application/io/NodeIoSpecUtils.h"
#include "domain/NodeConfigKeys.h"

#include <QRegularExpression>

namespace vws::application {

namespace {

QList<domain::PortDimensionSpec> parseSide(const QString& code, const QString& side)
{
    QList<domain::PortDimensionSpec> specs;
    const QRegularExpression pattern(QStringLiteral(
        R"(^\s*#\s*vws:%1\s+([A-Za-z_][A-Za-z0-9_]*)\s+dimension\s*=\s*(\d+)(?:\s+labels\s*=\s*([^\r\n#]+))?)")
        .arg(side),
        QRegularExpression::MultilineOption);
    auto matchIt = pattern.globalMatch(code);
    while (matchIt.hasNext()) {
        const auto match = matchIt.next();
        const auto labelsText = match.captured(3).trimmed();
        QStringList labels;
        if (!labelsText.isEmpty()) {
            for (const auto& label : labelsText.split(',', Qt::SkipEmptyParts)) {
                labels.append(label.trimmed());
            }
        }
        specs.append(NodeIoSpecUtils::makePortSpec(
            match.captured(1),
            match.captured(2).toInt(),
            QStringLiteral("code-comment"),
            labels));
    }
    return specs;
}

} // namespace

domain::NodeIoSpec PythonIoDimensionAnalyzer::analyze(const domain::Node& node) const
{
    const auto code = node.config.value(domain::NodeConfigKeys::Code).toString();

    domain::NodeIoSpec spec;
    spec.inputs = parseSide(code, QStringLiteral("input"));
    spec.outputs = parseSide(code, QStringLiteral("output"));
    return spec;
}

} // namespace vws::application
