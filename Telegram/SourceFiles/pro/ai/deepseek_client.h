/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkAccessManager>
#include <functional>
#include <memory>
#include <vector>

class QNetworkReply;

namespace ProAI {

struct Message {
	QString role;
	QString content;
};

struct ChatResponse {
	QString content;
	int promptTokens = 0;
	int completionTokens = 0;
};

struct Error {
	enum class Code {
		None,
		Network,
		JsonParse,
		JsonFormat,
		ApiError,
	};
	Code code = Code::None;
	QString message;

	explicit operator bool() const {
		return code != Code::None;
	}
};

struct ToolCall {
	QString id;
	QString functionName;
	QJsonObject arguments;
};

struct ChatResponseWithTools {
	QString content;
	std::vector<ToolCall> toolCalls;
	int promptTokens = 0;
	int completionTokens = 0;
	[[nodiscard]] bool hasToolCalls() const { return !toolCalls.empty(); }
};

using ChatCallback = std::function<void(ChatResponse, Error)>;
using ChatWithToolsCallback = std::function<void(ChatResponseWithTools, Error)>;

class DeepSeekClient final {
public:
	DeepSeekClient(const QString &apiToken, const QString &model);
	~DeepSeekClient();

	void setApiToken(const QString &token);
	void setModel(const QString &model);

	void setThinkingEnabled(bool enabled);

	void chat(
		const QString &systemPrompt,
		const std::vector<Message> &messages,
		ChatCallback callback);

	void chatWithTools(
		const QJsonArray &messages,
		const QJsonArray &tools,
		ChatWithToolsCallback callback);

	void cancel();

private:
	void destroyReplyDelayed(std::unique_ptr<QNetworkReply> reply);

	QString _apiToken;
	QString _model;
	bool _thinkingEnabled = false;
	QNetworkAccessManager _manager;
	std::unique_ptr<QNetworkReply> _reply;
	std::vector<std::unique_ptr<QNetworkReply>> _old;
};

} // namespace ProAI
