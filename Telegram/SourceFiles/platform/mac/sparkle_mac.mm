/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "platform/mac/sparkle_mac.h"

#ifdef TDESKTOP_USE_SPARKLE
#import <Sparkle/Sparkle.h>

@interface TGProSparkleDelegate : NSObject <SPUUpdaterDelegate>
@property (nonatomic, strong) NSMutableArray<NSString *> *logEntries;
@end

@implementation TGProSparkleDelegate

- (instancetype)init {
	self = [super init];
	if (self) {
		_logEntries = [[NSMutableArray alloc] init];
		[self addLog:@"Sparkle delegate initialized"];
	}
	return self;
}

- (void)addLog:(NSString *)message {
	NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
	[fmt setDateFormat:@"HH:mm:ss"];
	NSString *entry = [NSString stringWithFormat:@"[%@] %@",
		[fmt stringFromDate:[NSDate date]], message];
	@synchronized(self.logEntries) {
		[self.logEntries addObject:entry];
		if (self.logEntries.count > 100) {
			[self.logEntries removeObjectAtIndex:0];
		}
	}
}

- (void)updater:(SPUUpdater *)updater
		didFinishLoadingAppcast:(SUAppcast *)appcast {
	[self addLog:[NSString stringWithFormat:
		@"Appcast loaded, %lu items",
		(unsigned long)appcast.items.count]];
}

- (void)updater:(SPUUpdater *)updater
		didFindValidUpdate:(SUAppcastItem *)item {
	[self addLog:[NSString stringWithFormat:
		@"Update found: %@ (version %@)",
		item.displayVersionString,
		item.versionString]];
}

- (void)updaterDidNotFindUpdate:(SPUUpdater *)updater
		error:(NSError *)error {
	if (error) {
		[self addLog:[NSString stringWithFormat:
			@"No update found (error: %@)",
			error.localizedDescription]];
	} else {
		[self addLog:@"No update available (already latest)"];
	}
}

- (void)updater:(SPUUpdater *)updater
		didAbortWithError:(NSError *)error {
	[self addLog:[NSString stringWithFormat:
		@"Update aborted: %@", error.localizedDescription]];
}

- (void)updater:(SPUUpdater *)updater
		willDownloadUpdate:(SUAppcastItem *)item
		withRequest:(NSMutableURLRequest *)request {
	[self addLog:[NSString stringWithFormat:
		@"Downloading: %@", item.displayVersionString]];
}

- (void)updater:(SPUUpdater *)updater
		didDownloadUpdate:(SUAppcastItem *)item {
	[self addLog:[NSString stringWithFormat:
		@"Downloaded: %@", item.displayVersionString]];
}

- (void)updater:(SPUUpdater *)updater
		failedToDownloadUpdate:(SUAppcastItem *)item
		error:(NSError *)error {
	[self addLog:[NSString stringWithFormat:
		@"Download failed: %@", error.localizedDescription]];
}

- (void)updater:(SPUUpdater *)updater
		willInstallUpdate:(SUAppcastItem *)item {
	[self addLog:[NSString stringWithFormat:
		@"Installing: %@", item.displayVersionString]];
}

@end

namespace {

SPUStandardUpdaterController *g_updaterController = nil;
TGProSparkleDelegate *g_sparkleDelegate = nil;

} // namespace

namespace Platform {

void InitSparkle() {
	g_sparkleDelegate = [[TGProSparkleDelegate alloc] init];

	NSString *feedURL = [[NSBundle mainBundle]
		objectForInfoDictionaryKey:@"SUFeedURL"];
	NSString *pubKey = [[NSBundle mainBundle]
		objectForInfoDictionaryKey:@"SUPublicEDKey"];
	[g_sparkleDelegate addLog:
		[NSString stringWithFormat:@"Feed URL: %@", feedURL ?: @"(not set)"]];
	[g_sparkleDelegate addLog:
		[NSString stringWithFormat:@"Public key: %@",
			(pubKey.length > 0) ? @"set" : @"(not set)"]];

	g_updaterController = [[SPUStandardUpdaterController alloc]
		initWithStartingUpdater:YES
		updaterDelegate:g_sparkleDelegate
		userDriverDelegate:nil];

	[g_sparkleDelegate addLog:@"Updater controller created"];
}

void CheckForUpdates() {
	if (g_sparkleDelegate) {
		[g_sparkleDelegate addLog:@"Manual check initiated"];
	}
	[g_updaterController checkForUpdates:nil];
}

QString SparkleLog() {
	if (!g_sparkleDelegate) {
		return u"Sparkle not initialized"_q;
	}
	NSMutableString *result = [[NSMutableString alloc] init];
	@synchronized(g_sparkleDelegate.logEntries) {
		for (NSString *entry in g_sparkleDelegate.logEntries) {
			[result appendString:entry];
			[result appendString:@"\n"];
		}
	}
	return QString::fromNSString(result);
}

} // namespace Platform

#else // TDESKTOP_USE_SPARKLE

namespace Platform {

void InitSparkle() {
}

void CheckForUpdates() {
}

QString SparkleLog() {
	return u"Sparkle is disabled (DESKTOP_APP_DISABLE_SPARKLE=ON)"_q;
}

} // namespace Platform

#endif // TDESKTOP_USE_SPARKLE
