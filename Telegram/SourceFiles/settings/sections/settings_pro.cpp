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
#include "lang/lang_keys.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Builder;

void BuildSaveDeletedSection(SectionBuilder &builder) {
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
			.onClick = [] {},
			.keywords = { u"exceptions"_q, u"chats"_q, u"deleted"_q },
		});
	}, toggle ? toggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(u"When enabled, messages deleted by other users will be preserved locally. Use Exceptions to exclude specific chats."_q));
}

void BuildWeakWordsSection(SectionBuilder &builder) {
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
			.onClick = [] {},
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
	BuildSaveDeletedSection(builder);
	BuildWeakWordsSection(builder);
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
