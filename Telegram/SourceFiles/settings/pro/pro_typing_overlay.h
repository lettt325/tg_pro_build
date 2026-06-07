/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"

#include <QWidget>

namespace Main { class Session; }

namespace ProOverlay {

enum class Corner : int {
	TopLeft = 0,
	TopRight = 1,
	BottomRight = 2,
	BottomLeft = 3,
};

enum class Size : int {
	Small = 0,
	Medium = 1,
	Large = 2,
};

enum class Style : int {
	Dark = 0,
	Light = 1,
};

struct Config {
	Corner corner = Corner::TopRight;
	Size size = Size::Medium;
	Style style = Style::Dark;
	QString screenName;
};

class TypingOverlay final : public QWidget {
public:
	TypingOverlay();

	void showTyping(
		const QString &userName,
		uint64 peerId,
		Main::Session *session,
		const Config &config);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	void hideOverlay();
	void updatePosition(const Config &config);
	void applySize(Size size);

	QString _userName;
	uint64 _peerId = 0;
	Main::Session *_session = nullptr;
	Style _style = Style::Dark;
	int _fontSize = 14;
	int _radius = 12;
	base::Timer _hideTimer;
};

} // namespace ProOverlay
