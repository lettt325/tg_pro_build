/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "base/flat_map.h"

#include <vector>
#include <functional>

namespace Main { class Session; }

namespace ProMemory {

enum class Source : char {
	Message,
	Manual,
};

struct Entry {
	int64 id = 0;
	uint64 peerId = 0;
	QString text;
	Source source = Source::Manual;
	int64 msgId = 0;
	int64 date = 0;
	int64 createdAt = 0;
	QStringList tags;
	QString category;
	QString importance;
	QString dateContext;
	QString relatedTo;
};

class Storage {
public:
	explicit Storage(not_null<Main::Session*> session);
	~Storage();

	void addEntry(
		uint64 peerId,
		const QString &text,
		Source source,
		int64 msgId = 0,
		int64 date = 0,
		const QStringList &tags = {},
		const QString &category = {},
		const QString &importance = {},
		const QString &dateContext = {},
		const QString &relatedTo = {});

	void updateEntry(int64 entryId, const QString &text,
		const QStringList &tags);
	void removeEntry(int64 entryId);

	[[nodiscard]] std::vector<Entry> entriesForPeer(uint64 peerId) const;
	[[nodiscard]] std::vector<Entry> searchEntries(
		uint64 peerId,
		const QString &query) const;
	[[nodiscard]] int entryCount(uint64 peerId) const;

	[[nodiscard]] const Entry *findEntry(int64 entryId) const;

private:
	void load();
	void save();
	[[nodiscard]] int64 nextId();

	not_null<Main::Session*> _session;
	int64 _nextId = 1;
	base::flat_map<uint64, std::vector<Entry>> _entries;
};

} // namespace ProMemory
