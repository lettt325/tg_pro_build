/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "pro/ai/deepseek_client.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <crl/crl_on_main.h>

namespace ProAI {
namespace {

constexpr auto kApiUrl = "https://api.deepseek.com/chat/completions";

} // namespace

DeepSeekClient::DeepSeekClient(
	const QString &apiToken,
	const QString &model)
: _apiToken(apiToken)
, _model(model) {
}

DeepSeekClient::~DeepSeekClient() {
	const auto destroy = std::move(_old);
}

void DeepSeekClient::setApiToken(const QString &token) {
	_apiToken = token;
}

void DeepSeekClient::setModel(const QString &model) {
	_model = model;
}

void DeepSeekClient::setThinkingEnabled(bool enabled) {
	_thinkingEnabled = enabled;
}

void DeepSeekClient::chat(
		const QString &systemPrompt,
		const std::vector<Message> &messages,
		ChatCallback callback) {
	auto messagesArray = QJsonArray();
	if (!systemPrompt.isEmpty()) {
		messagesArray.append(QJsonObject{
			{ "role", "system" },
			{ "content", systemPrompt },
		});
	}
	for (const auto &msg : messages) {
		messagesArray.append(QJsonObject{
			{ "role", msg.role },
			{ "content", msg.content },
		});
	}

	const auto model = _model.isEmpty()
		? u"deepseek-v4-flash"_q
		: _model;
	auto body = QJsonObject{
		{ "model", model },
		{ "messages", messagesArray },
		{ "stream", false },
	};
	body["thinking"] = QJsonObject{
		{ "type", _thinkingEnabled
			? u"enabled"_q
			: u"disabled"_q },
	};
	const auto data = QJsonDocument(body).toJson(QJsonDocument::Compact);

	auto request = QNetworkRequest(QUrl(kApiUrl));
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		"application/json");
	request.setRawHeader(
		"Authorization",
		("Bearer " + _apiToken).toUtf8());

	destroyReplyDelayed(std::move(_reply));
	_reply.reset(_manager.post(request, data));

	const auto finish = [=](ChatResponse response, Error error) {
		crl::on_main([
			callback,
			response = std::move(response),
			error = std::move(error)
		] {
			callback(std::move(response), std::move(error));
		});
	};

	QObject::connect(_reply.get(), &QNetworkReply::finished, [=] {
		const auto replyError = int(_reply->error());
		const auto replyErrorString = _reply->errorString();
		const auto bytes = _reply->readAll();
		destroyReplyDelayed(std::move(_reply));

		if (bytes.isEmpty() && replyError != QNetworkReply::NoError) {
			finish({}, Error{
				Error::Code::Network,
				replyErrorString,
			});
			return;
		}

		auto parseError = QJsonParseError();
		const auto doc = QJsonDocument::fromJson(bytes, &parseError);
		if (parseError.error != QJsonParseError::NoError) {
			finish({}, Error{
				Error::Code::JsonParse,
				parseError.errorString(),
			});
			return;
		}

		const auto obj = doc.object();
		if (obj.contains("error")) {
			const auto err = obj.value("error").toObject();
			finish({}, Error{
				Error::Code::ApiError,
				err.value("message").toString(),
			});
			return;
		}

		if (replyError != QNetworkReply::NoError) {
			finish({}, Error{
				Error::Code::Network,
				replyErrorString,
			});
			return;
		}

		const auto choices = obj.value("choices").toArray();
		if (choices.isEmpty()) {
			finish({}, Error{
				Error::Code::JsonFormat,
				"No choices in response.",
			});
			return;
		}

		const auto message = choices.first()
			.toObject().value("message").toObject();
		const auto usage = obj.value("usage").toObject();

		finish(ChatResponse{
			.content = message.value("content").toString(),
			.promptTokens = usage.value("prompt_tokens").toInt(),
			.completionTokens = usage.value("completion_tokens").toInt(),
		}, Error{});
	});
}

void DeepSeekClient::chatWithTools(
		const QJsonArray &messages,
		const QJsonArray &tools,
		ChatWithToolsCallback callback) {
	const auto model = _model.isEmpty()
		? u"deepseek-v4-flash"_q
		: _model;
	auto body = QJsonObject{
		{ "model", model },
		{ "messages", messages },
		{ "stream", false },
		{ "tools", tools },
	};
	// Thinking mode must be disabled when using tool calling
	body["thinking"] = QJsonObject{
		{ "type", u"disabled"_q },
	};

	const auto data = QJsonDocument(body).toJson(QJsonDocument::Compact);

	auto request = QNetworkRequest(QUrl(kApiUrl));
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		"application/json");
	request.setRawHeader(
		"Authorization",
		("Bearer " + _apiToken).toUtf8());

	destroyReplyDelayed(std::move(_reply));
	_reply.reset(_manager.post(request, data));

	const auto finish = [=](ChatResponseWithTools response, Error error) {
		crl::on_main([
			callback,
			response = std::move(response),
			error = std::move(error)
		] {
			callback(std::move(response), std::move(error));
		});
	};

	QObject::connect(_reply.get(), &QNetworkReply::finished, [=] {
		const auto replyError = int(_reply->error());
		const auto replyErrorString = _reply->errorString();
		const auto bytes = _reply->readAll();
		destroyReplyDelayed(std::move(_reply));

		if (bytes.isEmpty() && replyError != QNetworkReply::NoError) {
			finish({}, Error{ Error::Code::Network, replyErrorString });
			return;
		}

		auto parseError = QJsonParseError();
		const auto doc = QJsonDocument::fromJson(bytes, &parseError);
		if (parseError.error != QJsonParseError::NoError) {
			finish({}, Error{ Error::Code::JsonParse, parseError.errorString() });
			return;
		}

		const auto obj = doc.object();
		if (obj.contains("error")) {
			const auto err = obj.value("error").toObject();
			finish({}, Error{ Error::Code::ApiError, err.value("message").toString() });
			return;
		}
		if (replyError != QNetworkReply::NoError) {
			finish({}, Error{ Error::Code::Network, replyErrorString });
			return;
		}

		const auto choices = obj.value("choices").toArray();
		if (choices.isEmpty()) {
			finish({}, Error{ Error::Code::JsonFormat, "No choices in response." });
			return;
		}

		const auto message = choices.first()
			.toObject().value("message").toObject();
		const auto usage = obj.value("usage").toObject();

		auto response = ChatResponseWithTools{
			.content = message.value("content").toString(),
			.promptTokens = usage.value("prompt_tokens").toInt(),
			.completionTokens = usage.value("completion_tokens").toInt(),
		};

		const auto toolCallsArr = message.value("tool_calls").toArray();
		for (const auto &tc : toolCallsArr) {
			const auto tcObj = tc.toObject();
			const auto fn = tcObj.value("function").toObject();
			auto argsDoc = QJsonDocument::fromJson(
				fn.value("arguments").toString().toUtf8());
			response.toolCalls.push_back(ToolCall{
				.id = tcObj.value("id").toString(),
				.functionName = fn.value("name").toString(),
				.arguments = argsDoc.isObject()
					? argsDoc.object()
					: QJsonObject(),
			});
		}

		finish(std::move(response), Error{});
	});
}

void DeepSeekClient::cancel() {
	destroyReplyDelayed(std::move(_reply));
}

void DeepSeekClient::destroyReplyDelayed(
		std::unique_ptr<QNetworkReply> reply) {
	if (!reply) {
		return;
	}
	const auto raw = reply.get();
	_old.push_back(std::move(reply));
	QObject::disconnect(raw, &QNetworkReply::finished, nullptr, nullptr);
	raw->deleteLater();
	QObject::connect(raw, &QObject::destroyed, [=] {
		for (auto i = begin(_old); i != end(_old); ++i) {
			if (i->get() == raw) {
				i->release();
				_old.erase(i);
				break;
			}
		}
	});
}

} // namespace ProAI
