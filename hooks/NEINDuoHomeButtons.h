// Keeps Home's gear and the mirrored LINE buttons in step with the tab bar controller's layout.
#pragma once

#include "NEINDuoHomeSettings.h"
#include "NEINDuoVerticalBar.h"

// iPhone Duo ships with iOS 27.1; NEINInstallIPhoneDuoCompat checks it once.
#pragma clang attribute push (__attribute__((availability(ios, introduced=27.1))), apply_to = any(function, objc_method, objc_interface))

static void NEINDuoUpdateHomeButtons(UITabBarController *tabs) {
    UIViewController *homeTab = nil;
    UINavigationItem *home = NEINDuoHomeNavigationItem(tabs, &homeTab);
    // Other tab bar controllers leave the main one's buttons alone.
    if (!homeTab || (NEINDuoHomeTabs && NEINDuoHomeTabs != tabs && !home)) return;
    NEINDuoHomeTabs = tabs;
    BOOL gear = home && !NEINDuoHasLineSettings(home);
    NEINDuoUpdateVerticalBar(tabs, homeTab, home, gear);
}

static void NEINDuoTabBarDidLayoutSubviews(UITabBarController *self, SEL selector) {
    NEINDuoOriginalTabBarLayout(self, selector);
    if (self == NEINDuoProxyTabs) return;
    NEINDuoUpdateHomeButtons(self);
}

static void NEINDuoOpenHomeSettings(UITabBarController *self, SEL selector, id sender) {
    (void)selector;
    (void)sender;
    NEINOpenLineSettings(self);
}

static BOOL NEINInstallDuoHomeSettings(void) {
    Class controller = UITabBarController.class;
    if (class_addMethod(controller, NEINDuoOpenSettingsSelector(),
                        (IMP)NEINDuoOpenHomeSettings, "v@:@") &&
            NEINHookOwnMethod(controller, @selector(viewDidLayoutSubviews), "v", 2, NULL, NULL,
                              (IMP)NEINDuoTabBarDidLayoutSubviews,
                              (IMP *)&NEINDuoOriginalTabBarLayout)) {
        NEINDuoInstallVerticalBarHooks();
        return YES;
    }
    NSLog(@"[NEINIPhoneDuo] Tab bar layout unavailable; Home keeps LINE's own navigation items.");
    return NO;
}

#pragma clang attribute pop
