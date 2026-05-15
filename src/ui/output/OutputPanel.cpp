#include "ui/output/OutputPanel.h"

#include "domain/RunRecord.h"
#include "execution/WorkflowExecutionResult.h"
#include "ui/editor/PythonCodeEditor.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QAbstractItemView>
#include <QFrame>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vws::ui {

namespace {

constexpr int ArtifactPreviewRows = 8;
constexpr qsizetype MaxPreviewCharacters = 2400;
constexpr qsizetype MaxTableCellCharacters = 1200;
constexpr qsizetype MaxAutoResizeCharacters = 240;

QString compactJson(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString tableCellPreview(const QString& text)
{
    if (text.size() <= MaxTableCellCharacters) {
        return text;
    }

    return text.left(MaxTableCellCharacters) + QStringLiteral("\n...");
}

} // namespace

OutputPanel::OutputPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("outputPanel"));
    setProperty("panel", true);
    buildUi();
}

void OutputPanel::render(const OutputPanelViewModel& viewModel)
{
    m_workflowName = viewModel.workflowName;
    m_nodeNames = viewModel.nodeNamesById;
}

void OutputPanel::clearRun()
{
    m_timelineTable->setRowCount(0);
    m_nodeRunTable->setRowCount(0);
    m_threadTraceTable->setRowCount(0);
    m_artifactTable->setRowCount(0);
    m_nodeRunRows.clear();
    m_stdoutView->clear();
    m_debugOutputView->clear();
    m_stderrView->clear();
    m_tracebackView->clear();
}

void OutputPanel::recordWorkflowStatus(const QString& runId, const QString& status)
{
    appendTimelineRow(runId, tr("Workflow"), displayWorkflowName(runId), status);
}

void OutputPanel::recordNodeStatus(const QString& runId, const QString& nodeId, const QString& status)
{
    appendTimelineRow(runId, tr("Node"), displayNodeName(nodeId), status);

    const auto row = ensureNodeRunRow(nodeId);
    setCell(m_nodeRunTable, row, 1, status);
    setCell(m_nodeRunTable, row, 2, nowText());
}

void OutputPanel::recordNodeOutput(const QString& runId, const QString& nodeId, const QJsonObject& outputs)
{
    appendTimelineRow(runId, tr("Node Output"), displayNodeName(nodeId), tr("Output ready"));

    const auto row = ensureNodeRunRow(nodeId);
    setCell(m_nodeRunTable, row, 4, compactJson(outputs));
}

void OutputPanel::recordNodeError(const QString& runId, const QString& nodeId, const QString& message)
{
    appendTimelineRow(runId, tr("Node Error"), displayNodeName(nodeId), message);

    const auto row = ensureNodeRunRow(nodeId);
    setCell(m_nodeRunTable, row, 1, tr("Failed"));
    setCell(m_nodeRunTable, row, 5, message);
    appendStderr(QString("[%1] %2").arg(displayNodeName(nodeId), message));
}

void OutputPanel::recordThreadTrace(
    const QString& runId,
    const QString& nodeId,
    const QString& phase,
    const QString& threadId,
    const QString& threadName)
{
    if (m_threadTraceTable == nullptr) {
        return;
    }

    const auto row = m_threadTraceTable->rowCount();
    m_threadTraceTable->insertRow(row);
    setCell(m_threadTraceTable, row, 0, nowText());
    setCell(m_threadTraceTable, row, 1, displayWorkflowName(runId));
    setCell(m_threadTraceTable, row, 2, nodeId.trimmed().isEmpty() ? tr("(workflow)") : displayNodeName(nodeId));
    setCell(m_threadTraceTable, row, 3, phase);
    setCell(m_threadTraceTable, row, 4, threadId);
    setCell(m_threadTraceTable, row, 5, threadName);
    m_threadTraceTable->scrollToBottom();
}

void OutputPanel::showExecutionResult(const execution::WorkflowExecutionResult& result)
{
    for (auto it = result.nodeStatuses.cbegin(); it != result.nodeStatuses.cend(); ++it) {
        const auto row = ensureNodeRunRow(it.key());
        setCell(m_nodeRunTable, row, 1, it.value());
    }

    QList<domain::Artifact> artifacts;
    for (auto it = result.nodeResults.cbegin(); it != result.nodeResults.cend(); ++it) {
        const auto& nodeId = it.key();
        const auto& nodeResult = it.value();
        const auto row = ensureNodeRunRow(nodeId);

        setCell(m_nodeRunTable, row, 3, nodeResult.stdoutText.left(300));
        setCell(m_nodeRunTable, row, 4, compactJson(nodeResult.outputs));
        setCell(m_nodeRunTable, row, 5, nodeResult.errorMessage);

        const auto nodeDisplayName = displayNodeName(nodeId);
        if (!nodeResult.stdoutText.trimmed().isEmpty()) {
            appendDebugOutput(QString("[%1]\n%2").arg(nodeDisplayName, nodeResult.stdoutText));
        }
        if (!nodeResult.stderrText.trimmed().isEmpty()) {
            appendStderr(QString("[%1]\n%2").arg(nodeDisplayName, nodeResult.stderrText));
        }
        if (!nodeResult.errorStack.trimmed().isEmpty()) {
            appendTraceback(QString("[%1]\n%2").arg(nodeDisplayName, nodeResult.errorStack));
        }
        artifacts.append(nodeResult.artifacts);
    }

    showArtifacts(artifacts);
}

void OutputPanel::showRunRecord(
    const domain::RunRecord& record,
    const QHash<QString, QJsonObject>& nodeOutputsByNodeId)
{
    Q_UNUSED(nodeOutputsByNodeId);

    appendTimelineRow(record.id, tr("Workflow"), displayWorkflowName(record.id), record.status);

    for (const auto& nodeRun : record.nodeRuns) {
        const auto row = ensureNodeRunRow(nodeRun.nodeId);
        setCell(m_nodeRunTable, row, 1, nodeRun.status);
        setCell(m_nodeRunTable, row, 2, nodeRun.finishedAt);

        QJsonObject outputObject;
        QFile outputFile(nodeRun.outputPath);
        QJsonObject outputFileObj;
        if (outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const auto doc = QJsonDocument::fromJson(outputFile.readAll());
            if (doc.isObject()) {
                outputFileObj = doc.object();
                const auto outputs = outputFileObj.value("outputs").toObject();
                setCell(m_nodeRunTable, row, 4, compactJson(outputs));
            }
        }

        if (!nodeRun.status.trimmed().toLower().contains("succeeded")) {
            const auto errorText = outputFileObj.value("error").toString();
            if (!errorText.isEmpty()) {
                setCell(m_nodeRunTable, row, 5, errorText);
            }
        }
    }

    showArtifacts(record.artifacts);
}

void OutputPanel::appendStdout(const QString& text)
{
    if (m_stdoutView != nullptr && !text.isEmpty()) {
        m_stdoutView->appendPlainText(text);
    }
}

void OutputPanel::appendDebugOutput(const QString& text)
{
    if (m_debugOutputView != nullptr && !text.isEmpty()) {
        m_debugOutputView->appendPlainText(text);
    }
}

void OutputPanel::appendStderr(const QString& text)
{
    if (m_stderrView != nullptr && !text.isEmpty()) {
        m_stderrView->appendPlainText(text);
    }
}

void OutputPanel::appendTraceback(const QString& text)
{
    if (m_tracebackView != nullptr && !text.isEmpty()) {
        m_tracebackView->appendPlainText(text);
    }
}

void OutputPanel::showArtifacts(const QList<domain::Artifact>& artifacts)
{
    m_artifactTable->setRowCount(0);
    for (const auto& artifact : artifacts) {
        const auto row = m_artifactTable->rowCount();
        m_artifactTable->insertRow(row);
        setCell(m_artifactTable, row, 0, displayNodeName(artifact.nodeId));
        setCell(m_artifactTable, row, 1, artifact.type);
        setCell(m_artifactTable, row, 2, artifact.path);
        setCell(m_artifactTable, row, 3, QFileInfo(artifact.path).exists()
                ? QString::number(QFileInfo(artifact.path).size())
                : QString{});
        setCell(m_artifactTable, row, 4, previewArtifactRows(artifact.path, ArtifactPreviewRows));
    }
}

QString OutputPanel::previewArtifactRows(const QString& filePath, int maxRows) const
{
    QFile file(filePath);
    if (filePath.trimmed().isEmpty()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return tr("Cannot preview: %1").arg(file.errorString());
    }

    QStringList lines;
    while (!file.atEnd() && lines.size() < maxRows) {
        auto line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.size() > MaxPreviewCharacters) {
            line = line.left(MaxPreviewCharacters) + "...";
        }
        lines.append(line);
    }

    return lines.join('\n');
}

int OutputPanel::timelineRowCount() const
{
    return m_timelineTable != nullptr ? m_timelineTable->rowCount() : 0;
}

int OutputPanel::nodeRunRowCount() const
{
    return m_nodeRunTable != nullptr ? m_nodeRunTable->rowCount() : 0;
}

int OutputPanel::artifactRowCount() const
{
    return m_artifactTable != nullptr ? m_artifactTable->rowCount() : 0;
}

int OutputPanel::threadTraceRowCount() const
{
    return m_threadTraceTable != nullptr ? m_threadTraceTable->rowCount() : 0;
}

void OutputPanel::buildUi()
{
    auto* title = new QLabel(tr("Output / Logs / Errors / Artifacts"), this);
    title->setObjectName("panelTitle");

    auto setupTable = [](QTableWidget* table) {
        table->setAlternatingRowColors(true);
        table->setShowGrid(false);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setFrameShape(QFrame::NoFrame);
        table->setWordWrap(false);
        table->horizontalHeader()->setStretchLastSection(true);
    };

    m_timelineTable = new QTableWidget(0, 4, this);
    m_timelineTable->setObjectName(QStringLiteral("timelineTable"));
    m_timelineTable->setHorizontalHeaderLabels({tr("Time"), tr("Scope"), tr("Id"), tr("Status / Message")});
    m_timelineTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setupTable(m_timelineTable);

    m_nodeRunTable = new QTableWidget(0, 6, this);
    m_nodeRunTable->setObjectName(QStringLiteral("nodeRunTable"));
    m_nodeRunTable->setHorizontalHeaderLabels({tr("Node"), tr("Status"), tr("Updated"), tr("Debug"), tr("Output"), tr("Error")});
    setupTable(m_nodeRunTable);
    auto* nodeRunHeader = m_nodeRunTable->horizontalHeader();
    nodeRunHeader->setStretchLastSection(false);
    nodeRunHeader->setMinimumSectionSize(72);
    nodeRunHeader->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    nodeRunHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    nodeRunHeader->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    nodeRunHeader->setSectionResizeMode(3, QHeaderView::Interactive);
    nodeRunHeader->setSectionResizeMode(4, QHeaderView::Stretch);
    nodeRunHeader->setSectionResizeMode(5, QHeaderView::Interactive);
    m_nodeRunTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_nodeRunTable->setColumnWidth(3, 220);
    m_nodeRunTable->setColumnWidth(4, 360);
    m_nodeRunTable->setColumnWidth(5, 260);
    for (int column = 0; column < m_nodeRunTable->columnCount(); ++column) {
        auto* item = m_nodeRunTable->horizontalHeaderItem(column);
        if (item != nullptr) {
            item->setTextAlignment(Qt::AlignCenter);
        }
    }

    m_threadTraceTable = new QTableWidget(0, 6, this);
    m_threadTraceTable->setObjectName(QStringLiteral("threadTraceTable"));
    m_threadTraceTable->setHorizontalHeaderLabels({tr("Time"), tr("Run"), tr("Node"), tr("Phase"), tr("Thread Id"), tr("Thread Name")});
    m_threadTraceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setupTable(m_threadTraceTable);

    m_stdoutView = new QPlainTextEdit(this);
    m_stdoutView->setObjectName(QStringLiteral("stdoutView"));
    m_stdoutView->setReadOnly(true);
    m_stdoutView->setProperty("readOnly", true);
    m_stdoutView->setPlaceholderText(tr("application logs will appear here"));

    m_debugOutputView = new QPlainTextEdit(this);
    m_debugOutputView->setObjectName("debugOutputView");
    m_debugOutputView->setReadOnly(true);
    m_debugOutputView->setProperty("readOnly", true);
    m_debugOutputView->setPlaceholderText(tr("Python print() output will appear here"));

    m_stderrView = new PythonCodeEditor(this);
    m_stderrView->setObjectName(QStringLiteral("stderrView"));
    m_stderrView->setReadOnly(true);
    m_stderrView->setProperty("readOnly", true);
    m_stderrView->setPlaceholderText(tr("stderr will appear here"));

    m_tracebackView = new PythonCodeEditor(this);
    m_tracebackView->setObjectName(QStringLiteral("tracebackView"));
    m_tracebackView->setReadOnly(true);
    m_tracebackView->setProperty("readOnly", true);
    m_tracebackView->setPlaceholderText(tr("tracebacks will appear here"));

    m_artifactTable = new QTableWidget(0, 5, this);
    m_artifactTable->setObjectName(QStringLiteral("artifactTable"));
    m_artifactTable->setHorizontalHeaderLabels({tr("Node"), tr("Type"), tr("Path"), tr("Size"), tr("Preview")});
    m_artifactTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setupTable(m_artifactTable);

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("outputTabs"));
    tabs->addTab(m_timelineTable, tr("Run Timeline"));
    tabs->addTab(m_nodeRunTable, tr("Node Runs"));
    tabs->addTab(m_threadTraceTable, tr("Thread Trace"));
    tabs->addTab(m_stdoutView, tr("Logs"));
    tabs->addTab(m_debugOutputView, tr("Debug Output"));
    tabs->addTab(m_stderrView, tr("stderr"));
    tabs->addTab(m_tracebackView, tr("Traceback"));
    tabs->addTab(m_artifactTable, tr("Artifacts"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(title);
    layout->addWidget(tabs, 1);
}

void OutputPanel::appendTimelineRow(const QString& runId, const QString& scope, const QString& itemId, const QString& status)
{
    const auto row = m_timelineTable->rowCount();
    m_timelineTable->insertRow(row);
    setCell(m_timelineTable, row, 0, nowText());
    setCell(m_timelineTable, row, 1, scope);
    setCell(m_timelineTable, row, 2, itemId.isEmpty() ? runId : itemId);
    setCell(m_timelineTable, row, 3, status);
    m_timelineTable->scrollToBottom();
}

int OutputPanel::ensureNodeRunRow(const QString& nodeId)
{
    if (m_nodeRunRows.contains(nodeId)) {
        return m_nodeRunRows.value(nodeId);
    }

    const auto row = m_nodeRunTable->rowCount();
    m_nodeRunTable->insertRow(row);
    m_nodeRunRows.insert(nodeId, row);
    setCell(m_nodeRunTable, row, 0, displayNodeName(nodeId));
    return row;
}

QString OutputPanel::displayWorkflowName(const QString& runId) const
{
    const auto name = m_workflowName.trimmed();
    if (name.isEmpty()) {
        return runId;
    }

    const auto shortId = runId.left(8);
    return shortId.isEmpty()
        ? name
        : QString("%1 (%2)").arg(name, shortId);
}

QString OutputPanel::displayNodeName(const QString& nodeId) const
{
    const auto name = m_nodeNames.value(nodeId).trimmed();
    if (name.isEmpty()) {
        return nodeId;
    }

    const auto shortId = nodeId.left(8);
    return shortId.isEmpty()
        ? name
        : QString("%1 (%2)").arg(name, shortId);
}

void OutputPanel::setCell(QTableWidget* table, int row, int column, const QString& text)
{
    auto* item = table->item(row, column);
    if (item == nullptr) {
        item = new QTableWidgetItem();
        table->setItem(row, column, item);
    }
    item->setText(tableCellPreview(text));
    item->setToolTip(text);
    if (text.size() <= MaxAutoResizeCharacters) {
        table->resizeColumnToContents(column);
    }
    table->resizeRowToContents(row);
}

QString OutputPanel::nowText() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}

} // namespace vws::ui
