/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "platform/mac/sparkle_mac.h"

#ifdef TDESKTOP_USE_SPARKLE
#import <Sparkle/Sparkle.h>

namespace {

SPUStandardUpdaterController *g_updaterController = nil;

} // namespace

namespace Platform {

void InitSparkle() {
	g_updaterController = [[SPUStandardUpdaterController alloc]
		initWithStartingUpdater:YES
		updaterDelegate:nil
		userDriverDelegate:nil];
}

void CheckForUpdates() {
	[g_updaterController checkForUpdates:nil];
}

} // namespace Platform

#else // TDESKTOP_USE_SPARKLE

namespace Platform {

void InitSparkle() {
}

void CheckForUpdates() {
}

} // namespace Platform

#endif // TDESKTOP_USE_SPARKLE
