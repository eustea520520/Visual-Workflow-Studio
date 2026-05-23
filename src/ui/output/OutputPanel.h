#pragma once

#include "domain/Artifact.h"
#include "domain/RunRecord.h"
#include "ui/output/OutputPanelViewModel.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QWidget>

class QPlainTextEdit;
class QTableWidget;
class QTabWidget;

namespace vws::execution {
struct WorkflowExecutionResult;
}

namespace vws::ui {

class PythonCodeEditor;

// 底部运行结果面板。
//
// 它只负责展示运行结果：时间线、节点运行表、程序日志、调试输出、stderr、traceback、输出 JSON、Artifact。
// 运行逻辑仍然在 ExecutionEngine，文件保存仍然由 Worker / application service 负责。
class OutputPanel final : public QWidget {
public:
    explicit OutputPanel(QWidget* parent = nullptr);

    void render(const OutputPanelViewModel& viewModel);
    void setAdvancedDiagnosticsEnabled(bool enabled);
    bool advancedDiagnosticsEnabled() const;
    void clearRun();
    void recordWorkflowStatus(const QString& runId, const QString& status);
    void recordNodeStatus(const QString& runId, const QString& nodeId, const QString& status);
    void recordNodeOutput(const QString& runId, const QString& nodeId, const QJsonObject& outputs);
    void recordNodeError(const QString& runId, const QString& nodeId, const QString& message);
    void recordThreadTrace(
        const QString& runId,
        const QString& nodeId,
        const QString& phase,
        const QString& threadId,
        const QString& threadName);
    void showExecutionResult(const execution::WorkflowExecutionResult& result);
    void showRunRecord(
        const domain::RunRecord& record,
        const QHash<QString, QJsonObject>& nodeOutputsByNodeId);

    void appendStdout(const QString& text);
    void appendDebugOutput(const QString& text);
    void appendStderr(const QString& text);
    void appendTraceback(const QString& text);
    void showArtifacts(const QList<domain::Artifact>& artifacts);

    QString previewArtifactRows(const QString& filePath, int maxRows) const;

    int timelineRowCount() const;
    int nodeRunRowCount() const;
    int artifactRowCount() const;
    int threadTraceRowCount() const;

private:
    void buildUi();
    void updateAdvancedTabs();
    void addTabIfMissing(QWidget* widget, const QString& title, int index = -1);
    void removeTabForWidget(QWidget* widget);
    void appendTimelineRow(const QString& runId, const QString& scope, const QString& itemId, const QString& status);
    int ensureNodeRunRow(const QString& nodeId);
    QString displayWorkflowName(const QString& runId) const;
    QString displayNodeName(const QString& nodeId) const;
    void setCell(QTableWidget* table, int row, int column, const QString& text);
    QString nowText() const;

    QTableWidget* m_timelineTable = nullptr;
    QTableWidget* m_nodeRunTable = nullptr;
    QTableWidget* m_threadTraceTable = nullptr;
    QTableWidget* m_artifactTable = nullptr;
    QTabWidget* m_tabs = nullptr;
    QPlainTextEdit* m_stdoutView = nullptr;
    QPlainTextEdit* m_debugOutputView = nullptr;
    PythonCodeEditor* m_stderrView = nullptr;
    PythonCodeEditor* m_tracebackView = nullptr;
    QHash<QString, int> m_nodeRunRows;
    QHash<QString, QString> m_nodeNames;
    QString m_workflowName;
    bool m_advancedDiagnosticsEnabled = false;
};

} // namespace vws::ui
