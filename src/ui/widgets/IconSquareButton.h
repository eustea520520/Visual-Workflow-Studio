#pragma once

#include <QIcon>
#include <QPushButton>

namespace vws::ui {

class IconSquareButton : public QPushButton {
    Q_OBJECT

public:
    enum class Role {
        Primary,
        Secondary,
        Danger,
        Ghost
    };
    Q_ENUM(Role)

    explicit IconSquareButton(QWidget* parent = nullptr);
    explicit IconSquareButton(const QIcon& icon, const QString& tooltip,
                              QWidget* parent = nullptr);

    void setRole(Role role);
    Role role() const;
    void refreshIcon();

private:
    void applyCommonProperties();
    QColor iconColor() const;

    Role m_role = Role::Secondary;
    QIcon m_sourceIcon;
    bool m_hasSourceIcon = false;
};

} // namespace vws::ui
