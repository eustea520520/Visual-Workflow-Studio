#pragma once

#include "infrastructure/llm/LlmChatTypes.h"

#include <QNetworkAccessManager>
#include <QObject>

#include <functional>

namespace vws::infrastructure {

class LlmChatClient final : public QObject {
    Q_OBJECT

public:
    explicit LlmChatClient(QObject* parent = nullptr);

    void sendChatCompletion(
        const LlmChatRequest& request,
        QObject* receiver,
        std::function<void(LlmChatResponse)> callback);

private:
    QUrl completionUrl(const QString& url) const;
    QNetworkRequest buildRequest(const LlmChatRequest& request, bool includeResponseFormat) const;
    QByteArray buildPayload(const LlmChatRequest& request, bool includeResponseFormat) const;
    void sendAttempt(
        const LlmChatRequest& request,
        QObject* receiver,
        std::function<void(LlmChatResponse)> callback,
        bool includeResponseFormat);

    QNetworkAccessManager m_network;
};

} // namespace vws::infrastructure
