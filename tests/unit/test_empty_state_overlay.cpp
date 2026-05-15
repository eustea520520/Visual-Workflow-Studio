#include "ui/widgets/EmptyStateOverlay.h"

#include <QApplication>
#include <QPushButton>
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    vws::ui::EmptyStateOverlay overlay;
    int createWorkspaceCount = 0;
    int openWorkspaceCount = 0;
    int createWorkflowCount = 0;
    int openWorkflowCount = 0;

    QObject::connect(&overlay, &vws::ui::EmptyStateOverlay::createWorkspaceRequested,
        [&createWorkspaceCount]() { ++createWorkspaceCount; });
    QObject::connect(&overlay, &vws::ui::EmptyStateOverlay::openWorkspaceRequested,
        [&openWorkspaceCount]() { ++openWorkspaceCount; });
    QObject::connect(&overlay, &vws::ui::EmptyStateOverlay::createWorkflowRequested,
        [&createWorkflowCount]() { ++createWorkflowCount; });
    QObject::connect(&overlay, &vws::ui::EmptyStateOverlay::openWorkflowRequested,
        [&openWorkflowCount]() { ++openWorkflowCount; });

    auto* primaryButton = overlay.findChild<QPushButton*>("overlayPrimaryButton");
    auto* secondaryButton = overlay.findChild<QPushButton*>("overlaySecondaryButton");
    if (const auto check = expect(primaryButton != nullptr, "Overlay should expose primary button")) {
        return check;
    }
    if (const auto check = expect(secondaryButton != nullptr, "Overlay should expose secondary button")) {
        return check;
    }

    overlay.render(vws::ui::EmptyStateOverlay::Mode::NoWorkspace);
    primaryButton->click();
    secondaryButton->click();
    if (const auto check = expect(createWorkspaceCount == 1, "NoWorkspace primary should request workspace creation")) {
        return check;
    }
    if (const auto check = expect(openWorkspaceCount == 1, "NoWorkspace secondary should request workspace opening")) {
        return check;
    }

    overlay.render(vws::ui::EmptyStateOverlay::Mode::NoWorkflow);
    primaryButton->click();
    secondaryButton->click();
    if (const auto check = expect(createWorkflowCount == 1, "NoWorkflow primary should request workflow creation")) {
        return check;
    }
    if (const auto check = expect(openWorkflowCount == 1, "NoWorkflow secondary should request workflow opening")) {
        return check;
    }

    overlay.render(vws::ui::EmptyStateOverlay::Mode::Hidden);
    if (const auto check = expect(!overlay.isVisible(), "Hidden mode should hide overlay")) {
        return check;
    }

    QTextStream(stdout) << "empty state overlay tests passed" << Qt::endl;
    return 0;
}
