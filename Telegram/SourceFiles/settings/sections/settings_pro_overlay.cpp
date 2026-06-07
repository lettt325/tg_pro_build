/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_pro_overlay.h"

#include "settings/settings_common_session.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_main.h"
#include "settings/pro/pro_settings_storage.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/ui_utility.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Builder;

struct OverlayState {
	ProSettings::Storage *storage = nullptr;
};

void InitOverlayState(
		OverlayState *state,
		not_null<Main::Session*> session) {
	state->storage = &session->proStorage();
}

void BuildOverlaySection(SectionBuilder &builder, OverlayState *state) {
	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"Overlay"_q));

	const auto masterToggle = builder.addButton({
		.id = u"pro/overlay/enabled"_q,
		.title = rpl::single(u"Show Overlay"_q),
		.icon = { &st::menuIconNewWindow },
		.toggled = rpl::single(
			state ? state->storage->overlayEnabled() : false),
		.keywords = { u"overlay"_q, u"show"_q, u"window"_q },
	});

	if (masterToggle && state) {
		masterToggle->toggledChanges(
		) | rpl::on_next([=](bool enabled) {
			state->storage->setOverlayEnabled(enabled);
		}, masterToggle->lifetime());
	}

	builder.scope([&] {
		const auto typingToggle = builder.addButton({
			.id = u"pro/overlay/typing"_q,
			.title = rpl::single(u"Show when someone is typing"_q),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(
				state ? state->storage->overlayTypingEnabled() : false),
			.keywords = { u"typing"_q, u"overlay"_q, u"notify"_q },
		});

		if (typingToggle && state) {
			typingToggle->toggledChanges(
			) | rpl::on_next([=](bool enabled) {
				state->storage->setOverlayTypingEnabled(enabled);
			}, typingToggle->lifetime());
		}
	}, masterToggle ? masterToggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(
		u"When enabled, a floating overlay will appear on top of all "
		"windows when someone types a direct message to you — even if "
		"TGPro is in the background."_q));
}

class ProOverlaySettings : public Section<ProOverlaySettings> {
public:
	ProOverlaySettings(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

const auto kMeta = BuildHelper({
	.id = ProOverlaySettings::Id(),
	.parentId = MainId(),
	.title = &tr::lng_settings_advanced,
	.icon = &st::menuIconNewWindow,
}, [](SectionBuilder &builder) {
	const auto container = builder.container();
	const auto session = builder.session();
	const auto state = container
		? container->lifetime().make_state<OverlayState>()
		: nullptr;
	if (state) {
		InitOverlayState(state, session);
	}
	BuildOverlaySection(builder, state);
});

const SectionBuildMethod kProOverlaySection = kMeta.build;

ProOverlaySettings::ProOverlaySettings(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> ProOverlaySettings::title() {
	return rpl::single(u"PRO Overlay"_q);
}

void ProOverlaySettings::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	build(content, kProOverlaySection);

	Ui::ResizeFitChild(this, content);
}

} // namespace

Type ProOverlaySettingsId() {
	return ProOverlaySettings::Id();
}

} // namespace Settings
