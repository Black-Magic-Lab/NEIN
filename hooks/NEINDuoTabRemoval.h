// The system vertical bar on iPhone Duo draws its tabs from the tab bar
// controller, so NEIN's separate presentation bar cannot filter it. Remove the
// promotional tabs from the controller instead. LINE selects tabs by index, so
// calls from LINE's own code keep seeing the unfiltered lists and their indices.
// Requires NEINVisibleTabModel.h.
#pragma once

#import <UIKit/UIKit.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include "NEINObjCRuntime.h"
#include "NEINVisibleTabModel.h"

static char NEINDuoUnfilteredTabsKey;

static void (*NEINDuoOriginalSetTabs)(id, SEL, NSArray *, BOOL);
static void (*NEINDuoOriginalSetTabsPlain)(id, SEL, NSArray *);
static NSArray *(*NEINDuoOriginalTabs)(id, SEL);
static void (*NEINDuoOriginalSetTabIndex)(id, SEL, NSUInteger);
static NSUInteger (*NEINDuoOriginalTabIndex)(id, SEL);
static void (*NEINDuoOriginalTabRemovalLayout)(id, SEL);
static NSArray *(*NEINDuoOriginalBarItems)(id, SEL);

// Whether a hooked method was called from the main executable (LINE).
static BOOL NEINDuoCallerIsLine(void *returnAddress) {
    static uintptr_t start, end;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        unsigned long size = 0;
        uint8_t *text = getsegmentdata(
            (const struct mach_header_64 *)_dyld_get_image_header(0), "__TEXT", &size);
        start = (uintptr_t)text;
        end = start + size;
    });
    uintptr_t address = (uintptr_t)returnAddress;
    return address >= start && address < end;
}

static NSArray<UIViewController *> *NEINDuoUnfilteredTabs(UITabBarController *controller) {
    return objc_getAssociatedObject(controller, &NEINDuoUnfilteredTabsKey);
}

// The tabs to keep, or nil when this is not LINE's main bar or nothing is removed.
static NSArray<UIViewController *> *NEINDuoFilteredTabs(NSArray<UIViewController *> *controllers) {
    NSMutableArray<UITabBarItem *> *items = [NSMutableArray new];
    for (UIViewController *controller in controllers) {
        if (![controller isKindOfClass:UIViewController.class]) return nil;
        [items addObject:controller.tabBarItem];
    }
    if (!NEINIsMainLineTabBar(items)) return nil;
    NSArray<NSNumber *> *indices = NEINVisibleTabIndices(items);
    if (indices.count < 2 || indices.count == items.count) return nil;
    NSMutableArray<UIViewController *> *filtered = [NSMutableArray new];
    for (NSNumber *index in indices) [filtered addObject:controllers[index.unsignedIntegerValue]];
    return filtered;
}

// Returns the list UIKit should get and records the unfiltered one.
static NSArray *NEINDuoPrepareTabs(UITabBarController *controller, NSArray *requested) {
    NSArray *filtered = [requested isKindOfClass:NSArray.class] ? NEINDuoFilteredTabs(requested) : nil;
    if (filtered) {
        objc_setAssociatedObject(controller, &NEINDuoUnfilteredTabsKey, [requested copy],
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return filtered;
    }
    // UIKit forwards the filtered list between its own setters; keep the record then.
    NSArray *unfiltered = NEINDuoUnfilteredTabs(controller);
    if (unfiltered && ![NEINDuoFilteredTabs(unfiltered) isEqualToArray:requested ?: @[]]) {
        objc_setAssociatedObject(controller, &NEINDuoUnfilteredTabsKey, nil,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return requested;
}

static void NEINDuoSetTabs(UITabBarController *self, SEL selector, NSArray *requested, BOOL animated) {
    NEINDuoOriginalSetTabs(self, selector, NEINDuoPrepareTabs(self, requested), animated);
}

static void NEINDuoSetTabsPlain(UITabBarController *self, SEL selector, NSArray *requested) {
    NEINDuoOriginalSetTabsPlain(self, selector, NEINDuoPrepareTabs(self, requested));
}

static NSArray *NEINDuoTabs(UITabBarController *self, SEL selector) {
    NSArray *unfiltered = NEINDuoUnfilteredTabs(self);
    if (unfiltered && NEINDuoCallerIsLine(__builtin_return_address(0))) return unfiltered;
    return NEINDuoOriginalTabs(self, selector);
}

static void NEINDuoSetTabIndex(UITabBarController *self, SEL selector, NSUInteger requested) {
    NSArray *unfiltered = NEINDuoUnfilteredTabs(self);
    if (unfiltered && requested < unfiltered.count &&
            NEINDuoCallerIsLine(__builtin_return_address(0))) {
        NSUInteger index = [NEINDuoOriginalTabs(self, @selector(viewControllers))
            indexOfObjectIdenticalTo:unfiltered[requested]];
        // A removed tab has nothing to show; stay on the current one.
        if (index == NSNotFound) return;
        requested = index;
    }
    NEINDuoOriginalSetTabIndex(self, selector, requested);
}

static NSUInteger NEINDuoTabIndex(UITabBarController *self, SEL selector) {
    NSArray *unfiltered = NEINDuoUnfilteredTabs(self);
    if (unfiltered && NEINDuoCallerIsLine(__builtin_return_address(0))) {
        UIViewController *selected = self.selectedViewController;
        return selected ? [unfiltered indexOfObjectIdenticalTo:selected] : NSNotFound;
    }
    return NEINDuoOriginalTabIndex(self, selector);
}

// LINE assigns tab artwork by position in the bar's item list.
static NSArray *NEINDuoBarItems(UITabBar *self, SEL selector) {
    NSArray *items = NEINDuoOriginalBarItems(self, selector);
    id owner = self.delegate;
    if (![owner isKindOfClass:UITabBarController.class] ||
            !NEINDuoCallerIsLine(__builtin_return_address(0))) return items;
    NSArray<UIViewController *> *unfiltered = NEINDuoUnfilteredTabs(owner);
    if (!unfiltered) return items;
    NSMutableArray<UITabBarItem *> *all = [NSMutableArray new];
    for (UIViewController *controller in unfiltered) [all addObject:controller.tabBarItem];
    return all;
}

// LINE can title its tabs after handing them over; filter again once they are known.
static void NEINDuoTabRemovalDidLayoutSubviews(UITabBarController *self, SEL selector) {
    NEINDuoOriginalTabRemovalLayout(self, selector);
    if (NEINDuoUnfilteredTabs(self)) return;
    NSArray<UIViewController *> *current = NEINDuoOriginalTabs(self, @selector(viewControllers));
    NSArray<UIViewController *> *filtered = NEINDuoFilteredTabs(current);
    if (!filtered) return;
    UIViewController *selected = self.selectedViewController;
    objc_setAssociatedObject(self, &NEINDuoUnfilteredTabsKey, [current copy],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    NEINDuoOriginalSetTabs(self, @selector(setViewControllers:animated:), filtered, NO);
    if (selected && [filtered indexOfObjectIdenticalTo:selected] != NSNotFound) {
        self.selectedViewController = selected;
    }
}

static BOOL NEINInstallDuoTabRemoval(void) {
    Class controller = UITabBarController.class;
    SEL setTabs = @selector(setViewControllers:animated:);
    SEL setTabsPlain = @selector(setViewControllers:);
    SEL tabs = @selector(viewControllers);
    SEL setIndex = @selector(setSelectedIndex:);
    SEL index = @selector(selectedIndex);
    SEL layout = @selector(viewDidLayoutSubviews);
    SEL items = @selector(items);
    // Check every signature first so the hooks are installed together or not at all.
    if (!NEINMethodHasType(class_getInstanceMethod(controller, setTabs), "v", 4, "@", @encode(BOOL)) ||
            !NEINMethodHasType(class_getInstanceMethod(controller, setTabsPlain), "v", 3, "@", NULL) ||
            !NEINMethodHasType(class_getInstanceMethod(controller, tabs), "@", 2, NULL, NULL) ||
            !NEINMethodHasType(class_getInstanceMethod(controller, setIndex), "v", 3,
                               @encode(NSUInteger), NULL) ||
            !NEINMethodHasType(class_getInstanceMethod(controller, index), @encode(NSUInteger), 2,
                               NULL, NULL) ||
            !NEINMethodHasType(class_getInstanceMethod(controller, layout), "v", 2, NULL, NULL) ||
            !NEINMethodHasType(class_getInstanceMethod(UITabBar.class, items), "@", 2, NULL, NULL)) {
        NSLog(@"[NEINIPhoneDuo] Tab bar controller changed; promotional tabs stay in the vertical bar.");
        return NO;
    }
    NEINHookOwnMethod(controller, setTabs, "v", 4, "@", @encode(BOOL),
                      (IMP)NEINDuoSetTabs, (IMP *)&NEINDuoOriginalSetTabs);
    NEINHookOwnMethod(controller, setTabsPlain, "v", 3, "@", NULL,
                      (IMP)NEINDuoSetTabsPlain, (IMP *)&NEINDuoOriginalSetTabsPlain);
    NEINHookOwnMethod(controller, tabs, "@", 2, NULL, NULL,
                      (IMP)NEINDuoTabs, (IMP *)&NEINDuoOriginalTabs);
    NEINHookOwnMethod(controller, setIndex, "v", 3, @encode(NSUInteger), NULL,
                      (IMP)NEINDuoSetTabIndex, (IMP *)&NEINDuoOriginalSetTabIndex);
    NEINHookOwnMethod(controller, index, @encode(NSUInteger), 2, NULL, NULL,
                      (IMP)NEINDuoTabIndex, (IMP *)&NEINDuoOriginalTabIndex);
    NEINHookOwnMethod(controller, layout, "v", 2, NULL, NULL,
                      (IMP)NEINDuoTabRemovalDidLayoutSubviews,
                      (IMP *)&NEINDuoOriginalTabRemovalLayout);
    NEINHookOwnMethod(UITabBar.class, items, "@", 2, NULL, NULL,
                      (IMP)NEINDuoBarItems, (IMP *)&NEINDuoOriginalBarItems);
    return YES;
}
