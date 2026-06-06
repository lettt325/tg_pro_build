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
#include "ui/widgets/popup_menu.h"
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

class ProExceptionsController final : public PeerListController {
public:
	ProExceptionsController(
		not_null<Main::Session*> session,
		ProState *state);

	Main::Session &session() const override;
	void prepare() override;
	void rowClicked(not_null<PeerListRow*> row) override;
	void rowRightActionClicked(not_null<PeerListRow*> row) override;
	void loadMoreRows() override {}

	void addPeer(not_null<PeerData*> peer);

private:
	[[nodiscard]] std::unique_ptr<PeerListRow> createRow(
		not_null<PeerData*> peer) const;

	const not_null<Main::Session*> _session;
	ProState * const _state;

};

ProExceptionsController::ProExceptionsController(
	not_null<Main::Session*> session,
	ProState *state)
: _session(session)
, _state(state) {
}

Main::Session &ProExceptionsController::session() const {
	return *_session;
}

void ProExceptionsController::prepare() {
	for (const auto &peer : _state->exceptions) {
		delegate()->peerListAppendRow(createRow(peer));
	}
	delegate()->peerListRefreshRows();
}

void ProExceptionsController::rowClicked(not_null<PeerListRow*> row) {
}

void ProExceptionsController::rowRightActionClicked(
		not_null<PeerListRow*> row) {
	const auto peer = row->peer();
	_state->exceptions.remove(peer);
	_state->exceptionsCount = int(_state->exceptions.size());
	delegate()->peerListRemoveRow(row);
	delegate()->peerListRefreshRows();
}

void ProExceptionsController::addPeer(not_null<PeerData*> peer) {
	if (!_state->exceptions.emplace(peer).second) {
		return;
	}
	_state->exceptionsCount = int(_state->exceptions.size());
	delegate()->peerListAppendRow(createRow(peer));
	delegate()->peerListRefreshRows();
}

std::unique_ptr<PeerListRow> ProExceptionsController::createRow(
		not_null<PeerData*> peer) const {
	auto row = std::make_unique<PeerListRowWithLink>(peer);
	row->setActionLink(u"Remove"_q);
	return row;
}

void BuildSaveDeletedSection(SectionBuilder &builder, ProState *state) {
	const auto controller = builder.controller();

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
		struct ExceptionsState {
			std::unique_ptr<ProExceptionsController> controller;
			std::unique_ptr<PeerListContentDelegateSimple> delegate;
		};

		const auto inner = builder.container();
		ProExceptionsController *exceptionsController = nullptr;

		if (inner && controller && state) {
			auto listController = std::make_unique<ProExceptionsController>(
				&controller->session(),
				state);
			listController->setStyleOverrides(&st::settingsBlockedList);
			exceptionsController = listController.get();

			builder.addButton({
				.id = u"pro/save_deleted/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.onClick = [=] {
					auto pickerController = std::make_unique<
						ChooseRecipientBoxController>(
						ChooseRecipientArgs{
							.session = &controller->session(),
							.callback = [=](
									not_null<Data::Thread*> thread) {
								exceptionsController->addPeer(
									thread->peer());
							},
							.filter = [=](
									not_null<Data::Thread*> thread) {
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
				},
				.keywords = { u"exceptions"_q, u"add"_q, u"chats"_q },
			});

			const auto content = inner->add(
				object_ptr<PeerListContent>(
					inner,
					listController.get()));

			const auto es = content->lifetime().make_state<
				ExceptionsState>();
			es->controller = std::move(listController);
			es->delegate = std::make_unique<
				PeerListContentDelegateSimple>();
			es->delegate->setContent(content);
			es->controller->setDelegate(es->delegate.get());
		} else {
			builder.addButton({
				.id = u"pro/save_deleted/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.keywords = { u"exceptions"_q, u"add"_q, u"chats"_q },
			});
		}
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

		const auto menu = box->lifetime().make_state<
			base::unique_qptr<Ui::PopupMenu>>();

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
					*menu = base::make_unique_q<Ui::PopupMenu>(row);
					(*menu)->addAction(u"Remove"_q, [=] {
						auto updated = state->weakWords.current();
						updated.erase(
							std::remove(
								updated.begin(),
								updated.end(),
								word),
							updated.end());
						state->weakWords = std::move(updated);
						(*rebuild)();
					});
					(*menu)->popup(QCursor::pos());
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
