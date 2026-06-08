/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/memory/info_memory_inner_widget.h"

#include "info/info_controller.h"
#include "pro/memory/memory_storage.h"
#include "pro/ai/deepseek_client.h"
#include "settings/pro/pro_settings_storage.h"
#include "base/unixtime.h"
#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QDateTime>

namespace Info::Memory {

InnerWidget::InnerWidget(
	QWidget *parent,
	not_null<Controller*> controller,
	not_null<PeerData*> peer)
: RpWidget(parent)
, _controller(controller)
, _peer(peer)
, _content(Ui::CreateChild<Ui::VerticalLayout>(this)) {
	setupTabs();
	setupFragmentsTab();
	setupAiTab();

	_aiWrap->toggle(false, anim::type::instant);

	_content->heightValue(
	) | rpl::on_next([this](int) {
		resizeToWidth(width());
	}, lifetime());
}

rpl::producer<int> InnerWidget::desiredHeightValue() const {
	return _content->heightValue();
}

void InnerWidget::resizeEvent(QResizeEvent *e) {
	_content->resizeToWidth(width());
	_content->moveToLeft(0, 0);
}

void InnerWidget::setupTabs() {
	_fragmentsTab = _content->add(
		object_ptr<Ui::SettingsButton>(
			_content,
			rpl::single(u"Fragments"_q),
			st::infoSharedMediaButton));
	_aiTab = _content->add(
		object_ptr<Ui::SettingsButton>(
			_content,
			rpl::single(u"AI Assistant"_q),
			st::infoSharedMediaButton));

	_fragmentsTab->setClickedCallback([=] {
		_showingFragments = true;
		_fragmentsWrap->toggle(true, anim::type::normal);
		_aiWrap->toggle(false, anim::type::normal);
	});
	_aiTab->setClickedCallback([=] {
		_showingFragments = false;
		_fragmentsWrap->toggle(false, anim::type::normal);
		_aiWrap->toggle(true, anim::type::normal);
	});
}

void InnerWidget::setupFragmentsTab() {
	_fragmentsWrap = _content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_content,
			object_ptr<Ui::VerticalLayout>(_content)));
	const auto container = _fragmentsWrap->entity();

	const auto addBtn = container->add(
		object_ptr<Ui::SettingsButton>(
			container,
			rpl::single(u"+ Add Memory"_q),
			st::infoSharedMediaButton));
	addBtn->setClickedCallback([=] { addManualEntry(); });

	_fragmentsList = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	refreshFragments();
}

void InnerWidget::refreshFragments() {
	while (_fragmentsList->count()) {
		delete _fragmentsList->widgetAt(0);
	}

	const auto peerId = _peer->id.value;
	const auto entries = _peer->session().memoryStorage()
		.entriesForPeer(peerId);

	if (entries.empty()) {
		_fragmentsList->add(
			object_ptr<Ui::FlatLabel>(
				_fragmentsList,
				rpl::single(u"No memories saved yet.\n"
					"Right-click a message → Save to Memory."_q),
				st::boxLabel),
			QMargins(16, 12, 16, 12));
		_fragmentsList->resizeToWidth(_fragmentsList->width());
		return;
	}

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto &entry = *it;
		const auto entryId = entry.id;
		const auto wrap = _fragmentsList->add(
			object_ptr<Ui::VerticalLayout>(_fragmentsList),
			QMargins(16, 8, 16, 0));

		const auto dateStr = entry.createdAt
			? QDateTime::fromSecsSinceEpoch(
				entry.createdAt).toString(u"dd.MM.yyyy hh:mm"_q)
			: QString();
		auto headerText = (entry.source == ProMemory::Source::Message)
			? u"From message"_q
			: u"Manual note"_q;
		if (!dateStr.isEmpty()) {
			headerText += u"  "_q + dateStr;
		}

		wrap->add(
			object_ptr<Ui::FlatLabel>(
				wrap,
				rpl::single(headerText),
				st::boxLabel),
			QMargins(0, 0, 0, 2));

		const auto textLabel = wrap->add(
			object_ptr<Ui::FlatLabel>(
				wrap,
				rpl::single(entry.text),
				st::boxLabel));
		textLabel->setSelectable(true);

		if (!entry.tags.isEmpty()) {
			wrap->add(
				object_ptr<Ui::FlatLabel>(
					wrap,
					rpl::single(u"Tags: "_q + entry.tags.join(u", "_q)),
					st::boxLabel),
				QMargins(0, 2, 0, 0));
		}

		const auto deleteBtn = wrap->add(
			object_ptr<Ui::LinkButton>(
				wrap,
				u"Delete"_q),
			QMargins(0, 4, 0, 4));
		deleteBtn->setClickedCallback([=] {
			_peer->session().memoryStorage().removeEntry(entryId);
			refreshFragments();
		});

		wrap->add(object_ptr<Ui::FixedHeightWidget>(wrap, 1));
	}

	_fragmentsList->resizeToWidth(_fragmentsList->width());
}

void InnerWidget::addManualEntry() {
	const auto peerId = _peer->id.value;
	const auto controller = _controller->parentController();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Add Memory"_q));
		const auto field = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::defaultInputField,
				rpl::single(u"Memory note..."_q)));
		const auto tagsField = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::defaultInputField,
				rpl::single(u"Tags (comma-separated)..."_q)));
		box->addButton(
			rpl::single(u"Save"_q),
			[=] {
				const auto text = field->getLastText().trimmed();
				if (text.isEmpty()) return;
				auto tags = QStringList();
				for (auto &t : tagsField->getLastText().split(',')) {
					const auto tag = t.trimmed();
					if (!tag.isEmpty()) {
						tags.append(tag);
					}
				}
				_peer->session().memoryStorage().addEntry(
					peerId,
					text,
					ProMemory::Source::Manual,
					0,
					0,
					tags);
				box->closeBox();
				refreshFragments();
			});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

void InnerWidget::setupAiTab() {
	_aiWrap = _content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_content,
			object_ptr<Ui::VerticalLayout>(_content)));
	const auto container = _aiWrap->entity();

	_aiInput = container->add(
		object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Ask about this person..."_q)),
		QMargins(16, 12, 16, 8));

	const auto sendBtn = container->add(
		object_ptr<Ui::RoundButton>(
			container,
			rpl::single(u"Ask AI"_q),
			st::defaultBoxButton),
		QMargins(16, 0, 16, 12));
	sendBtn->setClickedCallback([=] { sendAiQuery(); });

	_aiResults = container->add(
		object_ptr<Ui::VerticalLayout>(container));
}

void InnerWidget::sendAiQuery() {
	const auto query = _aiInput->getLastText().trimmed();
	if (query.isEmpty()) return;

	const auto &pro = _peer->session().proStorage();
	const auto token = pro.deepseekApiToken();
	if (token.isEmpty()) {
		while (_aiResults->count()) {
			delete _aiResults->widgetAt(0);
		}
		_aiResults->add(
			object_ptr<Ui::FlatLabel>(
				_aiResults,
				rpl::single(
					u"Please set DeepSeek API token in Pro Settings."_q),
				st::boxLabel),
			QMargins(16, 8, 16, 8));
		_aiResults->resizeToWidth(_aiResults->width());
		return;
	}

	_aiClient = std::make_unique<ProAI::DeepSeekClient>(
		token,
		pro.deepseekModel());
	_aiClient->setThinkingEnabled(pro.aiThinkingEnabled());

	const auto peerId = _peer->id.value;
	const auto entries = _peer->session().memoryStorage()
		.entriesForPeer(peerId);

	auto contextParts = QStringList();
	for (const auto &entry : entries) {
		auto part = entry.text;
		if (entry.createdAt) {
			part += u" [saved: "_q + QDateTime::fromSecsSinceEpoch(
				entry.createdAt).toString(u"yyyy-MM-dd"_q) + u"]"_q;
		}
		if (!entry.tags.isEmpty()) {
			part += u" [tags: "_q + entry.tags.join(u", "_q) + u"]"_q;
		}
		contextParts.append(part);
	}

	const auto systemPrompt = pro.aiSystemPrompt()
		+ u"\n\nSaved memories about this contact:\n"_q
		+ contextParts.join(u"\n"_q);

	auto messages = std::vector<ProAI::Message>();
	messages.push_back({ u"user"_q, query });

	while (_aiResults->count()) {
		delete _aiResults->widgetAt(0);
	}
	_aiResults->add(
		object_ptr<Ui::FlatLabel>(
			_aiResults,
			rpl::single(u"Thinking..."_q),
			st::boxLabel),
		QMargins(16, 8, 16, 8));
	_aiResults->resizeToWidth(_aiResults->width());

	const auto guard = QPointer<InnerWidget>(this);
	_aiClient->chat(systemPrompt, messages,
		[guard](ProAI::ChatResponse response, ProAI::Error error) {
		if (!guard) return;
		const auto self = guard.data();
		while (self->_aiResults->count()) {
			delete self->_aiResults->widgetAt(0);
		}
		if (error) {
			self->_aiResults->add(
				object_ptr<Ui::FlatLabel>(
					self->_aiResults,
					rpl::single(u"Error: "_q + error.message),
					st::boxLabel),
				QMargins(16, 8, 16, 8));
		} else {
			const auto label = self->_aiResults->add(
				object_ptr<Ui::FlatLabel>(
					self->_aiResults,
					rpl::single(response.content),
					st::boxLabel),
				QMargins(16, 8, 16, 8));
			label->setSelectable(true);
		}
		self->_aiResults->resizeToWidth(
			self->_aiResults->width());
	});
}

} // namespace Info::Memory
