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
#include "ui/widgets/shadow.h"
#include "ui/widgets/slider_natural_width.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_dialogs.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QDateTime>

namespace Info::Memory {

InnerWidget::InnerWidget(
	QWidget *parent,
	not_null<Controller*> controller,
	not_null<PeerData*> peer)
: VerticalLayout(parent)
, _controller(controller)
, _peer(peer) {
	setupTabs();
	setupFragmentsTab();
	setupAiTab();
	_aiWrap->toggle(false, anim::type::instant);
}

void InnerWidget::setupTabs() {
	const auto fragmentsText = u"Fragments"_q;
	const auto aiText = u"AI Assistant"_q;

	_tabs = add(
		object_ptr<Ui::SlideWrap<Ui::CustomWidthSlider>>(
			this,
			object_ptr<Ui::CustomWidthSlider>(
				this,
				st::dialogsSearchTabs)));

	_tabs->entity()->addSection(fragmentsText);
	_tabs->entity()->addSection(aiText);

	{
		const auto &st = st::defaultTabsSlider;
		_tabs->entity()->setNaturalWidth(0
			+ st.labelStyle.font->width(fragmentsText)
			+ st.labelStyle.font->width(aiText)
			+ rect::m::sum::h(st::boxRowPadding));
	}

	const auto shadow = Ui::CreateChild<Ui::PlainShadow>(this);
	shadow->show();
	_tabs->geometryValue(
	) | rpl::on_next([=](const QRect &r) {
		shadow->setGeometry(
			x(),
			rect::bottom(r) - shadow->height(),
			width(),
			shadow->height());
	}, shadow->lifetime());

	_tabs->entity()->sectionActivated(
	) | rpl::on_next([=](int index) {
		_fragmentsWrap->toggle(!index, anim::type::instant);
		_aiWrap->toggle(index, anim::type::instant);
	}, lifetime());
}

void InnerWidget::setupFragmentsTab() {
	_fragmentsWrap = add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			this,
			object_ptr<Ui::VerticalLayout>(this)));
	const auto container = _fragmentsWrap->entity();

	container->add(
		object_ptr<Ui::SettingsButton>(
			container,
			rpl::single(u"Add Memory Note"_q),
			st::infoSharedMediaButton)
	)->setClickedCallback([=] { addManualEntry(); });

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
		Ui::AddDividerText(
			_fragmentsList,
			rpl::single(u"No memories saved yet. "
				"Right-click a message and tap Save to Memory."_q));
		_fragmentsList->resizeToWidth(width());
		return;
	}

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto &entry = *it;
		const auto entryId = entry.id;

		Ui::AddSkip(_fragmentsList);

		const auto dateStr = entry.createdAt
			? QDateTime::fromSecsSinceEpoch(
				entry.createdAt).toString(u"dd.MM.yyyy hh:mm"_q)
			: QString();
		auto sourceText = (entry.source == ProMemory::Source::Message)
			? u"From message"_q
			: u"Manual note"_q;
		if (!dateStr.isEmpty()) {
			sourceText += u"  \xB7  "_q + dateStr;
		}

		Ui::AddSubsectionTitle(
			_fragmentsList,
			rpl::single(sourceText));

		const auto textLabel = _fragmentsList->add(
			object_ptr<Ui::FlatLabel>(
				_fragmentsList,
				rpl::single(entry.text),
				st::boxDividerLabel),
			st::defaultBoxDividerLabelPadding);
		textLabel->setSelectable(true);

		if (!entry.tags.isEmpty()) {
			_fragmentsList->add(
				object_ptr<Ui::FlatLabel>(
					_fragmentsList,
					rpl::single(entry.tags.join(u", "_q)),
					st::defaultFlatLabel),
				QMargins(22, 2, 22, 0));
		}

		_fragmentsList->add(
			object_ptr<Ui::SettingsButton>(
				_fragmentsList,
				rpl::single(u"Delete"_q),
				st::settingsAttentionButtonWithIcon)
		)->setClickedCallback([=] {
			_peer->session().memoryStorage().removeEntry(entryId);
			refreshFragments();
		});

		Ui::AddDivider(_fragmentsList);
	}

	_fragmentsList->resizeToWidth(width());
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
	_aiWrap = add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			this,
			object_ptr<Ui::VerticalLayout>(this)));
	const auto container = _aiWrap->entity();

	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(
		container,
		rpl::single(u"Ask about this person"_q));

	_aiInput = container->add(
		object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Your question..."_q)),
		st::defaultBoxDividerLabelPadding);

	Ui::AddSkip(container);

	container->add(
		object_ptr<Ui::RoundButton>(
			container,
			rpl::single(u"Ask AI"_q),
			st::defaultBoxButton),
		QMargins(22, 0, 22, 0)
	)->setClickedCallback([=] { sendAiQuery(); });

	Ui::AddSkip(container);
	Ui::AddDivider(container);
	Ui::AddSkip(container);

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
		Ui::AddDividerText(
			_aiResults,
			rpl::single(
				u"Please set DeepSeek API token in Pro Settings."_q));
		_aiResults->resizeToWidth(width());
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
	Ui::AddSubsectionTitle(
		_aiResults,
		rpl::single(u"Response"_q));
	_aiResults->add(
		object_ptr<Ui::FlatLabel>(
			_aiResults,
			rpl::single(u"Thinking..."_q),
			st::boxDividerLabel),
		st::defaultBoxDividerLabelPadding);
	_aiResults->resizeToWidth(width());

	const auto guard = QPointer<InnerWidget>(this);
	_aiClient->chat(systemPrompt, messages,
		[guard](ProAI::ChatResponse response, ProAI::Error error) {
		if (!guard) return;
		const auto self = guard.data();
		while (self->_aiResults->count()) {
			delete self->_aiResults->widgetAt(0);
		}
		Ui::AddSubsectionTitle(
			self->_aiResults,
			rpl::single(u"Response"_q));
		if (error) {
			Ui::AddDividerText(
				self->_aiResults,
				rpl::single(u"Error: "_q + error.message));
		} else {
			const auto label = self->_aiResults->add(
				object_ptr<Ui::FlatLabel>(
					self->_aiResults,
					rpl::single(response.content),
					st::boxDividerLabel),
				st::defaultBoxDividerLabelPadding);
			label->setSelectable(true);
		}
		self->_aiResults->resizeToWidth(self->width());
	});
}

} // namespace Info::Memory
