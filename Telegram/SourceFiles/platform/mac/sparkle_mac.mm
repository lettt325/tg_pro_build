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
@property (nonatomic, strong) NSMutableArray<NSDictionary *> *sessions;
@property (nonatomic) int sessionCounter;
@end

@implementation TGProSparkleDelegate

- (instancetype)init {
	self = [super init];
	if (self) {
		_sessions = [[NSMutableArray alloc] init];
		_sessionCounter = 0;
		[self startSession:@"Init"];
		[self addLog:@"Sparkle delegate initialized"];
	}
	return self;
}

- (void)startSession:(NSString *)label {
	@synchronized(self.sessions) {
		self.sessionCounter++;
		NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
		[fmt setDateFormat:@"HH:mm:ss"];
		NSString *fullLabel = [NSString stringWithFormat:@"#%d %@ [%@]",
			self.sessionCounter, label,
			[fmt stringFromDate:[NSDate date]]];
		[self.sessions addObject:@{
			@"id": @(self.sessionCounter),
			@"label": fullLabel,
			@"entries": [[NSMutableArray alloc] init],
		}];
		if (self.sessions.count > 20) {
			[self.sessions removeObjectAtIndex:0];
		}
	}
}

- (void)addLog:(NSString *)message {
	NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
	[fmt setDateFormat:@"HH:mm:ss"];
	NSString *entry = [NSString stringWithFormat:@"[%@] %@",
		[fmt stringFromDate:[NSDate date]], message];
	@synchronized(self.sessions) {
		if (self.sessions.count > 0) {
			NSMutableArray *entries = self.sessions.lastObject[@"entries"];
			[entries addObject:entry];
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
			@"No update found — %@",
			error.localizedDescription]];
	} else {
		[self addLog:@"No update available (already latest)"];
	}
}

- (void)updater:(SPUUpdater *)updater
		didAbortWithError:(NSError *)error {
	[self addLog:[NSString stringWithFormat:
		@"ERROR: %@", error.localizedDescription]];
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
		[NSString stringWithFormat:@"Feed: %@", feedURL ?: @"(not set)"]];
	[g_sparkleDelegate addLog:
		[NSString stringWithFormat:@"Ed25519 key: %@",
			(pubKey.length > 0) ? @"present" : @"(not set)"]];

	g_updaterController = [[SPUStandardUpdaterController alloc]
		initWithStartingUpdater:YES
		updaterDelegate:g_sparkleDelegate
		userDriverDelegate:nil];

	[g_sparkleDelegate addLog:@"Updater started"];
}

void CheckForUpdates() {
	if (g_sparkleDelegate) {
		[g_sparkleDelegate startSession:@"Manual check"];
		[g_sparkleDelegate addLog:@"Checking for updates..."];
	}
	[g_updaterController checkForUpdates:nil];
}

std::vector<SparkleLogSession> SparkleSessions() {
	std::vector<SparkleLogSession> result;
	if (!g_sparkleDelegate) {
		return result;
	}
	@synchronized(g_sparkleDelegate.sessions) {
		for (NSDictionary *session in g_sparkleDelegate.sessions) {
			SparkleLogSession s;
			s.id = [session[@"id"] intValue];
			s.label = QString::fromNSString(session[@"label"]);
			NSMutableString *text = [[NSMutableString alloc] init];
			for (NSString *entry in session[@"entries"]) {
				[text appendString:entry];
				[text appendString:@"\n"];
			}
			s.text = QString::fromNSString(text);
			result.push_back(std::move(s));
		}
	}
	return result;
}

QString SparkleLog() {
	const auto sessions = SparkleSessions();
	QString result;
	for (const auto &s : sessions) {
		result += u"— "_q + s.label + u"\n"_q + s.text + u"\n"_q;
	}
	return result.isEmpty()
		? u"Sparkle not initialized"_q
		: result;
}

} // namespace Platform

#else // TDESKTOP_USE_SPARKLE

namespace Platform {

void InitSparkle() {
}

void CheckForUpdates() {
}

std::vector<SparkleLogSession> SparkleSessions() {
	return {};
}

QString SparkleLog() {
	return u"Sparkle is disabled (DESKTOP_APP_DISABLE_SPARKLE=ON)"_q;
}

} // namespace Platform

#endif // TDESKTOP_USE_SPARKLE
