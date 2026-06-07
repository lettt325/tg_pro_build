/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/pro/pro_typing_overlay.h"

#include "data/data_peer_id.h"
#include "main/main_session.h"
#include "ui/platform/ui_platform_utility.h"
#include "mainwindow.h"
#include "window/window_session_controller.h"

#include <QGuiApplication>
#include <QMouseEvent>
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
	setCursor(Qt::PointingHandCursor);
	setFixedSize(320, 52);

	Ui::Platform::InitOnTopPanel(this);
}

void TypingOverlay::showTyping(
		const QString &userName,
		uint64 peerId,
		Main::Session *session,
		const Config &config) {
	_userName = userName;
	_peerId = peerId;
	_session = session;
	_style = config.style;
	applySize(config.size);
	updatePosition(config);
	show();
	raise();
	update();
	_hideTimer.callOnce(6000);
}

void TypingOverlay::hideOverlay() {
	hide();
	_userName.clear();
	_peerId = 0;
}

void TypingOverlay::applySize(Size size) {
	switch (size) {
	case Size::Small:
		setFixedSize(280, 44);
		_fontSize = 12;
		_radius = 10;
		break;
	case Size::Medium:
		setFixedSize(320, 52);
		_fontSize = 14;
		_radius = 12;
		break;
	case Size::Large:
		setFixedSize(400, 64);
		_fontSize = 16;
		_radius = 14;
		break;
	}
}

void TypingOverlay::updatePosition(const Config &config) {
	auto *screen = static_cast<QScreen*>(nullptr);
	if (!config.screenName.isEmpty()) {
		for (auto *s : QGuiApplication::screens()) {
			if (s->name() == config.screenName) {
				screen = s;
				break;
			}
		}
	}
	if (!screen) {
		screen = QGuiApplication::primaryScreen();
	}
	if (!screen) {
		return;
	}
	const auto r = screen->availableGeometry();
	constexpr auto kPadding = 20;

	int x = 0;
	int y = 0;
	switch (config.corner) {
	case Corner::TopLeft:
		x = r.left() + kPadding;
		y = r.top() + kPadding;
		break;
	case Corner::TopRight:
		x = r.right() - width() - kPadding;
		y = r.top() + kPadding;
		break;
	case Corner::BottomRight:
		x = r.right() - width() - kPadding;
		y = r.bottom() - height() - kPadding;
		break;
	case Corner::BottomLeft:
		x = r.left() + kPadding;
		y = r.bottom() - height() - kPadding;
		break;
	}
	move(x, y);
}

void TypingOverlay::paintEvent(QPaintEvent *) {
	auto p = QPainter(this);
	p.setRenderHint(QPainter::Antialiasing);

	const auto isDark = (_style == Style::Dark);
	p.setBrush(isDark ? QColor(30, 30, 30, 220) : QColor(255, 255, 255, 230));
	p.setPen(Qt::NoPen);
	p.drawRoundedRect(rect(), _radius, _radius);

	auto font = p.font();
	font.setPixelSize(_fontSize);
	font.setBold(true);
	p.setFont(font);

	p.setPen(isDark ? QColor(255, 255, 255) : QColor(30, 30, 30));
	const auto textRect = rect().adjusted(16, 0, -16, 0);
	p.drawText(textRect, Qt::AlignVCenter, _userName + u" is typing…"_q);
}

void TypingOverlay::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton || !_session || !_peerId) {
		return;
	}
	if (const auto controller = _session->tryResolveWindow()) {
		controller->widget()->activate();
		controller->showPeerHistory(PeerId(_peerId));
	}
	hideOverlay();
}

} // namespace ProOverlay
