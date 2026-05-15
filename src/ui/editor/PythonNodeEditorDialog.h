#pragma once

#include "application/PythonCodeTemplates.h"

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
using DataTransferTemplate = application::DataTransferTemplate;

// Dialog for editing node metadata, optional Agent settings, and Python code.
class PythonNodeEditorDialog final : public QDialog {
    Q_OBJECT

public:
    PythonNodeEditorDialog(
        const QString& nodeName,
        const QString& nodeDescription,
        int timeoutMs,
        const QString& nodeType,
        const QJsonObject& nodeConfig,
        const QString& initialCode,
        const QString& defaultCode,
        QWidget* parent = nullptr);

    QString code() const;
    QString nodeName() const;
    QString nodeDescription() const;
    int timeoutMs() const;
    bool validateTimeout(QString* errorMessage = nullptr) const;

signals:
    void nodeSaved(const QString& name, const QString& description, int timeoutMs, const QString& code, const QJsonObject& configPatch);

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
    int agentMaxRetries() const;
    bool validateAgentMaxRetries(QString* errorMessage = nullptr) const;
    DataTransferTemplate agentTransferTemplate() const;
    void save();
    void markAgentTemplateNeedsRefresh();
    void applyAgentTemplateToEditor();
    bool confirmCloseIfDirty();
    void setDirty(bool dirty);
    void updateCursorStatus(int line, int column);

    PythonCodeEditor* m_editor = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_descriptionEdit = nullptr;
    QLineEdit* m_timeoutEdit = nullptr;
    QLineEdit* m_agentUrlEdit = nullptr;
    QLineEdit* m_agentModelEdit = nullptr;
    QLineEdit* m_agentApiKeyEdit = nullptr;
    QLineEdit* m_agentMaxRetriesEdit = nullptr;
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
    bool m_agentTemplateNeedsRefresh = false;
};

} // namespace vws::ui
