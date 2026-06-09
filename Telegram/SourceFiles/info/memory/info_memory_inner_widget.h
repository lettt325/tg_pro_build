/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/wrap/vertical_layout.h"

#include <memory>

namespace Ui {
class FlatLabel;
class InputField;
class PlainShadow;
class RoundButton;
class SettingsButton;
class CustomWidthSlider;
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

class InnerWidget final : public Ui::VerticalLayout {
public:
	InnerWidget(
		QWidget *parent,
		not_null<Controller*> controller,
		not_null<PeerData*> peer);

	[[nodiscard]] not_null<PeerData*> peer() const {
		return _peer;
	}

private:
	void setupTabs();
	void setupFragmentsTab();
	void setupAiTab();
	void refreshFragments();
	void addManualEntry();
	void sendAiQuery();

	not_null<Controller*> _controller;
	not_null<PeerData*> _peer;

	Ui::SlideWrap<Ui::CustomWidthSlider> *_tabs = nullptr;
	Ui::SlideWrap<Ui::VerticalLayout> *_fragmentsWrap = nullptr;
	Ui::SlideWrap<Ui::VerticalLayout> *_aiWrap = nullptr;
	Ui::VerticalLayout *_fragmentsList = nullptr;
	Ui::InputField *_aiInput = nullptr;
	Ui::VerticalLayout *_aiResults = nullptr;

	std::unique_ptr<ProAI::DeepSeekClient> _aiClient;
};

} // namespace Info::Memory
