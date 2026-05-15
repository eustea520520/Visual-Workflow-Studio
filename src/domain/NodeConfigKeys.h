#pragma once

#include <QString>

namespace vws::domain::NodeConfigKeys {

inline const QString Language = QStringLiteral("language");
inline const QString Entry = QStringLiteral("entry");
inline const QString Code = QStringLiteral("code");
inline const QString IoTemplate = QStringLiteral("io_template");

inline const QString AgentUrl = QStringLiteral("agent_url");
inline const QString AgentModel = QStringLiteral("agent_model");
inline const QString AgentApiKey = QStringLiteral("agent_api_key");
inline const QString AgentMaxRetries = QStringLiteral("agent_max_retries");
inline const QString AgentBackgroundPrompt = QStringLiteral("agent_background_prompt");
inline const QString AgentTaskPrompt = QStringLiteral("agent_task_prompt");

} // namespace vws::domain::NodeConfigKeys
