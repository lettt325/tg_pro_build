/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/pro/pro_settings_storage.h"

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

	_session->account().local().writePref<QByteArray>(
		kPrefKey,
		QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace ProSettings
