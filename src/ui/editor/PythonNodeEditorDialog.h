#pragma once

#include "ui/editor/PythonCodeTemplates.h"

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace vws::ui {

class PythonCodeEditor;

// Python 节点编辑 Dialog。
// 它编辑节点标题、描述和 config.code，保存结果交给 MainWindow 同步回 Workflow。
class PythonNodeEditorDialog final : public QDialog {
    Q_OBJECT

public:
    PythonNodeEditorDialog(
        const QString& nodeName,
        const QString& nodeDescription,
        const QString& nodeType,
        const QJsonObject& nodeConfig,
        const QString& initialCode,
        const QString& defaultCode,
        QWidget* parent = nullptr);

    QString code() const;
    QString nodeName() const;
    QString nodeDescription() const;

signals:
    void nodeSaved(const QString& name, const QString& description, const QString& code, const QJsonObject& configPatch);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi(const QString& nodeName);
    void buildAgentSettings(QVBoxLayout* layout);
    void loadAgentTemplate();
    QJsonObject agentConfigPatch() const;
    QString agentUrl() const;
    QString agentModel() const;
    QString agentApiKey() const;
    QString agentBackgroundPrompt() const;
    QString agentTaskPrompt() const;
    DataTransferTemplate agentTransferTemplate() const;
    void save();
    bool confirmCloseIfDirty();
    void setDirty(bool dirty);
    void updateCursorStatus(int line, int column);

    PythonCodeEditor* m_editor = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_descriptionEdit = nullptr;
    QLineEdit* m_agentUrlEdit = nullptr;
    QLineEdit* m_agentModelEdit = nullptr;
    QLineEdit* m_agentApiKeyEdit = nullptr;
    QPlainTextEdit* m_agentBackgroundPromptEdit = nullptr;
    QPlainTextEdit* m_agentTaskPromptEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_dirtyLabel = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_loadAgentTemplateButton = nullptr;
    QString m_nodeType;
    QJsonObject m_nodeConfig;
    bool m_dirty = false;
    bool m_saveInProgress = false;
};

} // namespace vws::ui
