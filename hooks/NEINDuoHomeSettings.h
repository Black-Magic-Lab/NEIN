// LINE's Home page can show no settings entry in its navigation bar on iPhone
// Duo. Pin a gear to the Home root page that opens LINE's settings, and step
// aside again as soon as LINE shows its own settings item there.
//
// With the system vertical bar the gear shares its pinned group with mirrors of
// LINE's own buttons; NEINDuoVerticalBar.h gathers those and places the host,
// and NEINDuoHomeButtons.h keeps them in step with layout.
#pragma once

#import <UIKit/UIKit.h>
#include "NEINObjCRuntime.h"
#include "NEINLineSettings.h"
#include "NEINLineStrings.h"

// iPhone Duo ships with iOS 27.1; NEINInstallIPhoneDuoCompat checks it once.
#pragma clang attribute push (__attribute__((availability(ios, introduced=27.1))), apply_to = any(function, objc_method, objc_interface))

static NSString * const NEINDuoSettingsIdentifier = @"nein.duo.settings";

@interface NEINDuoHomeSettings : NSObject
@property(nonatomic, strong) UIBarButtonItem *button;
// The host's pinned group before NEIN wrapped it, and the group that replaced it.
@property(nonatomic, strong) UIBarButtonItemGroup *originalGroup;
// A bar button item belongs to one group at a time, so wrapping empties the original.
@property(nonatomic, copy) NSArray<UIBarButtonItem *> *originalItems;
@property(nonatomic, strong) UIBarButtonItemGroup *installedGroup;
@property(nonatomic, copy) NSArray<UIBarButtonItem *> *installedItems;
@end

@implementation NEINDuoHomeSettings
@end

static char NEINDuoHomeSettingsKey;
static __weak UITabBarController *NEINDuoHomeTabs;
static __weak UINavigationItem *NEINDuoHomeHost;

typedef void (*NEINDuoTabBarLayoutIMP)(id, SEL);
static NEINDuoTabBarLayoutIMP NEINDuoOriginalTabBarLayout;

static BOOL NEINDuoIsLineSettingsItem(UIBarButtonItem *item) {
    // The gear, its representative and mirrored buttons are NEIN's, not LINE's.
    if (!item || [item.accessibilityIdentifier hasPrefix:@"nein.duo."]) return NO;
    return NEINTextMentionsSettings([NSString stringWithFormat:@"%@ %@", item.title ?: @"",
                                                               item.accessibilityLabel ?: @""]);
}

static BOOL NEINDuoGroupHasLineSettings(UIBarButtonItemGroup *group) {
    for (UIBarButtonItem *item in group.barButtonItems) {
        if (NEINDuoIsLineSettingsItem(item)) return YES;
    }
    return NEINDuoIsLineSettingsItem(group.representativeItem);
}

static BOOL NEINDuoHasLineSettings(UINavigationItem *item) {
    if (NEINDuoGroupHasLineSettings(item.pinnedTrailingGroup)) return YES;
    for (UIBarButtonItemGroup *group in item.leadingItemGroups) {
        if (NEINDuoGroupHasLineSettings(group)) return YES;
    }
    for (UIBarButtonItemGroup *group in item.trailingItemGroups) {
        if (NEINDuoGroupHasLineSettings(group)) return YES;
    }
    // Plain bar button item arrays are not mirrored into the item groups.
    for (NSArray<UIBarButtonItem *> *buttons in @[item.leftBarButtonItems ?: @[],
                                                  item.rightBarButtonItems ?: @[]]) {
        for (UIBarButtonItem *button in buttons) {
            if (NEINDuoIsLineSettingsItem(button)) return YES;
        }
    }
    return NO;
}

// The Home tab's root page only; pages pushed on top keep their own items.
static UINavigationItem *NEINDuoHomeNavigationItem(UITabBarController *controller,
                                                   UIViewController **tab) {
    NSArray<UIViewController *> *controllers = controller.viewControllers;
    NSArray<UITabBarItem *> *items = controller.tabBar.items;
    if (controllers.count != items.count) return nil;
    for (NSUInteger i = 0; i < items.count; i++) {
        if (!NEINIsHomeTabTitle(items[i].title)) continue;
        UIViewController *home = controllers[i];
        UINavigationController *navigation = [home isKindOfClass:UINavigationController.class]
            ? (UINavigationController *)home : home.navigationController;
        UIViewController *root = navigation.viewControllers.firstObject;
        if (tab) *tab = home;
        return root && navigation.topViewController == root ? root.navigationItem : nil;
    }
    return nil;
}

static SEL NEINDuoOpenSettingsSelector(void) {
    return NSSelectorFromString(@"nein_openHomeSettings:");
}

static UIBarButtonItem *NEINDuoSettingsButton(UITabBarController *target, NSString *identifier) {
    NSString *title = NEINLineSettingsTitle();
    UIBarButtonItem *button = [[UIBarButtonItem alloc]
        initWithImage:[UIImage systemImageNamed:@"gearshape"]
                style:UIBarButtonItemStylePlain
               target:target
               action:NEINDuoOpenSettingsSelector()];
    button.title = title;
    button.accessibilityLabel = title;
    button.accessibilityIdentifier = identifier;
    // Keep the gear out of the overflow menu on the narrow folded display.
    button.visibilityPriority = UIBarButtonItemVisibilityPriorityHigh;
    return button;
}

static void NEINDuoRemoveHomeButtons(UINavigationItem *host) {
    NEINDuoHomeSettings *settings = objc_getAssociatedObject(host, &NEINDuoHomeSettingsKey);
    if (!settings) return;
    if (host.pinnedTrailingGroup == settings.installedGroup) {
        settings.originalGroup.barButtonItems = settings.originalItems ?: @[];
        host.pinnedTrailingGroup = settings.originalGroup;
    }
    objc_setAssociatedObject(host, &NEINDuoHomeSettingsKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static void NEINDuoInstallHomeButtons(UINavigationItem *host, UITabBarController *controller,
                                      NSArray<UIBarButtonItem *> *mirrored, BOOL gear) {
    NEINDuoHomeSettings *settings = objc_getAssociatedObject(host, &NEINDuoHomeSettingsKey);
    BOOL pinned = settings && host.pinnedTrailingGroup == settings.installedGroup;
    if (!mirrored.count && !gear) {
        NEINDuoRemoveHomeButtons(host);
        return;
    }
    if (!settings) settings = [NEINDuoHomeSettings new];
    if (!settings.button) {
        settings.button = NEINDuoSettingsButton(controller, NEINDuoSettingsIdentifier);
    }
    NSArray<UIBarButtonItem *> *items = gear
        ? [mirrored arrayByAddingObject:settings.button] : mirrored;
    if (pinned && [settings.installedItems isEqualToArray:items]) return;
    // No group yet, LINE replaced the pinned group, or the items changed.
    UIBarButtonItemGroup *original = pinned ? settings.originalGroup : host.pinnedTrailingGroup;
    NSArray<UIBarButtonItem *> *originalItems =
        pinned ? settings.originalItems : original.barButtonItems;
    NSMutableArray<UIBarButtonItem *> *buttons = [originalItems mutableCopy] ?: [NSMutableArray new];
    for (UIBarButtonItem *item in items) {
        if (![buttons containsObject:item]) [buttons addObject:item];
    }
    UIBarButtonItem *representative = original.representativeItem;
    if (!representative && buttons.count > 1) {
        representative = NEINDuoSettingsButton(
            controller, [NEINDuoSettingsIdentifier stringByAppendingString:@".representative"]);
    }
    settings.originalGroup = original;
    settings.originalItems = originalItems;
    settings.installedItems = items;
    settings.installedGroup = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:buttons
                                                               representativeItem:representative];
    objc_setAssociatedObject(host, &NEINDuoHomeSettingsKey, settings,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    host.pinnedTrailingGroup = settings.installedGroup;
}

#pragma clang attribute pop
