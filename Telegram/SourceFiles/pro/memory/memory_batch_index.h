/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <vector>
#include <functional>

class HistoryItem;

namespace Main { class Session; }
namespace Window { class SessionController; }

namespace ProMemory {

void ShowBatchIndexBox(
	not_null<Window::SessionController*> controller,
	not_null<Main::Session*> session,
	uint64 peerId,
	std::vector<not_null<HistoryItem*>> items,
	Fn<void()> done);

} // namespace ProMemory
