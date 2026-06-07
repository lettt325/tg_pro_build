/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "base/flat_map.h"

#include <memory>
#include <vector>

namespace Main { class Session; }
namespace ProOverlay { class TypingOverlay; }

namespace ProSettings {

class Storage {
public:
	explicit Storage(not_null<Main::Session*> session);
	~Storage();

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

	[[nodiscard]] bool ghostEnabled() const;
	void setGhostEnabled(bool enabled);
	[[nodiscard]] bool ghostNoRead() const;
	void setGhostNoRead(bool enabled);
	[[nodiscard]] bool ghostNoOnline() const;
	void setGhostNoOnline(bool enabled);
	[[nodiscard]] bool ghostNoTyping() const;
	void setGhostNoTyping(bool enabled);
	[[nodiscard]] bool ghostReadOnInteract() const;
	void setGhostReadOnInteract(bool enabled);
	[[nodiscard]] bool ghostInstantOnline() const;
	void setGhostInstantOnline(bool enabled);

	[[nodiscard]] std::vector<uint64> ghostExceptionPeerIds() const;
	void addGhostException(uint64 peerId);
	void removeGhostException(uint64 peerId);
	void clearGhostExceptions();

	[[nodiscard]] bool isGhostActiveForPeer(uint64 peerId) const;

	[[nodiscard]] bool overlayEnabled() const;
	void setOverlayEnabled(bool enabled);
	[[nodiscard]] bool overlayTypingEnabled() const;
	void setOverlayTypingEnabled(bool enabled);
	[[nodiscard]] int overlayCorner() const;
	void setOverlayCorner(int corner);
	[[nodiscard]] int overlaySize() const;
	void setOverlaySize(int size);
	[[nodiscard]] int overlayStyle() const;
	void setOverlayStyle(int style);
	[[nodiscard]] QString overlayScreenName() const;
	void setOverlayScreenName(const QString &name);

	void showTypingOverlay(const QString &userName, uint64 peerId);

	struct DeletedMsg {
		QString text;
		QString from;
		int64 date = 0;
	};

	void addDeletedMessage(
		uint64 peerId,
		int64 msgId,
		const QString &text,
		const QString &from,
		int64 date);
	[[nodiscard]] bool isDeletedByOther(uint64 peerId, int64 msgId) const;

	void markPeerInteracted(uint64 peerId);
	[[nodiscard]] bool consumePeerInteracted(uint64 peerId);
	[[nodiscard]] bool hasAnyInteracted() const;

private:
	void load();
	void save();

	not_null<Main::Session*> _session;
	std::vector<QString> _weakWords;
	bool _weakWordsEnabled = false;
	std::vector<uint64> _exceptionPeerIds;
	bool _saveDeletedEnabled = false;

	bool _ghostEnabled = false;
	bool _ghostNoRead = false;
	bool _ghostNoOnline = false;
	bool _ghostNoTyping = false;
	bool _ghostReadOnInteract = false;
	bool _ghostInstantOnline = false;
	std::vector<uint64> _ghostExceptionPeerIds;
	base::flat_set<uint64> _ghostInteractedPeers;

	base::flat_map<uint64, base::flat_map<int64, DeletedMsg>> _deletedMessages;

	bool _overlayEnabled = false;
	bool _overlayTypingEnabled = false;
	int _overlayCorner = 1;
	int _overlaySize = 1;
	int _overlayStyle = 0;
	QString _overlayScreenName;
	std::unique_ptr<ProOverlay::TypingOverlay> _typingOverlay;
};

} // namespace ProSettings
