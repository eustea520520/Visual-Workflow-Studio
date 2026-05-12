#pragma once

#include "domain/Node.h"

#include <QJsonObject>
#include <QString>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;

namespace vws::ui {

class PythonCodeEditor;

// 右侧属性检查器。
// 用户选中节点后，这里会展示节点名称、类型、运行参数、Python/Agent 配置等。
class NodeInspector final : public QWidget {
public:
    explicit NodeInspector(QWidget* parent = nullptr);

    void displayNode(const domain::Node& node);
    void displayNode(const domain::Node& node, const QJsonObject& selectedNodeOutput);
    void clearSelectedNodeOutput();
    void clear();

private:
    void buildUi();
    void setReadOnly(QLineEdit* edit);
    void setReadOnly(QPlainTextEdit* edit);

    PythonCodeEditor* m_pythonEditor = nullptr;
    PythonCodeEditor* m_outputJsonEditor = nullptr;
    QTabWidget* m_tabs = nullptr;
    QString m_currentNodeId;
    QLineEdit* m_agentTitleEdit = nullptr;
    QLineEdit* m_agentDescriptionEdit = nullptr;
    QLineEdit* m_agentTimeoutEdit = nullptr;
    QLineEdit* m_agentTemplateEdit = nullptr;
    QLineEdit* m_agentUrlEdit = nullptr;
    QLineEdit* m_agentModelEdit = nullptr;
    QLineEdit* m_agentApiKeyEdit = nullptr;
    QLineEdit* m_agentMaxRetriesEdit = nullptr;
    QPlainTextEdit* m_agentBackgroundPromptEdit = nullptr;
    QPlainTextEdit* m_agentTaskPromptEdit = nullptr;
};

} // namespace vws::ui
