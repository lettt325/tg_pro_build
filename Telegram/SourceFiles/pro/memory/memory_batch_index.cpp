/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "pro/memory/memory_batch_index.h"

#include "pro/ai/deepseek_client.h"
#include "pro/memory/memory_storage.h"
#include "settings/pro/pro_settings_storage.h"
#include "base/unixtime.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace ProMemory {
namespace {

constexpr auto kChunkMaxMessages = 30;
constexpr auto kChunkMaxChars = 4000;

QJsonArray MakeToolsDefinition() {
	return QJsonArray{
		QJsonObject{
			{ "type", "function" },
			{ "function", QJsonObject{
				{ "name", "create_memory" },
				{ "description",
					"Save a structured memory fact about the contact" },
				{ "parameters", QJsonObject{
					{ "type", "object" },
					{ "properties", QJsonObject{
						{ "fact", QJsonObject{
							{ "type", "string" },
							{ "description",
								"The memory fact" },
						}},
						{ "category", QJsonObject{
							{ "type", "string" },
							{ "enum", QJsonArray{
								"personal", "work", "health",
								"family", "preferences", "plans",
								"events", "emotions", "other",
							}},
							{ "description", "Category" },
						}},
						{ "importance", QJsonObject{
							{ "type", "string" },
							{ "enum", QJsonArray{
								"high", "medium", "low",
							}},
							{ "description", "Importance level" },
						}},
						{ "date_context", QJsonObject{
							{ "type", "string" },
							{ "description",
								"Date context if available" },
						}},
						{ "source_message_index", QJsonObject{
							{ "type", "integer" },
							{ "description",
								"Index of source message (0-based)" },
						}},
						{ "related_to", QJsonObject{
							{ "type", "string" },
							{ "description",
								"Person/entity this relates to" },
						}},
					}},
					{ "required", QJsonArray{
						"fact", "category", "importance",
					}},
				}},
			}},
		},
	};
}

QString SystemPromptRu(const QString &role) {
	return u"Ты — агент по извлечению памяти из переписки.\n"_q
		+ (role.isEmpty()
			? QString()
			: (u"Контакт: "_q + role + u".\n"_q))
		+ u"\nПроанализируй сообщения и вызови create_memory для "
		"КАЖДОГО значимого факта.\n"
		"Извлекай ВСЕ: имена, даты, события, предпочтения, привычки, "
		"планы, эмоции, здоровье, семью, работу, хобби, питомцев — "
		"любую конкретную деталь.\n\n"
		"Указывай source_message_index чтобы связать факт с сообщением.\n"
		"Используй related_to для имён людей/питомцев/мест.\n"
		"Ставь importance=high для дат рождения, имён, ключевых событий.\n\n"
		"Цель: создать максимально детальное досье."_q;
}

QString SystemPromptEn(const QString &role) {
	return u"You are a memory extraction agent analyzing a conversation.\n"_q
		+ (role.isEmpty()
			? QString()
			: (u"Contact: "_q + role + u".\n"_q))
		+ u"\nAnalyze the messages and call create_memory for EVERY "
		"significant fact.\n"
		"Extract ALL: names, dates, events, preferences, habits, "
		"plans, emotions, health, family, work, hobbies, pets — "
		"any concrete detail.\n\n"
		"Use source_message_index to link facts to specific messages.\n"
		"Use related_to for names of people/pets/places.\n"
		"Set importance=high for birthdays, names, key events.\n\n"
		"Goal: create the most detailed dossier possible."_q;
}

struct Chunk {
	QString text;
	int messageCount = 0;
};

std::vector<Chunk> FormatAndChunk(
		const std::vector<not_null<HistoryItem*>> &items,
		not_null<Main::Session*> session) {
	auto chunks = std::vector<Chunk>();
	auto current = QString();
	auto count = 0;

	for (auto i = 0; i < int(items.size()); ++i) {
		const auto item = items[i];
		const auto date = QDateTime::fromSecsSinceEpoch(
			item->date()).toString(u"yyyy-MM-dd hh:mm"_q);
		const auto sender = item->out()
			? u"Me"_q
			: (item->from() ? item->from()->name() : u"Unknown"_q);
		auto text = item->originalText().text;
		if (text.isEmpty()) {
			text = u"[media]"_q;
		}

		auto line = u"[MSG #%1] [%2] %3: %4"_q
			.arg(i).arg(date).arg(sender).arg(text);

		if (const auto reply = item->Get<HistoryMessageReply>()) {
			if (const auto replyMsg = reply->resolvedMessage.get()) {
				auto replyText = replyMsg->originalText().text;
				if (replyText.length() > 80) {
					replyText = replyText.left(77) + u"..."_q;
				}
				line += u"\n  ↳ reply to: \""_q + replyText + u"\""_q;
			}
		}
		line += u"\n"_q;

		if (count >= kChunkMaxMessages
				|| (current.length() + line.length() > kChunkMaxChars
					&& count > 0)) {
			chunks.push_back({ std::move(current), count });
			current = QString();
			count = 0;
		}
		current += line;
		++count;
	}
	if (count > 0) {
		chunks.push_back({ std::move(current), count });
	}
	return chunks;
}

void RunIndexing(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		uint64 peerId,
		const QString &role,
		std::vector<Chunk> chunks,
		Fn<void()> done) {
	struct State {
		std::unique_ptr<ProAI::DeepSeekClient> client;
		QJsonArray tools;
		int totalChunks = 0;
		int currentChunk = 0;
		int extractedCount = 0;
		bool cancelled = false;
		Ui::FlatLabel *statusLabel = nullptr;
		Ui::VerticalLayout *logArea = nullptr;
	};
	const auto state = box->lifetime().make_state<State>();

	const auto &pro = session->proStorage();
	state->client = std::make_unique<ProAI::DeepSeekClient>(
		pro.deepseekApiToken(),
		pro.deepseekModel());
	state->tools = MakeToolsDefinition();
	state->totalChunks = int(chunks.size());

	state->statusLabel = box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Starting..."_q),
			st::boxLabel));

	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Extracted facts:"_q),
			st::boxLabel),
		QMargins(0, 8, 0, 4));

	state->logArea = box->addRow(
		object_ptr<Ui::VerticalLayout>(box));

	box->addButton(rpl::single(u"Cancel"_q), [=] {
		state->cancelled = true;
		if (state->client) state->client->cancel();
		box->closeBox();
	});

	const auto lang = pro.memoryLanguage();
	const auto systemPrompt = (lang == u"ru"_q)
		? SystemPromptRu(role)
		: SystemPromptEn(role);

	const auto processChunk = std::make_shared<
		std::function<void()>>();
	const auto processToolCalls = std::make_shared<
		std::function<void(QJsonArray)>>();
	const auto guard = QPointer<Ui::GenericBox>(box);

	*processChunk = [=, chunks = std::move(chunks)]() {
		if (!guard || state->cancelled) return;
		if (state->currentChunk >= state->totalChunks) {
			state->statusLabel->setText(
				u"Done! Extracted "_q
				+ QString::number(state->extractedCount)
				+ u" memories."_q);
			box->clearButtons();
			box->addButton(rpl::single(u"Close"_q), [=] {
				if (done) done();
				box->closeBox();
			});
			return;
		}

		state->statusLabel->setText(
			u"Chunk "_q
			+ QString::number(state->currentChunk + 1)
			+ u"/"_q
			+ QString::number(state->totalChunks)
			+ u" — processing..."_q);

		auto messages = QJsonArray();
		messages.append(QJsonObject{
			{ "role", "system" },
			{ "content", systemPrompt },
		});
		messages.append(QJsonObject{
			{ "role", "user" },
			{ "content", chunks[state->currentChunk].text },
		});

		(*processToolCalls)(std::move(messages));
	};

	*processToolCalls = [=](QJsonArray messages) {
		if (!guard || state->cancelled) return;

		state->client->chatWithTools(messages, state->tools,
			[=, messages = std::move(messages)](
				ProAI::ChatResponseWithTools response,
				ProAI::Error error) mutable {
			if (!guard || state->cancelled) return;
			if (error) {
				state->logArea->add(
					object_ptr<Ui::FlatLabel>(
						state->logArea,
						rpl::single(u"Error: "_q + error.message),
						st::boxLabel),
					QMargins(0, 2, 0, 0));
				state->logArea->resizeToWidth(
					state->logArea->width());
				state->currentChunk++;
				(*processChunk)();
				return;
			}

			if (response.hasToolCalls()) {
				auto assistantToolCalls = QJsonArray();
				for (const auto &tc : response.toolCalls) {
					const auto args = tc.arguments;
					const auto fact = args.value("fact").toString();
					const auto category =
						args.value("category").toString();
					const auto importance =
						args.value("importance").toString();
					const auto dateCtx =
						args.value("date_context").toString();
					const auto relatedTo =
						args.value("related_to").toString();

					if (!fact.isEmpty()) {
						session->memoryStorage().addEntry(
							peerId,
							fact,
							Source::Message,
							0,
							base::unixtime::now(),
							QStringList{ u"indexed"_q },
							category,
							importance,
							dateCtx,
							relatedTo);
						state->extractedCount++;

						auto label = u"["_q + importance
							+ u"] "_q + category
							+ u": "_q + fact;
						if (!relatedTo.isEmpty()) {
							label += u" → "_q + relatedTo;
						}
						state->logArea->add(
							object_ptr<Ui::FlatLabel>(
								state->logArea,
								rpl::single(label),
								st::defaultFlatLabel),
							QMargins(0, 2, 0, 0));
					}

					assistantToolCalls.append(QJsonObject{
						{ "id", tc.id },
						{ "type", "function" },
						{ "function", QJsonObject{
							{ "name", tc.functionName },
							{ "arguments",
								QString::fromUtf8(
									QJsonDocument(args).toJson(
										QJsonDocument::Compact)) },
						}},
					});
				}

				messages.append(QJsonObject{
					{ "role", "assistant" },
					{ "tool_calls", assistantToolCalls },
				});
				for (const auto &tc : response.toolCalls) {
					messages.append(QJsonObject{
						{ "role", "tool" },
						{ "tool_call_id", tc.id },
						{ "content", "Saved." },
					});
				}

				state->logArea->resizeToWidth(
					state->logArea->width());

				(*processToolCalls)(std::move(messages));
			} else {
				// Fallback: parse plain text "- fact" lines
				if (!response.content.isEmpty()) {
					for (const auto &line
							: response.content.split('\n')) {
						auto trimmed = line.trimmed();
						if (trimmed.startsWith(u"- "_q)) {
							trimmed = trimmed.mid(2).trimmed();
							if (!trimmed.isEmpty()) {
								session->memoryStorage().addEntry(
									peerId,
									trimmed,
									Source::Message,
									0,
									base::unixtime::now(),
									QStringList{ u"indexed"_q });
								state->extractedCount++;
								state->logArea->add(
									object_ptr<Ui::FlatLabel>(
										state->logArea,
										rpl::single(trimmed),
										st::defaultFlatLabel),
									QMargins(0, 2, 0, 0));
							}
						}
					}
					state->logArea->resizeToWidth(
						state->logArea->width());
				}
				state->currentChunk++;
				(*processChunk)();
			}
		});
	};

	(*processChunk)();
}

} // namespace

void ShowBatchIndexBox(
		not_null<Window::SessionController*> controller,
		not_null<Main::Session*> session,
		uint64 peerId,
		std::vector<not_null<HistoryItem*>> items,
		Fn<void()> done) {
	const auto &pro = session->proStorage();
	if (pro.deepseekApiToken().isEmpty()) {
		controller->show(Box([](not_null<Ui::GenericBox*> box) {
			box->setTitle(rpl::single(u"Error"_q));
			box->addRow(object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(u"Set DeepSeek API token in Pro Settings."_q),
				st::boxLabel));
			box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		}));
		return;
	}

	auto chunks = FormatAndChunk(items, session);
	if (chunks.empty()) return;

	const auto existingRole = pro.peerRole(peerId);

	controller->show(Box([=, chunks = std::move(chunks)](
			not_null<Ui::GenericBox*> box) mutable {
		box->setTitle(rpl::single(u"Index Memory"_q));

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(QString::number(int(items.size()))
				+ u" messages selected, "_q
				+ QString::number(int(chunks.size()))
				+ u" chunk(s)"_q),
			st::boxLabel));

		const auto roleField = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::defaultInputField,
				rpl::single(u"Contact role (sister, colleague...)"_q),
				existingRole));

		box->addButton(
			rpl::single(u"Start Indexing"_q),
			[=, chunks = std::move(chunks)]() mutable {
				const auto role = roleField->getLastText().trimmed();
				if (!role.isEmpty()) {
					session->proStorage().setPeerRole(peerId, role);
				}
				box->closeBox();

				controller->show(Box([=, chunks = std::move(chunks)](
						not_null<Ui::GenericBox*> progressBox) mutable {
					progressBox->setTitle(
						rpl::single(u"Indexing Memory..."_q));
					RunIndexing(
						progressBox,
						session,
						peerId,
						role,
						std::move(chunks),
						done);
				}));
			});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace ProMemory
