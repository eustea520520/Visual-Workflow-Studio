#include "ui/widgets/IconSquareButton.h"
#include "ui/theme/StyleReloader.h"
#include "ui/theme/ThemeManager.h"

#include <QPainter>
#include <QPixmap>
#include <QStyle>

namespace vws::ui {

namespace {

constexpr int ButtonSize = 38;
constexpr int IconSize = 18;

QString rolePropertyValue(IconSquareButton::Role role)
{
    switch (role) {
    case IconSquareButton::Role::Primary:   return QStringLiteral("primary");
    case IconSquareButton::Role::Danger:    return QStringLiteral("danger");
    case IconSquareButton::Role::Ghost:     return QStringLiteral("ghost");
    case IconSquareButton::Role::Secondary:
    default:                                return QStringLiteral("secondary");
    }
}

QPixmap tintPixmap(const QPixmap& source, const QColor& color)
{
    QPixmap tinted(source.size());
    tinted.fill(Qt::transparent);

    QPainter painter(&tinted);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Draw the source pixmap
    painter.drawPixmap(0, 0, source);

    // Use SourceIn composition to colorize
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();

    return tinted;
}

} // namespace

IconSquareButton::IconSquareButton(QWidget* parent)
    : QPushButton(parent)
{
    applyCommonProperties();
}

IconSquareButton::IconSquareButton(const QIcon& icon, const QString& tooltip,
                                   QWidget* parent)
    : QPushButton(parent)
{
    setToolTip(tooltip);
    m_sourceIcon = icon;
    m_hasSourceIcon = true;
    applyCommonProperties();
    refreshIcon();

    // Re-tint on theme change
    if (auto* tm = ThemeManager::instance()) {
        connect(tm, &ThemeManager::themeChanged, this, [this](AppTheme) {
            refreshIcon();
        });
    }
}

void IconSquareButton::setRole(Role role)
{
    if (m_role == role) {
        return;
    }

    m_role = role;
    setProperty("buttonRole", rolePropertyValue(role));
    StyleReloader::refresh(this);

    if (m_hasSourceIcon) {
        refreshIcon();
    }
}

IconSquareButton::Role IconSquareButton::role() const
{
    return m_role;
}

void IconSquareButton::refreshIcon()
{
    if (!m_hasSourceIcon || m_sourceIcon.isNull()) {
        return;
    }

    const QSize pxSize(IconSize * 2, IconSize * 2);
    const auto sourcePx = m_sourceIcon.pixmap(pxSize);
    if (sourcePx.isNull()) {
        return;
    }

    const auto color = iconColor();
    const auto tinted = tintPixmap(sourcePx, color);
    setIcon(QIcon(tinted));
}

QColor IconSquareButton::iconColor() const
{
    auto* tm = ThemeManager::instance();
    const bool isDark = tm ? tm->isDark() : false;

    if (!isEnabled()) {
        return tm ? tm->color("text-disabled") : QColor("#CBD5E1");
    }

    switch (m_role) {
    case Role::Primary:
        // Primary buttons have blue background, always use white
        return QColor("#FFFFFF");
    case Role::Danger:
        // Danger buttons have red background, matching Primary's fixed white icon treatment.
        return QColor("#FFFFFF");
    case Role::Ghost:
        return tm ? tm->color("text-secondary") : QColor("#475569");
    case Role::Secondary:
    default:
        return isDark ? QColor("#CBD5E1") : QColor("#334155");
    }
}

void IconSquareButton::applyCommonProperties()
{
    setFixedSize(ButtonSize, ButtonSize);
    setIconSize(QSize(IconSize, IconSize));
    setProperty("iconSquare", true);
    setProperty("buttonRole", rolePropertyValue(m_role));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
}

} // namespace vws::ui
