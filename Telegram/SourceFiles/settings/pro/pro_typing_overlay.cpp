/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/pro/pro_typing_overlay.h"

#include "ui/platform/ui_platform_utility.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

namespace ProOverlay {

TypingOverlay::TypingOverlay()
: QWidget(nullptr)
, _hideTimer([=] { hideOverlay(); }) {
	setWindowFlags(Qt::WindowFlags(Qt::FramelessWindowHint)
		| Qt::WindowStaysOnTopHint
		| Qt::BypassWindowManagerHint
		| Qt::NoDropShadowWindowHint
		| Qt::Tool);
	setAttribute(Qt::WA_MacAlwaysShowToolWindow);
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_ShowWithoutActivating);
	setFixedSize(320, 52);

	Ui::Platform::InitOnTopPanel(this);
}

void TypingOverlay::showTyping(const QString &userName) {
	_userName = userName;
	updatePosition();
	show();
	raise();
	update();
	_hideTimer.callOnce(6000);
}

void TypingOverlay::hideOverlay() {
	hide();
	_userName.clear();
}

void TypingOverlay::updatePosition() {
	const auto screen = QGuiApplication::primaryScreen();
	if (!screen) {
		return;
	}
	const auto available = screen->availableGeometry();
	move(available.right() - width() - 20, available.top() + 20);
}

void TypingOverlay::paintEvent(QPaintEvent *) {
	auto p = QPainter(this);
	p.setRenderHint(QPainter::Antialiasing);

	p.setBrush(QColor(30, 30, 30, 220));
	p.setPen(Qt::NoPen);
	p.drawRoundedRect(rect(), 12, 12);

	auto font = p.font();
	font.setPixelSize(14);
	font.setBold(true);
	p.setFont(font);

	p.setPen(QColor(255, 255, 255));
	const auto textRect = rect().adjusted(16, 0, -16, 0);
	p.drawText(textRect, Qt::AlignVCenter, _userName + u" is typing…"_q);
}

} // namespace ProOverlay
