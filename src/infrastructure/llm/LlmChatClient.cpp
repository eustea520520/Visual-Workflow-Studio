#include "infrastructure/llm/LlmChatClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUrl>

namespace vws::infrastructure {

LlmChatClient::LlmChatClient(QObject* parent)
    : QObject(parent)
{
}

void LlmChatClient::sendChatCompletion(
    const LlmChatRequest& request,
    QObject* receiver,
    std::function<void(LlmChatResponse)> callback)
{
    sendAttempt(request, receiver, std::move(callback), true);
}

QUrl LlmChatClient::completionUrl(const QString& url) const
{
    auto normalized = url.trimmed();
    if (!normalized.endsWith("/chat/completions")) {
        normalized = normalized.endsWith('/')
            ? normalized + QStringLiteral("chat/completions")
            : normalized + QStringLiteral("/chat/completions");
    }
    return QUrl(normalized);
}

QNetworkRequest LlmChatClient::buildRequest(const LlmChatRequest& request, bool includeResponseFormat) const
{
    Q_UNUSED(includeResponseFormat);

    QNetworkRequest networkRequest(completionUrl(request.url));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    networkRequest.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(request.apiKey).toUtf8());
    networkRequest.setTransferTimeout(request.timeoutMs);
    return networkRequest;
}

QByteArray LlmChatClient::buildPayload(const LlmChatRequest& request, bool includeResponseFormat) const
{
    QJsonObject object;
    object.insert("model", request.modelName);
    object.insert("temperature", 0.2);
    object.insert("messages", QJsonArray{
        QJsonObject{{"role", "system"}, {"content", request.systemPrompt}},
        QJsonObject{{"role", "user"}, {"content", request.userPrompt}},
    });
    if (includeResponseFormat) {
        object.insert("response_format", QJsonObject{{"type", "json_object"}});
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void LlmChatClient::sendAttempt(
    const LlmChatRequest& request,
    QObject* receiver,
    std::function<void(LlmChatResponse)> callback,
    bool includeResponseFormat)
{
    QPointer<QObject> guardedReceiver(receiver);
    auto* reply = m_network.post(buildRequest(request, includeResponseFormat), buildPayload(request, includeResponseFormat));
    reply->setParent(this);

    connect(reply, &QNetworkReply::finished, this, [this, reply, guardedReceiver, callback = std::move(callback), request, includeResponseFormat]() mutable {
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->readAll();
        const auto networkError = reply->error();
        const auto errorText = reply->errorString();
        reply->deleteLater();

        if (guardedReceiver.isNull()) {
            return;
        }

        if (networkError != QNetworkReply::NoError) {
            const auto body = QString::fromUtf8(bytes);
            if (includeResponseFormat && body.contains("response_format", Qt::CaseInsensitive)) {
                sendAttempt(request, guardedReceiver, std::move(callback), false);
                return;
            }

            callback({false, {}, QStringLiteral("LLM request failed: HTTP %1 %2 %3").arg(status).arg(errorText, body), status});
            return;
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            callback({false, {}, QStringLiteral("LLM response was not JSON: %1").arg(parseError.errorString()), status});
            return;
        }

        const auto choices = document.object().value("choices").toArray();
        if (choices.isEmpty()) {
            callback({false, {}, QStringLiteral("LLM response did not contain choices."), status});
            return;
        }

        const auto content = choices.first().toObject().value("message").toObject().value("content").toString();
        if (content.trimmed().isEmpty()) {
            callback({false, {}, QStringLiteral("LLM response content was empty."), status});
            return;
        }

        callback({true, content, {}, status});
    });
}

} // namespace vws::infrastructure
