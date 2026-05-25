#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextStream>

namespace {

constexpr int kCurrentWorkflowSchemaVersion = 2;

int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}

int normalizedSlot(const QJsonObject& edge, const QString& key)
{
    const auto value = edge.value(key).toInt(0);
    return value < 0 ? 0 : value;
}

QJsonObject migratedWorkflow(QJsonObject workflow)
{
    workflow.insert(QStringLiteral("schema_version"), kCurrentWorkflowSchemaVersion);

    QJsonArray migratedEdges;
    const auto edges = workflow.value(QStringLiteral("edges")).toArray();
    for (const auto& value : edges) {
        auto edge = value.toObject();
        edge.insert(QStringLiteral("from_slot"), normalizedSlot(edge, QStringLiteral("from_slot")));
        edge.insert(QStringLiteral("to_slot"), normalizedSlot(edge, QStringLiteral("to_slot")));
        migratedEdges.append(edge);
    }
    workflow.insert(QStringLiteral("edges"), migratedEdges);

    return workflow;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() != 3) {
        return fail(QStringLiteral("Usage: vws_workflow_migrator <input-workflow.json> <output-workflow.json>"));
    }

    QFile input(arguments.at(1));
    if (!input.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Could not open input workflow: %1").arg(input.errorString()));
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("Input workflow is not valid JSON object: %1").arg(parseError.errorString()));
    }

    QFile output(arguments.at(2));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return fail(QStringLiteral("Could not open output workflow: %1").arg(output.errorString()));
    }

    output.write(QJsonDocument(migratedWorkflow(document.object())).toJson(QJsonDocument::Indented));
    QTextStream(stdout) << QStringLiteral("Migrated workflow written to: %1").arg(arguments.at(2)) << Qt::endl;
    return 0;
}
