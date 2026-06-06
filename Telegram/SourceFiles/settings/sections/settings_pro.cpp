/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_pro.h"

#include "settings/settings_common_session.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_main.h"
#include "boxes/peer_list_box.h"
#include "boxes/peer_list_controllers.h"
#include "data/data_peer.h"
#include "data/data_thread.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Builder;

struct ProState {
	base::flat_set<not_null<PeerData*>> exceptions;
	rpl::variable<int> exceptionsCount = 0;
	rpl::variable<std::vector<QString>> weakWords = std::vector<QString>{
		u"ну"_q,
		u"типа"_q,
		u"как бы"_q,
		u"вообще"_q,
		u"короче"_q,
		u"блин"_q,
		u"просто"_q,
		u"реально"_q,
		u"кстати"_q,
		u"в принципе"_q,
		u"по сути"_q,
		u"наверное"_q,
		u"может быть"_q,
		u"так сказать"_q,
	};
};

void BuildSaveDeletedSection(SectionBuilder &builder, ProState *state) {
	const auto controller = builder.controller();
	const auto session = builder.session();

	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"Deleted Messages"_q));

	const auto toggle = builder.addButton({
		.id = u"pro/save_deleted"_q,
		.title = rpl::single(u"Save deleted messages"_q),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(false),
		.keywords = { u"deleted"_q, u"messages"_q, u"save"_q },
	});

	builder.scope([&] {
		builder.addButton({
			.id = u"pro/save_deleted/exceptions"_q,
			.title = rpl::single(u"Exceptions"_q),
			.st = &st::settingsButtonNoIcon,
			.label = state
				? state->exceptionsCount.value(
				) | rpl::map([](int count) {
					return count
						? QString::number(count)
						: QString()
					;
				}) | rpl::type_erased
				: rpl::single(QString()) | rpl::type_erased,
			.onClick = (controller && state) ? Fn<void()>([=] {
				auto pickerController = std::make_unique<
					ChooseRecipientBoxController>(
					ChooseRecipientArgs{
						.session = &controller->session(),
						.callback = [=](not_null<Data::Thread*> thread) {
							const auto peer = thread->peer();
							if (state->exceptions.emplace(peer).second) {
								state->exceptionsCount = int(
									state->exceptions.size());
							}
						},
						.filter = [=](not_null<Data::Thread*> thread) {
							return !state->exceptions.contains(
								thread->peer());
						},
					});
				controller->show(Box<PeerListBox>(
					std::move(pickerController),
					[](not_null<PeerListBox*> box) {
						box->addButton(
							tr::lng_cancel(),
							[=] { box->closeBox(); });
					}));
			}) : Fn<void()>(nullptr),
			.keywords = { u"exceptions"_q, u"chats"_q, u"deleted"_q },
		});

		builder.addButton({
			.id = u"pro/save_deleted/clear"_q,
			.title = rpl::single(u"Clear all exceptions"_q),
			.st = &st::settingsButtonNoIcon,
			.onClick = (state) ? Fn<void()>([=] {
				state->exceptions.clear();
				state->exceptionsCount = 0;
			}) : Fn<void()>(nullptr),
			.shown = state
				? state->exceptionsCount.value(
				) | rpl::map(rpl::mappers::_1 > 0) | rpl::type_erased
				: rpl::single(false) | rpl::type_erased,
		});
	}, toggle ? toggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(u"When enabled, messages deleted by other users will be preserved locally. Use Exceptions to exclude specific chats."_q));
}

void ShowEditWeakWordsBox(
		not_null<Window::SessionController*> controller,
		ProState *state) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Weak Words"_q));

		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::settingsAddReplyField,
			rpl::single(u"Add a word…"_q),
			QString()));
		box->setFocusCallback([=] {
			field->setFocusFast();
		});

		const auto wordsWrap = box->addRow(
			object_ptr<Ui::VerticalLayout>(box));

		const auto rebuild = box->lifetime().make_state<Fn<void()>>();
		*rebuild = [=] {
			while (wordsWrap->count()) {
				delete wordsWrap->widgetAt(0);
			}
			const auto &words = state->weakWords.current();
			for (auto i = 0, count = int(words.size()); i < count; ++i) {
				const auto word = words[i];
				const auto row = wordsWrap->add(
					CreateButtonWithIcon(
						wordsWrap,
						rpl::single(word),
						st::settingsButtonNoIcon));
				row->addClickHandler([=] {
					auto updated = state->weakWords.current();
					if (i < int(updated.size())) {
						updated.erase(updated.begin() + i);
						state->weakWords = std::move(updated);
					}
					(*rebuild)();
				});
			}
			wordsWrap->resizeToWidth(wordsWrap->width());
		};
		(*rebuild)();

		const auto addWord = [=] {
			const auto text = field->getLastText().trimmed();
			if (text.isEmpty()) {
				field->showError();
				return;
			}
			auto updated = state->weakWords.current();
			for (const auto &existing : updated) {
				if (!existing.compare(text, Qt::CaseInsensitive)) {
					field->showError();
					return;
				}
			}
			updated.push_back(text);
			state->weakWords = std::move(updated);
			field->setText(QString());
			(*rebuild)();
		};

		field->submits(
		) | rpl::on_next([=] {
			addWord();
		}, field->lifetime());

		box->addButton(rpl::single(u"Add"_q), addWord);
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void BuildWeakWordsSection(SectionBuilder &builder, ProState *state) {
	const auto controller = builder.controller();

	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"Weak Words Filter"_q));

	const auto toggle = builder.addButton({
		.id = u"pro/weak_words"_q,
		.title = rpl::single(u"Filter weak words in messages"_q),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(false),
		.keywords = { u"weak"_q, u"words"_q, u"filter"_q, u"filler"_q },
	});

	builder.scope([&] {
		builder.addButton({
			.id = u"pro/weak_words/edit_list"_q,
			.title = rpl::single(u"Edit word list"_q),
			.st = &st::settingsButtonNoIcon,
			.label = state
				? state->weakWords.value(
				) | rpl::map([](const std::vector<QString> &words) {
					return words.empty()
						? QString()
						: QString::number(words.size());
				}) | rpl::type_erased
				: rpl::single(QString()) | rpl::type_erased,
			.onClick = (controller && state) ? Fn<void()>([=] {
				ShowEditWeakWordsBox(controller, state);
			}) : Fn<void()>(nullptr),
			.keywords = { u"edit"_q, u"words"_q, u"list"_q },
		});
	}, toggle ? toggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(u"Messages containing filler or weak words cannot be sent until rephrased. Edit the word list to customize which words are flagged."_q));
}

class ProSettings : public Section<ProSettings> {
public:
	ProSettings(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

const auto kMeta = BuildHelper({
	.id = ProSettings::Id(),
	.parentId = MainId(),
	.title = &tr::lng_settings_advanced,
	.icon = &st::menuIconSettings,
}, [](SectionBuilder &builder) {
	const auto container = builder.container();
	const auto state = container
		? container->lifetime().make_state<ProState>()
		: nullptr;
	BuildSaveDeletedSection(builder, state);
	BuildWeakWordsSection(builder, state);
	builder.addSkip();
});

const SectionBuildMethod kProSettingsSection = kMeta.build;

ProSettings::ProSettings(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> ProSettings::title() {
	return rpl::single(u"Pro Settings"_q);
}

void ProSettings::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	build(content, kProSettingsSection);

	Ui::ResizeFitChild(this, content);
}

} // namespace

Type ProSettingsId() {
	return ProSettings::Id();
}

} // namespace Settings
