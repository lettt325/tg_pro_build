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
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <QGuiApplication>
#include <QScreen>

namespace Settings {
namespace {

using namespace Builder;

[[nodiscard]] QString CornerName(int corner) {
	switch (corner) {
	case 0: return u"Top Left"_q;
	case 1: return u"Top Right"_q;
	case 2: return u"Bottom Right"_q;
	case 3: return u"Bottom Left"_q;
	}
	return u"Top Right"_q;
}

[[nodiscard]] QString SizeName(int size) {
	switch (size) {
	case 0: return u"Small"_q;
	case 1: return u"Medium"_q;
	case 2: return u"Large"_q;
	}
	return u"Medium"_q;
}

[[nodiscard]] QString StyleName(int style) {
	switch (style) {
	case 0: return u"Dark"_q;
	case 1: return u"Light"_q;
	}
	return u"Dark"_q;
}

[[nodiscard]] QString ScreenLabel(const QString &name) {
	return name.isEmpty() ? u"Default"_q : name;
}

struct OverlayState {
	ProSettings::Storage *storage = nullptr;
	rpl::variable<QString> cornerLabel;
	rpl::variable<QString> sizeLabel;
	rpl::variable<QString> styleLabel;
	rpl::variable<QString> screenLabel;
};

void InitOverlayState(
		OverlayState *state,
		not_null<Main::Session*> session) {
	state->storage = &session->proStorage();
	state->cornerLabel = CornerName(state->storage->overlayCorner());
	state->sizeLabel = SizeName(state->storage->overlaySize());
	state->styleLabel = StyleName(state->storage->overlayStyle());
	state->screenLabel = ScreenLabel(
		state->storage->overlayScreenName());
}

void BuildOverlaySection(SectionBuilder &builder, OverlayState *state) {
	const auto container = builder.container();

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
		const auto inner = builder.container();
		const auto menu = inner
			? inner->lifetime().make_state<
				base::unique_qptr<Ui::PopupMenu>>()
			: nullptr;

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

		builder.addButton({
			.id = u"pro/overlay/corner"_q,
			.title = rpl::single(u"Position"_q),
			.st = &st::settingsButtonNoIcon,
			.label = state
				? state->cornerLabel.value() | rpl::type_erased
				: rpl::single(QString()) | rpl::type_erased,
			.onClick = (inner && state) ? Fn<void()>([=] {
				*menu = base::make_unique_q<Ui::PopupMenu>(inner);
				const auto corners = { 0, 1, 2, 3 };
				for (const auto c : corners) {
					(*menu)->addAction(CornerName(c), [=] {
						state->storage->setOverlayCorner(c);
						state->cornerLabel = CornerName(c);
					});
				}
				(*menu)->popup(QCursor::pos());
			}) : Fn<void()>(nullptr),
			.keywords = { u"position"_q, u"corner"_q },
		});

		builder.addButton({
			.id = u"pro/overlay/size"_q,
			.title = rpl::single(u"Size"_q),
			.st = &st::settingsButtonNoIcon,
			.label = state
				? state->sizeLabel.value() | rpl::type_erased
				: rpl::single(QString()) | rpl::type_erased,
			.onClick = (inner && state) ? Fn<void()>([=] {
				*menu = base::make_unique_q<Ui::PopupMenu>(inner);
				const auto sizes = { 0, 1, 2 };
				for (const auto s : sizes) {
					(*menu)->addAction(SizeName(s), [=] {
						state->storage->setOverlaySize(s);
						state->sizeLabel = SizeName(s);
					});
				}
				(*menu)->popup(QCursor::pos());
			}) : Fn<void()>(nullptr),
			.keywords = { u"size"_q, u"small"_q, u"large"_q },
		});

		builder.addButton({
			.id = u"pro/overlay/style"_q,
			.title = rpl::single(u"Style"_q),
			.st = &st::settingsButtonNoIcon,
			.label = state
				? state->styleLabel.value() | rpl::type_erased
				: rpl::single(QString()) | rpl::type_erased,
			.onClick = (inner && state) ? Fn<void()>([=] {
				*menu = base::make_unique_q<Ui::PopupMenu>(inner);
				const auto styles = { 0, 1 };
				for (const auto s : styles) {
					(*menu)->addAction(StyleName(s), [=] {
						state->storage->setOverlayStyle(s);
						state->styleLabel = StyleName(s);
					});
				}
				(*menu)->popup(QCursor::pos());
			}) : Fn<void()>(nullptr),
			.keywords = { u"style"_q, u"dark"_q, u"light"_q },
		});

		const auto screens = QGuiApplication::screens();
		if (screens.size() > 1) {
			builder.addButton({
				.id = u"pro/overlay/screen"_q,
				.title = rpl::single(u"Display"_q),
				.st = &st::settingsButtonNoIcon,
				.label = state
					? state->screenLabel.value() | rpl::type_erased
					: rpl::single(QString()) | rpl::type_erased,
				.onClick = (inner && state) ? Fn<void()>([=] {
					*menu = base::make_unique_q<Ui::PopupMenu>(inner);
					(*menu)->addAction(u"Default"_q, [=] {
						state->storage->setOverlayScreenName(QString());
						state->screenLabel = u"Default"_q;
					});
					for (auto *s : QGuiApplication::screens()) {
						const auto name = s->name();
						const auto geo = s->geometry();
						const auto label = u"%1 (%2×%3)"_q
							.arg(name)
							.arg(geo.width())
							.arg(geo.height());
						(*menu)->addAction(label, [=] {
							state->storage->setOverlayScreenName(name);
							state->screenLabel = name;
						});
					}
					(*menu)->popup(QCursor::pos());
				}) : Fn<void()>(nullptr),
				.keywords = { u"display"_q, u"monitor"_q, u"screen"_q },
			});
		}
	}, masterToggle ? masterToggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(
		u"A floating overlay appears on top of all windows when someone "
		"types a direct message to you. Click the overlay to open the "
		"chat."_q));
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
