#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace vws::ui {

class EmptyStateOverlay final : public QWidget {
    Q_OBJECT

public:
    enum class Mode {
        Hidden,
        NoWorkspace,
        NoWorkflow
    };

    explicit EmptyStateOverlay(QWidget* parent = nullptr);

    void render(Mode mode);
    Mode mode() const;

signals:
    void createWorkspaceRequested();
    void openWorkspaceRequested();
    void createWorkflowRequested();
    void openWorkflowRequested();

private:
    void buildUi();
    void emitPrimaryIntent();
    void emitSecondaryIntent();

    Mode m_mode = Mode::Hidden;
    QLabel* m_title = nullptr;
    QPushButton* m_primaryButton = nullptr;
    QPushButton* m_secondaryButton = nullptr;
};

} // namespace vws::ui
