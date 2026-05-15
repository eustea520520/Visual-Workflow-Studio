#pragma once

#include <QString>

namespace vws::ui {

struct AgentInspectorViewModel {
    QString title;
    QString description;
    QString timeoutMs;
    QString ioTemplate;
    QString url;
    QString modelName;
    QString apiKey;
    QString maxRetries;
    QString backgroundPrompt;
    QString taskPrompt;
};

struct NodeInspectorViewModel {
    QString nodeId;
    QString code;
    QString outputJsonText;
    bool showAgentTab = false;
    bool focusAgentTab = false;
    AgentInspectorViewModel agent;
};

} // namespace vws::ui
