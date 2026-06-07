/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QString>
#include <vector>

namespace Platform {

void InitSparkle();
void CheckForUpdates();

struct SparkleLogSession {
	int id = 0;
	QString label;
	QString text;
};

[[nodiscard]] std::vector<SparkleLogSession> SparkleSessions();
[[nodiscard]] QString SparkleLog();

} // namespace Platform
