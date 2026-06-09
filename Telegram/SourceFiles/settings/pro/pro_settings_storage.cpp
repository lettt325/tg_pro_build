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

bool Storage::saveDeletedInBotsEnabled() const {
	return _saveDeletedInBots;
}

void Storage::setSaveDeletedInBotsEnabled(bool enabled) {
	_saveDeletedInBots = enabled;
	save();
}

bool Storage::saveEditsEnabled() const {
	return _saveEditsEnabled;
}

void Storage::setSaveEditsEnabled(bool enabled) {
	_saveEditsEnabled = enabled;
	save();
}

std::vector<uint64> Storage::editExceptionPeerIds() const {
	return _editExceptionPeerIds;
}

void Storage::addEditException(uint64 peerId) {
	if (ranges::contains(_editExceptionPeerIds, peerId)) {
		return;
	}
	_editExceptionPeerIds.push_back(peerId);
	save();
}

void Storage::removeEditException(uint64 peerId) {
	_editExceptionPeerIds.erase(
		std::remove(
			_editExceptionPeerIds.begin(),
			_editExceptionPeerIds.end(),
			peerId),
		_editExceptionPeerIds.end());
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

void Storage::addEditVersion(
		uint64 peerId,
		int64 msgId,
		const QString &text,
		int64 date) {
	_editHistory[peerId][msgId].push_back(EditVersion{
		.text = text,
		.date = date,
	});
	save();
}

bool Storage::hasEditHistory(uint64 peerId, int64 msgId) const {
	const auto pi = _editHistory.find(peerId);
	if (pi == _editHistory.end()) return false;
	const auto mi = pi->second.find(msgId);
	return mi != pi->second.end() && !mi->second.empty();
}

std::vector<Storage::EditVersion> Storage::editHistory(
		uint64 peerId,
		int64 msgId) const {
	const auto pi = _editHistory.find(peerId);
	if (pi == _editHistory.end()) return {};
	const auto mi = pi->second.find(msgId);
	if (mi == pi->second.end()) return {};
	return mi->second;
}

void Storage::addDeletedMessage(
		uint64 peerId,
		int64 msgId,
		const QString &text,
		const QString &from,
		int64 date) {
	_deletedMessages[peerId].emplace(msgId, DeletedMsg{
		.text = text,
		.from = from,
		.date = date,
	});
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

bool Storage::aiMemoryEnabled() const {
	return _aiMemoryEnabled;
}

void Storage::setAiMemoryEnabled(bool enabled) {
	_aiMemoryEnabled = enabled;
	save();
}

QString Storage::deepseekApiToken() const {
	return _deepseekApiToken;
}

void Storage::setDeepseekApiToken(const QString &token) {
	_deepseekApiToken = token;
	save();
}

QString Storage::deepseekModel() const {
	return _deepseekModel;
}

void Storage::setDeepseekModel(const QString &model) {
	_deepseekModel = model;
	save();
}

bool Storage::aiThinkingEnabled() const {
	return _aiThinkingEnabled;
}

void Storage::setAiThinkingEnabled(bool enabled) {
	_aiThinkingEnabled = enabled;
	save();
}

QString Storage::aiSystemPrompt() const {
	return _aiSystemPrompt.isEmpty()
		? defaultAiSystemPrompt()
		: _aiSystemPrompt;
}

void Storage::setAiSystemPrompt(const QString &prompt) {
	_aiSystemPrompt = prompt;
	save();
}

QString Storage::defaultAiSystemPrompt() {
	return u"You are a personal memory assistant. "
		"You have access to saved notes about a specific contact. "
		"Answer questions precisely and concisely based on the provided memories. "
		"Always cite specific details and dates when available. "
		"If you don't have relevant information in the memories, say so honestly. "
		"Respond in the same language as the question."_q;
}

QString Storage::memoryLanguage() const {
	return _memoryLanguage;
}

void Storage::setMemoryLanguage(const QString &lang) {
	_memoryLanguage = lang;
	save();
}

QString Storage::peerRole(uint64 peerId) const {
	const auto it = _peerRoles.find(peerId);
	return (it != _peerRoles.end()) ? it->second : QString();
}

void Storage::setPeerRole(uint64 peerId, const QString &role) {
	if (role.isEmpty()) {
		_peerRoles.remove(peerId);
	} else {
		_peerRoles[peerId] = role;
	}
	save();
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
	_saveDeletedInBots = obj.value("saveDeletedInBots").toBool();
	_saveEditsEnabled = obj.value("saveEditsEnabled").toBool();

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

	_editExceptionPeerIds.clear();
	for (const auto &v : obj.value("editExceptions").toArray()) {
		const auto id = static_cast<uint64>(v.toDouble());
		if (id) {
			_editExceptionPeerIds.push_back(id);
		}
	}

	_editHistory.clear();
	const auto eh = obj.value("editHistory").toObject();
	for (auto pi = eh.begin(); pi != eh.end(); ++pi) {
		const auto peer = static_cast<uint64>(pi.key().toDouble());
		if (!peer) continue;
		auto &peerMap = _editHistory[peer];
		const auto msgs = pi.value().toObject();
		for (auto mi = msgs.begin(); mi != msgs.end(); ++mi) {
			const auto msgId = static_cast<int64>(mi.key().toDouble());
			if (!msgId) continue;
			auto &versions = peerMap[msgId];
			for (const auto &v : mi.value().toArray()) {
				const auto o = v.toObject();
				versions.push_back(EditVersion{
					.text = o.value("t").toString(),
					.date = static_cast<int64>(o.value("d").toDouble()),
				});
			}
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
		auto &map = _deletedMessages[peer];
		for (const auto &v : it.value().toArray()) {
			if (v.isObject()) {
				const auto o = v.toObject();
				const auto id = static_cast<int64>(o.value("id").toDouble());
				if (id) {
					map.emplace(id, DeletedMsg{
						.text = o.value("t").toString(),
						.from = o.value("f").toString(),
						.date = static_cast<int64>(o.value("d").toDouble()),
					});
				}
			} else {
				const auto id = static_cast<int64>(v.toDouble());
				if (id) {
					map.emplace(id, DeletedMsg{});
				}
			}
		}
	}

	_aiMemoryEnabled = obj.value("aiMemoryEnabled").toBool();
	_deepseekApiToken = obj.value("deepseekApiToken").toString();
	_deepseekModel = obj.value("deepseekModel").toString(
		u"deepseek-v4-flash"_q);
	_aiThinkingEnabled = obj.value("aiThinkingEnabled").toBool();
	_aiSystemPrompt = obj.value("aiSystemPrompt").toString();
	_memoryLanguage = obj.value("memoryLanguage").toString(u"ru"_q);

	_peerRoles.clear();
	const auto roles = obj.value("peerRoles").toObject();
	for (auto it = roles.begin(); it != roles.end(); ++it) {
		const auto id = static_cast<uint64>(it.key().toDouble());
		if (id) {
			_peerRoles[id] = it.value().toString();
		}
	}
}

void Storage::save() {
	auto obj = QJsonObject();
	obj["weakWordsEnabled"] = _weakWordsEnabled;
	obj["saveDeletedEnabled"] = _saveDeletedEnabled;
	obj["saveDeletedInBots"] = _saveDeletedInBots;
	obj["saveEditsEnabled"] = _saveEditsEnabled;

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

	auto editExceptions = QJsonArray();
	for (const auto &id : _editExceptionPeerIds) {
		editExceptions.append(static_cast<double>(id));
	}
	obj["editExceptions"] = editExceptions;

	if (!_editHistory.empty()) {
		auto eh = QJsonObject();
		for (const auto &[peer, msgs] : _editHistory) {
			auto mo = QJsonObject();
			for (const auto &[msgId, versions] : msgs) {
				auto arr = QJsonArray();
				for (const auto &v : versions) {
					auto o = QJsonObject();
					o["t"] = v.text;
					o["d"] = static_cast<double>(v.date);
					arr.append(o);
				}
				mo[QString::number(msgId)] = arr;
			}
			eh[QString::number(peer)] = mo;
		}
		obj["editHistory"] = eh;
	}

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
			for (const auto &[id, info] : msgs) {
				auto o = QJsonObject();
				o["id"] = static_cast<double>(id);
				if (!info.text.isEmpty()) {
					o["t"] = info.text;
				}
				if (!info.from.isEmpty()) {
					o["f"] = info.from;
				}
				if (info.date) {
					o["d"] = static_cast<double>(info.date);
				}
				arr.append(o);
			}
			dm[QString::number(peer)] = arr;
		}
		obj["deletedMessages"] = dm;
	}

	obj["aiMemoryEnabled"] = _aiMemoryEnabled;
	if (!_deepseekApiToken.isEmpty()) {
		obj["deepseekApiToken"] = _deepseekApiToken;
	}
	obj["deepseekModel"] = _deepseekModel;
	obj["aiThinkingEnabled"] = _aiThinkingEnabled;
	if (!_aiSystemPrompt.isEmpty()) {
		obj["aiSystemPrompt"] = _aiSystemPrompt;
	}
	obj["memoryLanguage"] = _memoryLanguage;
	if (!_peerRoles.empty()) {
		auto roles = QJsonObject();
		for (const auto &[id, role] : _peerRoles) {
			roles[QString::number(id)] = role;
		}
		obj["peerRoles"] = roles;
	}

	_session->account().local().writePref<QByteArray>(
		kPrefKey,
		QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace ProSettings
