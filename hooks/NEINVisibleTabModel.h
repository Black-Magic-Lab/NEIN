#pragma once
// Keep model indices intact. Only the separate presentation bar is filtered.
#include "NEINLineStrings.h"

static BOOL NEINIsPromotionalTabItem(UITabBarItem *item) {
    // LINE 26.14.0 includes this specific item class. Do not match generic
    // TabBar/TabItem names or infer item identity from view positions.
    return NEINIsPromotionalTabTitle(item.title) ||
           [NSStringFromClass(item.class) hasSuffix:@"VOOMSkinnedTabBarItem"];
}

static BOOL NEINIsMainLineTabBar(NSArray<UITabBarItem *> *items) {
    BOOL hasHome = NO, hasChat = NO, hasCalls = NO;
    for (UITabBarItem *item in items) {
        NSString *title = item.title;
        hasHome |= NEINIsHomeTabTitle(title);
        hasChat |= NEINIsChatTabTitle(title);
        hasCalls |= NEINIsCallTabTitle(title);
    }
    return hasHome && hasChat && hasCalls;
}

static NSArray<NSNumber *> *NEINVisibleTabIndices(NSArray<UITabBarItem *> *items) {
    NSMutableArray *indices = [NSMutableArray new];
    for (NSUInteger i = 0; i < items.count; i++) {
        if (!NEINIsPromotionalTabItem(items[i])) [indices addObject:@(i)];
    }
    return indices;
}

static NSUInteger NEINVisibleTabDestination(NSArray<UITabBarItem *> *items,
                                          NSUInteger current, NSUInteger requested) {
    if (requested >= items.count) return NSNotFound;
    if (!NEINIsPromotionalTabItem(items[requested]) && items[requested].enabled) return requested;
    NSInteger direction = current < items.count && requested < current ? -1 : 1;
    for (NSInteger i = (NSInteger)requested + direction;
         i >= 0 && (NSUInteger)i < items.count; i += direction) {
        if (!NEINIsPromotionalTabItem(items[i]) && items[i].enabled) return (NSUInteger)i;
    }
    if (current < items.count && !NEINIsPromotionalTabItem(items[current]) &&
        items[current].enabled) return current;
    for (NSUInteger i = 0; i < items.count; i++) {
        if (!NEINIsPromotionalTabItem(items[i]) && items[i].enabled) return i;
    }
    return NSNotFound;
}
