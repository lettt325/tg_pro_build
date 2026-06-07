/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"

#include <QWidget>

namespace ProOverlay {

class TypingOverlay final : public QWidget {
public:
	TypingOverlay();

	void showTyping(const QString &userName);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void hideOverlay();
	void updatePosition();

	QString _userName;
	base::Timer _hideTimer;
};

} // namespace ProOverlay
