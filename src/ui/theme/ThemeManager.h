#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>

namespace vws::ui {

enum class AppTheme {
    Light,
    Dark
};

class ThemeManager : public QObject {
    Q_OBJECT

public:
    explicit ThemeManager(QObject* parent = nullptr);

    void applyTheme(AppTheme theme);
    AppTheme currentTheme() const;
    void toggleTheme();
    bool isDark() const;

    QColor color(const QString& token) const;

    static ThemeManager* instance();
    static void setInstance(ThemeManager* mgr);

signals:
    void themeChanged(AppTheme theme);

private:
    QString loadStyleSheet(const QString& resourcePath) const;
    QString renderStyleSheet(QString styleSheet) const;
    void buildColorMaps();

    AppTheme m_currentTheme = AppTheme::Light;
    bool m_styleSheetApplied = false;
    QHash<QString, QColor> m_lightColors;
    QHash<QString, QColor> m_darkColors;
    static ThemeManager* s_instance;
};

} // namespace vws::ui
