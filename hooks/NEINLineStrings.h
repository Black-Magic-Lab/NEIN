// Every LINE title and label NEIN matches on, with the spellings LINE ships in
// each language, plus the settings title NEIN shows itself. Inline so builds
// that use only some of them stay warning-free.
#pragma once

#import <Foundation/Foundation.h>

static inline BOOL NEINTabTitleIsOneOf(NSString *title, NSArray<NSString *> *knownTitles) {
    NSString *normalized = [[title stringByTrimmingCharactersInSet:
                             NSCharacterSet.whitespaceAndNewlineCharacterSet] uppercaseString];
    return [knownTitles containsObject:normalized ?: @""];
}

// Any of the lowercase tokens somewhere inside the text.
static inline BOOL NEINTextHasToken(NSString *text, NSArray<NSString *> *tokens) {
    NSString *lowercase = text.lowercaseString ?: @"";
    for (NSString *token in tokens) {
        if ([lowercase containsString:token]) return YES;
    }
    return NO;
}

static inline BOOL NEINIsHomeTabTitle(NSString *title) {
    return NEINTabTitleIsOneOf(title,
        @[@"HOME", @"首頁", @"主頁", @"主页", @"ホーム", @"홈"]);
}

static inline BOOL NEINIsChatTabTitle(NSString *title) {
    return NEINTabTitleIsOneOf(title,
        @[@"CHAT", @"CHATS", @"聊天", @"トーク", @"채팅"]);
}

static inline BOOL NEINIsCallTabTitle(NSString *title) {
    return NEINTabTitleIsOneOf(title,
        @[@"CALL", @"CALLS", @"通話", @"통화"]);
}

// 逛逛/Discover is LINE's TW commerce tab ("tw.commerce.gnb").
static inline BOOL NEINIsPromotionalTabTitle(NSString *title) {
    return NEINTabTitleIsOneOf(title,
        @[@"VOOM", @"LINE VOOM", @"NEWS", @"LINE NEWS",
          @"SHOPPING", @"LINE SHOPPING", @"逛逛", @"DISCOVER"]);
}

static inline BOOL NEINIsSettingsTabTitle(NSString *title) {
    return NEINTabTitleIsOneOf(title,
        @[@"SETTINGS", @"設定", @"设置", @"設置", @"설정"]);
}

// A bar button spells out its own wording, so its title and label are searched
// rather than matched whole.
static inline BOOL NEINTextMentionsSettings(NSString *text) {
    return NEINTextHasToken(text, @[@"設定", @"设置", @"settings", @"설정"]);
}

static inline BOOL NEINTextMentionsCalendar(NSString *text) {
    return NEINTextHasToken(text,
        @[@"行事曆", @"行事历", @"カレンダー", @"calendar", @"캘린더", @"ปฏิทิน"]);
}

// Title for NEIN's own settings entries (Settings tab, iPhone Duo Home gear).
static inline NSString *NEINLineSettingsTitle(void) {
    NSString *language = NSBundle.mainBundle.preferredLocalizations.firstObject.lowercaseString;
    if ([language hasPrefix:@"en"]) return @"Settings";
    if ([language hasPrefix:@"zh-hans"]) return @"设置";
    if ([language hasPrefix:@"ko"]) return @"설정";
    return @"設定";
}
