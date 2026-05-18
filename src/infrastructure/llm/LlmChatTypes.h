#pragma once

#include <QString>

namespace vws::infrastructure {

struct LlmProviderSettings {
    QString url;
    QString modelName;
    QString apiKey;
    int timeoutMs = 120000;
};

struct LlmChatRequest {
    QString url;
    QString modelName;
    QString apiKey;
    QString systemPrompt;
    QString userPrompt;
    int timeoutMs = 120000;
};

struct LlmChatResponse {
    bool success = false;
    QString content;
    QString errorMessage;
    int httpStatus = 0;
};

} // namespace vws::infrastructure
