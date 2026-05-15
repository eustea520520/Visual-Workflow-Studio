#include "ui/theme/ThemeManager.h"

#include <QApplication>
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

int verifyTheme(vws::ui::ThemeManager& themeManager, vws::ui::AppTheme theme)
{
    themeManager.applyTheme(theme);
    const auto styleSheet = qApp->styleSheet();
    if (const auto check = expect(!styleSheet.isEmpty(), "Theme stylesheet should be loaded")) {
        return check;
    }
    if (const auto check = expect(!styleSheet.contains("${"), "Theme tokens should be rendered before applying QSS")) {
        return check;
    }
    if (const auto check = expect(styleSheet.contains("QWidget#canvasOverlay"),
            "Canvas overlay style should remain centralized in QSS")) {
        return check;
    }
    if (const auto check = expect(!styleSheet.contains("QWidget QWidget QWidget"),
            "QSS should not use broad deep QWidget descendant selectors")) {
        return check;
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    vws::ui::ThemeManager themeManager;
    vws::ui::ThemeManager::setInstance(&themeManager);

    if (const auto check = verifyTheme(themeManager, vws::ui::AppTheme::Light)) {
        return check;
    }
    if (const auto check = verifyTheme(themeManager, vws::ui::AppTheme::Dark)) {
        return check;
    }

    QTextStream(stdout) << "theme manager tests passed" << Qt::endl;
    return 0;
}
