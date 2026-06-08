/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/widgets/scroll_area.h"

#include <memory>

namespace Ui {
class VerticalLayout;
class FlatLabel;
class InputField;
class RoundButton;
class SettingsButton;
template <typename Widget>
class SlideWrap;
} // namespace Ui

namespace ProAI {
class DeepSeekClient;
} // namespace ProAI

namespace Info {
class Controller;
} // namespace Info

namespace Info::Memory {

class Memento;

class InnerWidget final : public Ui::RpWidget {
public:
	InnerWidget(
		QWidget *parent,
		not_null<Controller*> controller,
		not_null<PeerData*> peer);

	[[nodiscard]] not_null<PeerData*> peer() const {
		return _peer;
	}

	[[nodiscard]] rpl::producer<int> desiredHeightValue() const override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void setupTabs();
	void setupFragmentsTab();
	void setupAiTab();
	void refreshFragments();
	void addManualEntry();
	void sendAiQuery();

	not_null<Controller*> _controller;
	not_null<PeerData*> _peer;

	Ui::VerticalLayout *_content = nullptr;
	Ui::SettingsButton *_fragmentsTab = nullptr;
	Ui::SettingsButton *_aiTab = nullptr;
	Ui::SlideWrap<Ui::VerticalLayout> *_fragmentsWrap = nullptr;
	Ui::SlideWrap<Ui::VerticalLayout> *_aiWrap = nullptr;
	Ui::VerticalLayout *_fragmentsList = nullptr;
	Ui::InputField *_aiInput = nullptr;
	Ui::VerticalLayout *_aiResults = nullptr;

	std::unique_ptr<ProAI::DeepSeekClient> _aiClient;

	bool _showingFragments = true;
};

} // namespace Info::Memory
