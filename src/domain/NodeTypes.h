#pragma once

#include <QString>

namespace vws::domain::NodeTypes {

inline const QString Starter = QStringLiteral("starter");
inline const QString Function = QStringLiteral("function");
inline const QString Agent = QStringLiteral("agent");
inline const QString Subsystem = QStringLiteral("subsystem");
inline const QString Loop = QStringLiteral("loop");

inline bool isPythonBacked(const QString& nodeType)
{
    const auto normalized = nodeType.trimmed().toLower();
    return normalized == Starter || normalized == Function || normalized == Agent || normalized == Loop;
}

} // namespace vws::domain::NodeTypes
