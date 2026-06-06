/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <vector>

namespace Main { class Session; }

namespace ProSettings {

class Storage {
public:
	explicit Storage(not_null<Main::Session*> session);

	[[nodiscard]] std::vector<QString> weakWords() const;
	void setWeakWords(std::vector<QString> words);
	[[nodiscard]] bool weakWordsFilterEnabled() const;
	void setWeakWordsFilterEnabled(bool enabled);

	[[nodiscard]] std::vector<uint64> exceptionPeerIds() const;
	void addException(uint64 peerId);
	void removeException(uint64 peerId);
	void clearExceptions();
	[[nodiscard]] bool saveDeletedEnabled() const;
	void setSaveDeletedEnabled(bool enabled);

private:
	void load();
	void save();

	not_null<Main::Session*> _session;
	std::vector<QString> _weakWords;
	bool _weakWordsEnabled = false;
	std::vector<uint64> _exceptionPeerIds;
	bool _saveDeletedEnabled = false;
};

} // namespace ProSettings
