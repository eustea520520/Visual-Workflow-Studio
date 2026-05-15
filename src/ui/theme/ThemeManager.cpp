#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QStyle>
#include <QTextStream>
#include <QWidget>

namespace vws::ui {

ThemeManager* ThemeManager::s_instance = nullptr;

namespace {

QString qssColorValue(const QColor& color)
{
    if (color.alpha() < 255) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alpha());
    }
    return color.name(QColor::HexRgb);
}

} // namespace

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
    buildColorMaps();
}

void ThemeManager::applyTheme(AppTheme theme)
{
    if (m_styleSheetApplied && m_currentTheme == theme) {
        return;
    }

    m_currentTheme = theme;

    const auto resourcePath = theme == AppTheme::Light
        ? QStringLiteral(":/styles/light.qss")
        : QStringLiteral(":/styles/dark.qss");

    const auto qss = renderStyleSheet(loadStyleSheet(resourcePath));
    if (qApp != nullptr) {
        qApp->setStyleSheet(qss);

        // Force re-polish all top-level widgets so QSS re-evaluates
        for (auto* widget : qApp->topLevelWidgets()) {
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        }
    }
    m_styleSheetApplied = true;

    emit themeChanged(theme);
}

AppTheme ThemeManager::currentTheme() const
{
    return m_currentTheme;
}

void ThemeManager::toggleTheme()
{
    applyTheme(m_currentTheme == AppTheme::Light ? AppTheme::Dark : AppTheme::Light);
}

bool ThemeManager::isDark() const
{
    return m_currentTheme == AppTheme::Dark;
}

QColor ThemeManager::color(const QString& token) const
{
    const auto& map = m_currentTheme == AppTheme::Light ? m_lightColors : m_darkColors;
    const auto it = map.constFind(token);
    if (it != map.constEnd()) {
        return it.value();
    }

    // Try the other theme as fallback
    const auto& fallbackMap = m_currentTheme == AppTheme::Light ? m_darkColors : m_lightColors;
    const auto fallbackIt = fallbackMap.constFind(token);
    if (fallbackIt != fallbackMap.constEnd()) {
        return fallbackIt.value();
    }

    // Debug fallback: bright magenta to expose missing tokens
    return QColor("#FF00FF");
}

ThemeManager* ThemeManager::instance()
{
    return s_instance;
}

void ThemeManager::setInstance(ThemeManager* mgr)
{
    s_instance = mgr;
}

QString ThemeManager::loadStyleSheet(const QString& resourcePath) const
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    return stream.readAll();
}

QString ThemeManager::renderStyleSheet(QString styleSheet) const
{
    const auto& tokens = m_currentTheme == AppTheme::Light ? m_lightColors : m_darkColors;
    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it) {
        styleSheet.replace(QStringLiteral("${%1}").arg(it.key()), qssColorValue(it.value()));
    }
    return styleSheet;
}

void ThemeManager::buildColorMaps()
{
    // ── Light theme color tokens ─────────────────────────────
    m_lightColors = {
        // Surface
        {"window-bg",       QColor("#F5F6FA")},
        {"surface-0",       QColor("#FFFFFF")},
        {"surface-1",       QColor("#F8FAFC")},
        {"surface-2",       QColor("#F1F5F9")},
        {"surface-raised",  QColor("#FFFFFF")},

        // Legacy bg aliases
        {"bg-main",         QColor("#F5F6FA")},
        {"bg-panel",        QColor("#FFFFFF")},
        {"bg-panel-soft",   QColor("#F8FAFC")},
        {"bg-canvas",       QColor("#F7F8FB")},
        {"bg-input",        QColor("#FFFFFF")},

        // Text
        {"text-primary",    QColor("#0F172A")},
        {"text-secondary",  QColor("#475569")},
        {"text-muted",      QColor("#94A3B8")},
        {"text-disabled",   QColor("#CBD5E1")},
        {"text-on-accent",  QColor("#FFFFFF")},
        {"text-main",       QColor("#0F172A")},

        // Border
        {"border-subtle",   QColor("#E2E8F0")},
        {"border-default",  QColor("#CBD5E1")},
        {"border-strong",   QColor("#94A3B8")},
        {"border",          QColor("#E2E8F0")},
        {"border-strong",   QColor("#CBD5E1")},

        // Accent
        {"accent",          QColor("#2563EB")},
        {"accent-hover",    QColor("#1D4ED8")},
        {"accent-pressed",  QColor("#1E40AF")},
        {"accent-soft",     QColor("#DBEAFE")},
        {"accent-softer",   QColor("#EFF6FF")},
        {"primary",         QColor("#2563EB")},
        {"primary-hover",   QColor("#1D4ED8")},
        {"primary-soft",    QColor("#DBEAFE")},
        {"focus-ring",      QColor(37, 99, 235, 82)},

        // Semantic
        {"success",         QColor("#16A34A")},
        {"success-soft",    QColor("#DCFCE7")},
        {"warning",         QColor("#D97706")},
        {"warning-soft",    QColor("#FEF3C7")},
        {"danger",          QColor("#DC2626")},
        {"danger-hover",    QColor("#B91C1C")},
        {"danger-soft",     QColor("#FEE2E2")},
        {"queued",          QColor("#7C3AED")},
        {"queued-soft",     QColor("#EDE9FE")},

        // Node border
        {"node-border-idle",      QColor("#6B7280")},
        {"node-border-pending",   QColor("#9CA3AF")},
        {"node-border-queued",    QColor("#7C3AED")},
        {"node-border-running",   QColor("#2563EB")},
        {"node-border-succeeded", QColor("#16A34A")},
        {"node-border-failed",    QColor("#DC2626")},

        // Node fill
        {"node-fill-idle",      QColor("#FFFFFF")},
        {"node-fill-pending",   QColor("#F9FAFB")},
        {"node-fill-queued",    QColor("#F5F3FF")},
        {"node-fill-running",   QColor("#EFF6FF")},
        {"node-fill-succeeded", QColor("#F0FDF4")},
        {"node-fill-failed",    QColor("#FEF2F2")},

        // Node strip
        {"node-strip-idle",      QColor("#64748B")},
        {"node-strip-pending",   QColor("#9CA3AF")},
        {"node-strip-queued",    QColor("#7C3AED")},
        {"node-strip-running",   QColor("#2563EB")},
        {"node-strip-succeeded", QColor("#16A34A")},
        {"node-strip-failed",    QColor("#DC2626")},

        // Node text
        {"node-text-title",   QColor("#111827")},
        {"node-text-type",    QColor("#4B5563")},
        {"node-text-id",      QColor("#6B7280")},
        {"node-port-border",  QColor("#9CA3AF")},
        {"node-port-fill",    QColor("#FFFFFF")},
        {"node-glow-running", QColor(37, 99, 235, 44)},

        // Node glow
        {"canvas-node-glow-selected", QColor(37, 99, 235, 30)},

        // Edge
        {"edge-normal",   QColor("#64748B")},
        {"edge-hover",    QColor("#334155")},
        {"edge-selected", QColor("#2563EB")},
        {"edge-running",  QColor("#2563EB")},
        {"edge-failed",   QColor("#DC2626")},
        {"edge-preview",  QColor("#2563EB")},

        // Editor
        {"editor-line-bg",       QColor("#F3F4F6")},
        {"editor-line-text",     QColor("#6B7280")},
        {"editor-current-line",  QColor("#EFF6FF")},

        // Syntax
        {"syntax-keyword",  QColor("#7C3AED")},
        {"syntax-number",   QColor("#B45309")},
        {"syntax-function", QColor("#2563EB")},
        {"syntax-class",    QColor("#0F766E")},
        {"syntax-string",   QColor("#15803D")},
        {"syntax-comment",  QColor("#6B7280")},

        // Canvas
        {"canvas-bg",         QColor("#F7F8FB")},
        {"canvas-grid-major", QColor(100, 116, 139, 51)},
        {"canvas-grid-minor", QColor(100, 116, 139, 31)},

        // Overlay
        {"overlay-bg",                  QColor(15, 23, 42, 41)},
        {"overlay-text",               QColor("#FFFFFF")},
        {"overlay-btn-primary-bg",     QColor("#F8FAFC")},
        {"overlay-btn-primary-text",   QColor("#111827")},
        {"overlay-btn-secondary-bg",   QColor("#F8FAFC")},
        {"overlay-btn-secondary-text", QColor("#111827")},
        {"overlay-btn-secondary-border", QColor("#CBD5E1")},
    };

    // ── Dark theme color tokens ──────────────────────────────
    m_darkColors = {
        // Surface
        {"window-bg",       QColor("#0B1020")},
        {"surface-0",       QColor("#111827")},
        {"surface-1",       QColor("#172033")},
        {"surface-2",       QColor("#1E293B")},
        {"surface-raised",  QColor("#121A2A")},

        // Legacy bg aliases
        {"bg-main",         QColor("#0B1020")},
        {"bg-panel",        QColor("#111827")},
        {"bg-panel-soft",   QColor("#172033")},
        {"bg-canvas",       QColor("#0F172A")},
        {"bg-input",        QColor("#020617")},

        // Text
        {"text-primary",    QColor("#E5E7EB")},
        {"text-secondary",  QColor("#CBD5E1")},
        {"text-muted",      QColor("#64748B")},
        {"text-disabled",   QColor("#475569")},
        {"text-on-accent",  QColor("#FFFFFF")},
        {"text-main",       QColor("#E5E7EB")},

        // Border
        {"border-subtle",   QColor("#263244")},
        {"border-default",  QColor("#334155")},
        {"border-strong",   QColor("#475569")},
        {"border",          QColor("#263244")},
        {"border-strong",   QColor("#334155")},

        // Accent
        {"accent",          QColor("#60A5FA")},
        {"accent-hover",    QColor("#3B82F6")},
        {"accent-pressed",  QColor("#2563EB")},
        {"accent-soft",     QColor("#1E3A8A")},
        {"accent-softer",   QColor("#172554")},
        {"primary",         QColor("#60A5FA")},
        {"primary-hover",   QColor("#3B82F6")},
        {"primary-soft",    QColor("#1E3A8A")},
        {"focus-ring",      QColor(96, 165, 250, 92)},

        // Semantic
        {"success",         QColor("#22C55E")},
        {"success-soft",    QColor("#064E3B")},
        {"warning",         QColor("#F59E0B")},
        {"warning-soft",    QColor("#78350F")},
        {"danger",          QColor("#F87171")},
        {"danger-hover",    QColor("#EF4444")},
        {"danger-soft",     QColor("#7F1D1D")},
        {"queued",          QColor("#A78BFA")},
        {"queued-soft",     QColor("#4C1D95")},

        // Node border
        {"node-border-idle",      QColor("#94A3B8")},
        {"node-border-pending",   QColor("#64748B")},
        {"node-border-queued",    QColor("#A78BFA")},
        {"node-border-running",   QColor("#60A5FA")},
        {"node-border-succeeded", QColor("#22C55E")},
        {"node-border-failed",    QColor("#F87171")},

        // Node fill
        {"node-fill-idle",      QColor("#111827")},
        {"node-fill-pending",   QColor("#1E293B")},
        {"node-fill-queued",    QColor("#4C1D95")},
        {"node-fill-running",   QColor("#1E3A8A")},
        {"node-fill-succeeded", QColor("#064E3B")},
        {"node-fill-failed",    QColor("#7F1D1D")},

        // Node strip
        {"node-strip-idle",      QColor("#94A3B8")},
        {"node-strip-pending",   QColor("#64748B")},
        {"node-strip-queued",    QColor("#A78BFA")},
        {"node-strip-running",   QColor("#60A5FA")},
        {"node-strip-succeeded", QColor("#22C55E")},
        {"node-strip-failed",    QColor("#F87171")},

        // Node text
        {"node-text-title",   QColor("#E5E7EB")},
        {"node-text-type",    QColor("#CBD5E1")},
        {"node-text-id",      QColor("#64748B")},
        {"node-port-border",  QColor("#475569")},
        {"node-port-fill",    QColor("#1E293B")},
        {"node-glow-running", QColor(96, 165, 250, 44)},

        // Node glow
        {"canvas-node-glow-selected", QColor(96, 165, 250, 35)},

        // Edge
        {"edge-normal",   QColor("#94A3B8")},
        {"edge-hover",    QColor("#CBD5E1")},
        {"edge-selected", QColor("#60A5FA")},
        {"edge-running",  QColor("#38BDF8")},
        {"edge-failed",   QColor("#F87171")},
        {"edge-preview",  QColor("#60A5FA")},

        // Editor
        {"editor-line-bg",       QColor("#1E293B")},
        {"editor-line-text",     QColor("#64748B")},
        {"editor-current-line",  QColor("#1E3A8A")},

        // Syntax
        {"syntax-keyword",  QColor("#A78BFA")},
        {"syntax-number",   QColor("#FBBF24")},
        {"syntax-function", QColor("#60A5FA")},
        {"syntax-class",    QColor("#34D399")},
        {"syntax-string",   QColor("#4ADE80")},
        {"syntax-comment",  QColor("#64748B")},

        // Canvas
        {"canvas-bg",         QColor("#0F172A")},
        {"canvas-grid-major", QColor(148, 163, 184, 41)},
        {"canvas-grid-minor", QColor(148, 163, 184, 26)},

        // Overlay
        {"overlay-bg",                  QColor(0, 0, 0, 97)},
        {"overlay-text",               QColor("#E5E7EB")},
        {"overlay-btn-primary-bg",     QColor("#3B82F6")},
        {"overlay-btn-primary-text",   QColor("#FFFFFF")},
        {"overlay-btn-secondary-bg",   QColor("#1E293B")},
        {"overlay-btn-secondary-text", QColor("#E5E7EB")},
        {"overlay-btn-secondary-border", QColor("#334155")},
    };
}

} // namespace vws::ui
