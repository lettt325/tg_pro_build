/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "pro/memory/memory_storage.h"

#include "main/main_session.h"
#include "main/main_account.h"
#include "storage/storage_account.h"
#include "base/unixtime.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace ProMemory {
namespace {

constexpr auto kPrefKey = "pro.memory";

} // namespace

Storage::Storage(not_null<Main::Session*> session)
: _session(session) {
	load();
}

Storage::~Storage() = default;

int64 Storage::nextId() {
	return _nextId++;
}

void Storage::addEntry(
		uint64 peerId,
		const QString &text,
		Source source,
		int64 msgId,
		int64 date,
		const QStringList &tags) {
	auto entry = Entry{
		.id = nextId(),
		.peerId = peerId,
		.text = text,
		.source = source,
		.msgId = msgId,
		.date = date,
		.createdAt = base::unixtime::now(),
		.tags = tags,
	};
	_entries[peerId].push_back(std::move(entry));
	save();
}

void Storage::updateEntry(
		int64 entryId,
		const QString &text,
		const QStringList &tags) {
	for (auto &[peer, entries] : _entries) {
		for (auto &entry : entries) {
			if (entry.id == entryId) {
				entry.text = text;
				entry.tags = tags;
				save();
				return;
			}
		}
	}
}

void Storage::removeEntry(int64 entryId) {
	for (auto &[peer, entries] : _entries) {
		const auto it = ranges::find(entries, entryId, &Entry::id);
		if (it != entries.end()) {
			entries.erase(it);
			if (entries.empty()) {
				_entries.remove(peer);
			}
			save();
			return;
		}
	}
}

std::vector<Entry> Storage::entriesForPeer(uint64 peerId) const {
	const auto it = _entries.find(peerId);
	if (it == _entries.end()) {
		return {};
	}
	return it->second;
}

std::vector<Entry> Storage::searchEntries(
		uint64 peerId,
		const QString &query) const {
	const auto all = entriesForPeer(peerId);
	if (query.isEmpty()) {
		return all;
	}
	auto result = std::vector<Entry>();
	const auto lower = query.toLower();
	for (const auto &entry : all) {
		if (entry.text.toLower().contains(lower)) {
			result.push_back(entry);
			continue;
		}
		for (const auto &tag : entry.tags) {
			if (tag.toLower().contains(lower)) {
				result.push_back(entry);
				break;
			}
		}
	}
	return result;
}

int Storage::entryCount(uint64 peerId) const {
	const auto it = _entries.find(peerId);
	return (it != _entries.end()) ? int(it->second.size()) : 0;
}

const Entry *Storage::findEntry(int64 entryId) const {
	for (const auto &[peer, entries] : _entries) {
		for (const auto &entry : entries) {
			if (entry.id == entryId) {
				return &entry;
			}
		}
	}
	return nullptr;
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
	_nextId = static_cast<int64>(obj.value("nextId").toDouble(1));

	const auto peers = obj.value("peers").toObject();
	for (auto pi = peers.begin(); pi != peers.end(); ++pi) {
		const auto peerId = static_cast<uint64>(pi.key().toDouble());
		if (!peerId) continue;

		auto &peerEntries = _entries[peerId];
		for (const auto &v : pi.value().toArray()) {
			const auto o = v.toObject();
			auto entry = Entry{
				.id = static_cast<int64>(o.value("id").toDouble()),
				.peerId = peerId,
				.text = o.value("t").toString(),
				.source = static_cast<Source>(o.value("s").toInt()),
				.msgId = static_cast<int64>(o.value("m").toDouble()),
				.date = static_cast<int64>(o.value("d").toDouble()),
				.createdAt = static_cast<int64>(o.value("c").toDouble()),
			};
			const auto tagsArr = o.value("tags").toArray();
			for (const auto &tag : tagsArr) {
				const auto s = tag.toString();
				if (!s.isEmpty()) {
					entry.tags.append(s);
				}
			}
			if (entry.id >= _nextId) {
				_nextId = entry.id + 1;
			}
			peerEntries.push_back(std::move(entry));
		}
	}
}

void Storage::save() {
	auto obj = QJsonObject();
	obj["nextId"] = static_cast<double>(_nextId);

	auto peers = QJsonObject();
	for (const auto &[peerId, entries] : _entries) {
		auto arr = QJsonArray();
		for (const auto &e : entries) {
			auto o = QJsonObject();
			o["id"] = static_cast<double>(e.id);
			o["t"] = e.text;
			o["s"] = static_cast<int>(e.source);
			if (e.msgId) {
				o["m"] = static_cast<double>(e.msgId);
			}
			if (e.date) {
				o["d"] = static_cast<double>(e.date);
			}
			o["c"] = static_cast<double>(e.createdAt);
			if (!e.tags.isEmpty()) {
				auto tags = QJsonArray();
				for (const auto &tag : e.tags) {
					tags.append(tag);
				}
				o["tags"] = tags;
			}
			arr.append(o);
		}
		peers[QString::number(peerId)] = arr;
	}
	obj["peers"] = peers;

	const auto data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
	_session->account().local().writePref<QByteArray>(kPrefKey, data);
}

} // namespace ProMemory
