/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/pro/pro_settings_storage.h"

#include "settings/pro/pro_typing_overlay.h"
#include "main/main_session.h"
#include "main/main_account.h"
#include "storage/storage_account.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace ProSettings {
namespace {

constexpr auto kPrefKey = "pro.settings";

const auto kDefaultWeakWords = std::vector<QString>{
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

} // namespace

Storage::Storage(not_null<Main::Session*> session)
: _session(session)
, _weakWords(kDefaultWeakWords) {
	load();
}

Storage::~Storage() = default;

std::vector<QString> Storage::weakWords() const {
	return _weakWords;
}

void Storage::setWeakWords(std::vector<QString> words) {
	_weakWords = std::move(words);
	save();
}

bool Storage::weakWordsFilterEnabled() const {
	return _weakWordsEnabled;
}

void Storage::setWeakWordsFilterEnabled(bool enabled) {
	_weakWordsEnabled = enabled;
	save();
}

std::vector<uint64> Storage::exceptionPeerIds() const {
	return _exceptionPeerIds;
}

void Storage::addException(uint64 peerId) {
	if (ranges::contains(_exceptionPeerIds, peerId)) {
		return;
	}
	_exceptionPeerIds.push_back(peerId);
	save();
}

void Storage::removeException(uint64 peerId) {
	_exceptionPeerIds.erase(
		std::remove(_exceptionPeerIds.begin(), _exceptionPeerIds.end(), peerId),
		_exceptionPeerIds.end());
	save();
}

void Storage::clearExceptions() {
	_exceptionPeerIds.clear();
	save();
}

bool Storage::saveDeletedEnabled() const {
	return _saveDeletedEnabled;
}

void Storage::setSaveDeletedEnabled(bool enabled) {
	_saveDeletedEnabled = enabled;
	save();
}

bool Storage::ghostEnabled() const {
	return _ghostEnabled;
}

void Storage::setGhostEnabled(bool enabled) {
	_ghostEnabled = enabled;
	save();
}

bool Storage::ghostNoRead() const {
	return _ghostNoRead;
}

void Storage::setGhostNoRead(bool enabled) {
	_ghostNoRead = enabled;
	save();
}

bool Storage::ghostNoOnline() const {
	return _ghostNoOnline;
}

void Storage::setGhostNoOnline(bool enabled) {
	_ghostNoOnline = enabled;
	save();
}

bool Storage::ghostNoTyping() const {
	return _ghostNoTyping;
}

void Storage::setGhostNoTyping(bool enabled) {
	_ghostNoTyping = enabled;
	save();
}

bool Storage::ghostReadOnInteract() const {
	return _ghostReadOnInteract;
}

void Storage::setGhostReadOnInteract(bool enabled) {
	_ghostReadOnInteract = enabled;
	save();
}

bool Storage::ghostInstantOnline() const {
	return _ghostInstantOnline;
}

void Storage::setGhostInstantOnline(bool enabled) {
	_ghostInstantOnline = enabled;
	save();
}

std::vector<uint64> Storage::ghostExceptionPeerIds() const {
	return _ghostExceptionPeerIds;
}

void Storage::addGhostException(uint64 peerId) {
	if (ranges::contains(_ghostExceptionPeerIds, peerId)) {
		return;
	}
	_ghostExceptionPeerIds.push_back(peerId);
	save();
}

void Storage::removeGhostException(uint64 peerId) {
	_ghostExceptionPeerIds.erase(
		std::remove(
			_ghostExceptionPeerIds.begin(),
			_ghostExceptionPeerIds.end(),
			peerId),
		_ghostExceptionPeerIds.end());
	save();
}

void Storage::clearGhostExceptions() {
	_ghostExceptionPeerIds.clear();
	save();
}

bool Storage::overlayEnabled() const {
	return _overlayEnabled;
}

void Storage::setOverlayEnabled(bool enabled) {
	_overlayEnabled = enabled;
	save();
}

bool Storage::overlayTypingEnabled() const {
	return _overlayTypingEnabled;
}

void Storage::setOverlayTypingEnabled(bool enabled) {
	_overlayTypingEnabled = enabled;
	save();
}

int Storage::overlayCorner() const {
	return _overlayCorner;
}

void Storage::setOverlayCorner(int corner) {
	_overlayCorner = corner;
	save();
}

int Storage::overlaySize() const {
	return _overlaySize;
}

void Storage::setOverlaySize(int size) {
	_overlaySize = size;
	save();
}

int Storage::overlayStyle() const {
	return _overlayStyle;
}

void Storage::setOverlayStyle(int style) {
	_overlayStyle = style;
	save();
}

QString Storage::overlayScreenName() const {
	return _overlayScreenName;
}

void Storage::setOverlayScreenName(const QString &name) {
	_overlayScreenName = name;
	save();
}

void Storage::addDeletedMessage(uint64 peerId, int64 msgId) {
	_deletedMessages[peerId].emplace(msgId);
	save();
}

bool Storage::isDeletedByOther(uint64 peerId, int64 msgId) const {
	const auto i = _deletedMessages.find(peerId);
	return i != _deletedMessages.end() && i->second.contains(msgId);
}

void Storage::showTypingOverlay(const QString &userName, uint64 peerId) {
	if (!_typingOverlay) {
		_typingOverlay = std::make_unique<ProOverlay::TypingOverlay>();
	}
	_typingOverlay->showTyping(userName, peerId, _session, ProOverlay::Config{
		.corner = static_cast<ProOverlay::Corner>(_overlayCorner),
		.size = static_cast<ProOverlay::Size>(_overlaySize),
		.style = static_cast<ProOverlay::Style>(_overlayStyle),
		.screenName = _overlayScreenName,
	});
}

bool Storage::isGhostActiveForPeer(uint64 peerId) const {
	if (!_ghostEnabled) {
		return false;
	}
	if (peerId && ranges::contains(_ghostExceptionPeerIds, peerId)) {
		return false;
	}
	return true;
}

void Storage::markPeerInteracted(uint64 peerId) {
	if (peerId) {
		_ghostInteractedPeers.emplace(peerId);
	}
}

bool Storage::consumePeerInteracted(uint64 peerId) {
	return peerId && _ghostInteractedPeers.remove(peerId);
}

bool Storage::hasAnyInteracted() const {
	return !_ghostInteractedPeers.empty();
}

void Storage::load() {
	const auto data = _session->account().local().readPref<QByteArray>(
		kPrefKey,
		QByteArray());
	if (data.isEmpty()) {
		return;
	}
	const auto doc = QJsonDocument::fromJson(data);
	if (doc.isNull()) {
		return;
	}
	const auto obj = doc.object();

	_weakWordsEnabled = obj.value("weakWordsEnabled").toBool();
	_saveDeletedEnabled = obj.value("saveDeletedEnabled").toBool();

	if (obj.contains("weakWords")) {
		_weakWords.clear();
		for (const auto &v : obj.value("weakWords").toArray()) {
			if (const auto s = v.toString(); !s.isEmpty()) {
				_weakWords.push_back(s);
			}
		}
	}

	_exceptionPeerIds.clear();
	for (const auto &v : obj.value("exceptions").toArray()) {
		const auto id = static_cast<uint64>(v.toDouble());
		if (id) {
			_exceptionPeerIds.push_back(id);
		}
	}

	_ghostEnabled = obj.value("ghostEnabled").toBool();
	_ghostNoRead = obj.value("ghostNoRead").toBool();
	_ghostNoOnline = obj.value("ghostNoOnline").toBool();
	_ghostNoTyping = obj.value("ghostNoTyping").toBool();
	_ghostReadOnInteract = obj.value("ghostReadOnInteract").toBool();
	_ghostInstantOnline = obj.value("ghostInstantOnline").toBool();

	_ghostExceptionPeerIds.clear();
	for (const auto &v : obj.value("ghostExceptions").toArray()) {
		const auto id = static_cast<uint64>(v.toDouble());
		if (id) {
			_ghostExceptionPeerIds.push_back(id);
		}
	}

	_overlayEnabled = obj.value("overlayEnabled").toBool();
	_overlayTypingEnabled = obj.value("overlayTypingEnabled").toBool();
	_overlayCorner = obj.value("overlayCorner").toInt(1);
	_overlaySize = obj.value("overlaySize").toInt(1);
	_overlayStyle = obj.value("overlayStyle").toInt(0);
	_overlayScreenName = obj.value("overlayScreenName").toString();

	_deletedMessages.clear();
	const auto dm = obj.value("deletedMessages").toObject();
	for (auto it = dm.begin(); it != dm.end(); ++it) {
		const auto peer = static_cast<uint64>(it.key().toDouble());
		if (!peer) continue;
		auto &set = _deletedMessages[peer];
		for (const auto &v : it.value().toArray()) {
			const auto msg = static_cast<int64>(v.toDouble());
			if (msg) {
				set.emplace(msg);
			}
		}
	}
}

void Storage::save() {
	auto obj = QJsonObject();
	obj["weakWordsEnabled"] = _weakWordsEnabled;
	obj["saveDeletedEnabled"] = _saveDeletedEnabled;

	auto words = QJsonArray();
	for (const auto &w : _weakWords) {
		words.append(w);
	}
	obj["weakWords"] = words;

	auto exceptions = QJsonArray();
	for (const auto &id : _exceptionPeerIds) {
		exceptions.append(static_cast<double>(id));
	}
	obj["exceptions"] = exceptions;

	obj["ghostEnabled"] = _ghostEnabled;
	obj["ghostNoRead"] = _ghostNoRead;
	obj["ghostNoOnline"] = _ghostNoOnline;
	obj["ghostNoTyping"] = _ghostNoTyping;
	obj["ghostReadOnInteract"] = _ghostReadOnInteract;
	obj["ghostInstantOnline"] = _ghostInstantOnline;

	auto ghostExceptions = QJsonArray();
	for (const auto &id : _ghostExceptionPeerIds) {
		ghostExceptions.append(static_cast<double>(id));
	}
	obj["ghostExceptions"] = ghostExceptions;

	obj["overlayEnabled"] = _overlayEnabled;
	obj["overlayTypingEnabled"] = _overlayTypingEnabled;
	obj["overlayCorner"] = _overlayCorner;
	obj["overlaySize"] = _overlaySize;
	obj["overlayStyle"] = _overlayStyle;
	if (!_overlayScreenName.isEmpty()) {
		obj["overlayScreenName"] = _overlayScreenName;
	}

	if (!_deletedMessages.empty()) {
		auto dm = QJsonObject();
		for (const auto &[peer, msgs] : _deletedMessages) {
			auto arr = QJsonArray();
			for (const auto &msg : msgs) {
				arr.append(static_cast<double>(msg));
			}
			dm[QString::number(peer)] = arr;
		}
		obj["deletedMessages"] = dm;
	}

	_session->account().local().writePref<QByteArray>(
		kPrefKey,
		QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace ProSettings
