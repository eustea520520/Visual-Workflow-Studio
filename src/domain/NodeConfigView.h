#pragma once

#include "domain/NodeConfigKeys.h"

#include <QJsonObject>
#include <QString>

namespace vws::domain {

class NodeConfigView {
public:
    explicit NodeConfigView(const QJsonObject& config)
        : m_config(config)
    {
    }

    QString language(const QString& fallback = QStringLiteral("python")) const
    {
        return m_config.value(NodeConfigKeys::Language).toString(fallback);
    }

    QString entry(const QString& fallback = QStringLiteral("run")) const
    {
        return m_config.value(NodeConfigKeys::Entry).toString(fallback);
    }

    QString code() const
    {
        return m_config.value(NodeConfigKeys::Code).toString();
    }

    QString ioTemplate(const QString& fallback = QStringLiteral("data_to_data")) const
    {
        return m_config.value(NodeConfigKeys::IoTemplate).toString(fallback);
    }

    QString agentUrl() const
    {
        return m_config.value(NodeConfigKeys::AgentUrl).toString();
    }

    QString agentModel() const
    {
        return m_config.value(NodeConfigKeys::AgentModel).toString();
    }

    QString agentApiKey() const
    {
        return m_config.value(NodeConfigKeys::AgentApiKey).toString();
    }

    int agentMaxRetries(int fallback) const
    {
        return m_config.value(NodeConfigKeys::AgentMaxRetries).toInt(fallback);
    }

    QString agentBackgroundPrompt(const QString& fallback = {}) const
    {
        return m_config.value(NodeConfigKeys::AgentBackgroundPrompt).toString(fallback);
    }

    QString agentTaskPrompt(const QString& fallback = {}) const
    {
        return m_config.value(NodeConfigKeys::AgentTaskPrompt).toString(fallback);
    }

    int loopIterations(int fallback = 0) const
    {
        return m_config.value(NodeConfigKeys::LoopIterations).toInt(fallback);
    }

private:
    const QJsonObject& m_config;
};

} // namespace vws::domain
