// iOS 27.1's system vertical bar serves only the page at the window's trailing
// edge, and only its plain bar button items. LINE packs Home's buttons into one
// custom view, and the chat list and chat rooms hide the navigation bar to draw
// their own, so those buttons are mirrored as plain items next to the gear. On
// the unfolded split layout only the detail column's buttons move, and a system
// tab bar wrapped around the split (or stand-ins, where it cannot be) carries
// the primary column's tabs.
//
// Builds on NEINDuoHomeSettings.h (the gear and the Home host); included by
// NEINDuoHomeButtons.h, whose tab bar layout hook all of this hangs on.
#pragma once

#import <UIKit/UIKit.h>
#include "NEINDuoHomeSettings.h"

// iPhone Duo ships with iOS 27.1; NEINInstallIPhoneDuoCompat checks it once.
#pragma clang attribute push (__attribute__((availability(ios, introduced=27.1))), apply_to = any(function, objc_method, objc_interface))

// Defined by NEINDuoHomeButtons.h below its include of this file; the hooks here ask for a
// fresh pass whenever LINE moves buttons without a layout above them.
static void NEINDuoUpdateHomeButtons(UITabBarController *tabs);

static NSString * const NEINDuoMirrorIdentifier = @"nein.duo.line";
static NSString * const NEINDuoTabIdentifier = @"nein.duo.tab";
static char NEINDuoMirrorItemKey;
static char NEINDuoTabItemsKey;
static char NEINDuoShownBarKey;

// The navigation controller that shows a page's bar for NEIN, held weakly: a
// fold moves pages between navigation controllers and frees the old one.
static void NEINDuoSetShownBar(UIViewController *page, UINavigationController *navigation) {
    NSPointerArray *box = nil;
    if (navigation) {
        box = [NSPointerArray weakObjectsPointerArray];
        [box addPointer:(__bridge void *)navigation];
    }
    objc_setAssociatedObject(page, &NEINDuoShownBarKey, box, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static UINavigationController *NEINDuoShownBar(UIViewController *page) {
    NSPointerArray *box = objc_getAssociatedObject(page, &NEINDuoShownBarKey);
    return box.count ? (__bridge UINavigationController *)[box pointerAtIndex:0] : nil;
}
static char NEINDuoHiddenLeftItemsKey;
static char NEINDuoFadedKey;
static __weak UIViewController *NEINDuoShownBarPage;
static __weak UIViewController *NEINDuoToolbarPage;
static __weak UINavigationController *NEINDuoToolbarNavigation;
static __weak UIViewController *NEINDuoChatRoomPage;
static __weak UINavigationItem *NEINDuoGearHost;
static __weak UIViewController *NEINDuoHomeHostPage;
static BOOL NEINDuoHidTabBar;
static __weak UIViewController *NEINDuoDetailTopPage;
static BOOL NEINDuoDetailBarHidden;
static NEINDuoTabBarLayoutIMP NEINDuoOriginalNavigationLayout;

static void NEINDuoCollectButtons(UIView *view, NSMutableArray<UIButton *> *buttons) {
    for (UIView *subview in view.subviews) {
        if ([subview isKindOfClass:UIButton.class]) [buttons addObject:(UIButton *)subview];
        else NEINDuoCollectButtons(subview, buttons);
    }
}

static UIImage *NEINDuoControlImage(UIControl *control) {
    if ([control isKindOfClass:UIButton.class] && ((UIButton *)control).currentImage) {
        return ((UIButton *)control).currentImage;
    }
    NSMutableArray<UIView *> *queue = [control.subviews mutableCopy];
    while (queue.count) {
        UIView *view = queue.firstObject;
        [queue removeObjectAtIndex:0];
        if ([view isKindOfClass:UIImageView.class] && !view.hidden && ((UIImageView *)view).image) {
            return ((UIImageView *)view).image;
        }
        [queue addObjectsFromArray:view.subviews];
    }
    return nil;
}

@interface NEINDuoMirrorTarget : NSObject
@property(nonatomic, weak) UIControl *source;
// Dismissed instead while no source has come yet.
@property(nonatomic, weak) UIViewController *presented;
@end

@implementation NEINDuoMirrorTarget
@end

static char NEINDuoMirrorTargetKey;

static UIBarButtonItem *NEINDuoNewMirrorItem(UIControl *source, UIViewController *presented) {
    NEINDuoMirrorTarget *target = [NEINDuoMirrorTarget new];
    target.source = source;
    target.presented = presented;
    UIBarButtonItem *item = [[UIBarButtonItem alloc] initWithPrimaryAction:
        [UIAction actionWithTitle:@"" image:nil identifier:nil handler:^(UIAction *action) {
            (void)action;
            if (target.source) {
                [target.source sendActionsForControlEvents:UIControlEventTouchUpInside];
            } else {
                [target.presented dismissViewControllerAnimated:YES completion:nil];
            }
        }]];
    item.accessibilityIdentifier = NEINDuoMirrorIdentifier;
    objc_setAssociatedObject(item, &NEINDuoMirrorTargetKey, target, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return item;
}

static UIBarButtonItem *NEINDuoMirrorItem(UIControl *control) {
    UIBarButtonItem *item = objc_getAssociatedObject(control, &NEINDuoMirrorItemKey);
    if (!item) {
        item = NEINDuoNewMirrorItem(control, nil);
        objc_setAssociatedObject(control, &NEINDuoMirrorItemKey, item,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    // LINE redraws its images for badges, so follow the source on every pass.
    UIImage *image = NEINDuoControlImage(control);
    if (item.image != image) item.image = image;
    NSString *label = control.accessibilityLabel;
    if (label != item.accessibilityLabel && ![label isEqualToString:item.accessibilityLabel]) {
        item.title = label;
        item.accessibilityLabel = label;
    }
    if (item.enabled != control.enabled) item.enabled = control.enabled;
    return item;
}

static NSArray<UIBarButtonItem *> *NEINDuoMirrorControls(NSArray<UIControl *> *controls) {
    NSMutableArray<UIBarButtonItem *> *mirrored = [NSMutableArray new];
    for (UIControl *control in controls) [mirrored addObject:NEINDuoMirrorItem(control)];
    return mirrored;
}

static void NEINDuoSortByX(NSMutableArray<UIControl *> *controls, UIView *root) {
    [controls sortUsingComparator:^NSComparisonResult(UIControl *a, UIControl *b) {
        return [@(CGRectGetMinX([a convertRect:a.bounds toView:root]))
                   compare:@(CGRectGetMinX([b convertRect:b.bounds toView:root]))];
    }];
}

// A container's own visible buttons, in screen order.
static NSMutableArray<UIControl *> *NEINDuoVisibleButtons(UIView *container) {
    NSMutableArray<UIControl *> *buttons = [NSMutableArray new];
    for (UIView *subview in container.subviews) {
        if ([subview isKindOfClass:UIButton.class] && !subview.hidden) {
            [buttons addObject:(UIControl *)subview];
        }
    }
    NEINDuoSortByX(buttons, container);
    return buttons;
}

// True when every control below view is mirrored and no text sits outside them.
static BOOL NEINDuoOnlyMirrored(UIView *view, NSArray<UIControl *> *controls) {
    if (view.hidden) return YES;
    if ([view isKindOfClass:UIControl.class]) return [controls containsObject:(UIControl *)view];
    if ([view isKindOfClass:UILabel.class] && !view.hidden && ((UILabel *)view).text.length) return NO;
    for (UIView *subview in view.subviews) {
        if (!NEINDuoOnlyMirrored(subview, controls)) return NO;
    }
    return YES;
}

// Fade the mirrored controls' own pill rather than hiding them: LINE uses
// hidden to drop buttons a chat does not offer.
static NSArray<UIView *> *NEINDuoFadeSources(NSArray<UIControl *> *controls, UIView *root) {
    if (!controls.count) return @[];
    UIView *common = controls.firstObject;
    for (UIControl *control in controls) {
        while (common && ![control isDescendantOfView:common]) common = common.superview;
    }
    if (!common || common == root) {
        for (UIControl *control in controls) control.alpha = 0;
        return controls;
    }
    while (common.superview && common.superview != root &&
           CGRectGetWidth(common.superview.bounds) < CGRectGetWidth(root.bounds) * 0.6 &&
           NEINDuoOnlyMirrored(common.superview, controls)) {
        common = common.superview;
    }
    common.alpha = 0;
    return @[common];
}

// Message lists hold no bar buttons, and walking their cells on every layout costs.
static void NEINDuoFindClass(UIView *view, NSString *suffix, NSMutableArray<UIView *> *found) {
    if ([NSStringFromClass(view.class) hasSuffix:suffix]) {
        [found addObject:view];
        return;
    }
    for (UIView *subview in view.subviews) {
        if (![subview isKindOfClass:UIScrollView.class]) NEINDuoFindClass(subview, suffix, found);
    }
}

// The header container caches its buttons and width, so it keeps a removed
// button's slot. Its packing (first x, stride, removed width) is recorded at
// removal and reapplied after its own layout.
static char NEINDuoPackingKey;
static CGSize (*NEINDuoOriginalContainerSize)(id, SEL);
static NEINDuoTabBarLayoutIMP NEINDuoOriginalContainerLayout;

static CGRect NEINDuoPacking(UIView *container) {
    NSValue *value = objc_getAssociatedObject(container, &NEINDuoPackingKey);
    return value ? value.CGRectValue : CGRectZero;
}

static CGSize NEINDuoContainerSize(UIView *self, SEL selector) {
    CGSize size = NEINDuoOriginalContainerSize(self, selector);
    CGFloat removed = NEINDuoPacking(self).size.height;
    if (removed > 0 && size.width > removed) size.width -= removed;
    return size;
}

static void NEINDuoContainerLayout(UIView *self, SEL selector) {
    NEINDuoOriginalContainerLayout(self, selector);
    CGRect packing = NEINDuoPacking(self);
    if (packing.size.width <= 0) return;
    NSArray<UIControl *> *buttons = NEINDuoVisibleButtons(self);
    [buttons enumerateObjectsUsingBlock:^(UIControl *button, NSUInteger index, BOOL *stop) {
        (void)stop;
        CGRect frame = button.frame;
        frame.origin.x = packing.origin.x + index * packing.size.width;
        button.frame = frame;
    }];
}

static BOOL NEINDuoHookContainer(Class container) {
    static Class hooked;
    if (hooked) return hooked == container;
    if (!NEINHookOwnMethod(container, @selector(intrinsicContentSize), @encode(CGSize), 2, NULL, NULL,
                           (IMP)NEINDuoContainerSize, (IMP *)&NEINDuoOriginalContainerSize)) {
        return NO;
    }
    if (!NEINHookOwnMethod(container, @selector(layoutSubviews), "v", 2, NULL, NULL,
                           (IMP)NEINDuoContainerLayout, (IMP *)&NEINDuoOriginalContainerLayout)) {
        NSLog(@"[NEINIPhoneDuo] Header layout unavailable; removed buttons leave a gap.");
    }
    hooked = container;
    return YES;
}

// The chat list header's visible buttons, nil on other pages. LINE shows a
// calendar there only until the list first refreshes; removing it keeps the
// header and the vertical bar from changing under the user. Hiding it would
// leave a gap, since the header does not reflow hidden buttons.
static NSArray<UIButton *> *NEINDuoChatListButtons(UIViewController *page) {
    if (![NSStringFromClass(page.class) hasSuffix:@"ChatTabPageViewController"] || !page.viewIfLoaded) {
        return nil;
    }
    NSMutableArray<UIView *> *containers = [NSMutableArray new];
    NEINDuoFindClass(page.view, @"BarButtonItemGroupContainerView", containers);
    NSMutableArray<UIButton *> *buttons = [NSMutableArray new];
    if (containers.count) NEINDuoCollectButtons(containers.firstObject, buttons);
    NSMutableArray<UIButton *> *visible = [NSMutableArray new];
    BOOL removed = NO;
    UIView *container = containers.firstObject;
    for (UIButton *button in buttons) {
        if (NEINTextMentionsCalendar(button.accessibilityLabel)) {
            if (button.superview == container && NEINDuoHookContainer(container.class)) {
                NSArray<UIControl *> *sorted = NEINDuoVisibleButtons(container);
                CGRect packing = NEINDuoPacking(container);
                if (sorted.count >= 2) {
                    packing.origin.x = CGRectGetMinX(sorted[0].frame);
                    packing.size.width = CGRectGetMinX(sorted[1].frame) - packing.origin.x;
                    packing.size.height += packing.size.width;
                    objc_setAssociatedObject(container, &NEINDuoPackingKey,
                                             [NSValue valueWithCGRect:packing],
                                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                }
            }
            [button removeFromSuperview];
            removed = YES;
        } else if (!button.hidden) {
            [visible addObject:button];
        }
    }
    if (removed) {
        for (UIView *view = containers.firstObject; view && view != page.view; view = view.superview) {
            [view invalidateIntrinsicContentSize];
            [view setNeedsLayout];
        }
    }
    return visible;
}

static char NEINDuoMovedLeadingKey;

// Icon-only custom buttons among items, hidden in place: custom views cannot
// move into the vertical bar.
static NSArray<UIControl *> *NEINDuoIconControls(NSArray<UIBarButtonItem *> *items) {
    NSMutableArray<UIControl *> *controls = [NSMutableArray new];
    for (UIBarButtonItem *item in items) {
        UIControl *control = [item.customView isKindOfClass:UIControl.class] ? (UIControl *)item.customView : nil;
        BOOL moved = objc_getAssociatedObject(item, &NEINDuoMovedLeadingKey) != nil;
        if (!control || (item.hidden && !moved) || !NEINDuoControlImage(control)) continue;
        if ([control isKindOfClass:UIButton.class] && ((UIButton *)control).currentTitle.length) continue;
        objc_setAssociatedObject(item, &NEINDuoMovedLeadingKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        if (!item.hidden) item.hidden = YES;
        [controls addObject:control];
    }
    return controls;
}

// LINE's back and close buttons at a page's leading edge; the vertical bar is
// at the trailing edge.
static NSArray<UIControl *> *NEINDuoLeadingIcons(UINavigationItem *item) {
    return NEINDuoIconControls(item.leftBarButtonItems);
}

static void NEINDuoRestoreLeading(UINavigationItem *item) {
    for (UIBarButtonItem *left in item.leftBarButtonItems) {
        if (!objc_getAssociatedObject(left, &NEINDuoMovedLeadingKey)) continue;
        objc_setAssociatedObject(left, &NEINDuoMovedLeadingKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        left.hidden = NO;
    }
}

static char NEINDuoChatBackKey;
static char NEINDuoChatInsetsKey;

// Whether the system shows the vertical bar; upright on the unfolded display
// it puts a top bar and a bottom tab bar there instead.
static BOOL NEINDuoBarShown = YES;

// Once the window turns upright, UIKit still insets pages for the vertical bar
// it no longer shows (its detached bar container keeps the old insets), so they
// stop short of the trailing edge; target's sides take that back out.
static void NEINDuoUndoStaleBarInset(UIView *view, UIEdgeInsets current, UIEdgeInsets *target) {
    UIEdgeInsets safe = view.safeAreaInsets;
    UIEdgeInsets outer = view.superview.safeAreaInsets;
    target->left -= safe.left - outer.left - current.left;
    target->right -= safe.right - outer.right - current.right;
}

// The insets LINE wants, from saved (its own and NEIN's last ones). LINE sets
// single sides again (the chat list its top, for its header), keeping NEIN's
// other sides, which would otherwise be taken for its own and pile up.
static UIEdgeInsets NEINDuoLineInsets(UIEdgeInsets current, NSArray<NSValue *> *saved) {
    if (!saved) return current;
    UIEdgeInsets line = saved[0].UIEdgeInsetsValue;
    UIEdgeInsets ours = saved[1].UIEdgeInsetsValue;
    if (current.top != ours.top) line.top = current.top;
    if (current.left != ours.left) line.left = current.left;
    if (current.bottom != ours.bottom) line.bottom = current.bottom;
    if (current.right != ours.right) line.right = current.right;
    return line;
}

static char NEINDuoListTitleKey;
static char NEINDuoListTopKey;
static BOOL NEINDuoOwnInsets;
static void (*NEINDuoOriginalSetListInsets)(id, SEL, UIEdgeInsets);

// LINE sets the chat list's top inset, room for its header row, on its own
// layout passes; while the row is lifted into the top bar that room goes, and
// LINE's value is kept to put back.
static void NEINDuoSetListInsets(UIViewController *self, SEL selector, UIEdgeInsets insets) {
    if (!NEINDuoOwnInsets) {
        objc_setAssociatedObject(self, &NEINDuoListTopKey, @(insets.top), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    if (objc_getAssociatedObject(self, &NEINDuoListTitleKey)) insets.top = 0;
    NEINDuoOriginalSetListInsets(self, selector, insets);
}

static void NEINDuoSetOwnInsets(UIViewController *page, UIEdgeInsets insets) {
    NEINDuoOwnInsets = YES;
    page.additionalSafeAreaInsets = insets;
    NEINDuoOwnInsets = NO;
}

static void NEINDuoSetListTop(UIViewController *page, CGFloat top) {
    UIEdgeInsets insets = page.additionalSafeAreaInsets;
    if (insets.top == top) return;
    insets.top = top;
    NEINDuoSetOwnInsets(page, insets);
}

// Upright, the chat list's title and its sort menu move into the system top
// bar, and LINE's header row beneath it fades out; NO when not lifted.
static BOOL NEINDuoLiftChatListTitle(UIViewController *page, BOOL lift) {
    NSArray *saved = objc_getAssociatedObject(page, &NEINDuoListTitleKey);
    UINavigationItem *item = page.navigationItem;
    UINavigationController *navigation = page.navigationController;
    UIButton *button = nil;
    UIView *header = nil;
    if (lift && navigation.viewControllers.firstObject == page && !navigation.navigationBarHidden &&
            [NSStringFromClass(page.class) hasSuffix:@"ChatTabPageViewController"]) {
        NSMutableArray<UIView *> *found = [NSMutableArray new];
        NEINDuoFindClass(page.view, @"ChatTabNavigationTitleView", found);
        if ([found.firstObject isKindOfClass:UIButton.class]) button = (UIButton *)found.firstObject;
        header = button.superview;
        while (header && ![NSStringFromClass(header.class) hasSuffix:@"ChatTabPageHeaderView"]) {
            header = header.superview;
        }
    }
    if (!button || !header) {
        if (!saved) return NO;
        UIView *old = saved[0];
        old.alpha = 1;
        old.userInteractionEnabled = YES;
        old.accessibilityElementsHidden = NO;
        item.title = saved.count > 1 ? saved[1] : nil;
        item.titleMenuProvider = nil;
        objc_setAssociatedObject(page, &NEINDuoListTitleKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        NSNumber *top = objc_getAssociatedObject(page, &NEINDuoListTopKey);
        if (top && NEINDuoOriginalSetListInsets) NEINDuoSetListTop(page, top.doubleValue);
        return NO;
    }
    // Without the setter hook LINE would put its room back at once.
    if (!NEINDuoOriginalSetListInsets) return NO;
    if (!saved || saved[0] != header) {
        saved = item.title ? @[header, item.title] : @[header];
        objc_setAssociatedObject(page, &NEINDuoListTitleKey, saved, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        // LINE rebuilds the menu to tick the current order, so it is read on
        // every tap.
        __weak UIButton *weakButton = button;
        item.titleMenuProvider = ^UIMenu *(NSArray<UIMenuElement *> *suggested) {
            (void)suggested;
            return weakButton.menu;
        };
    }
    NSString *title = button.currentTitle;
    if (title.length && ![item.title isEqualToString:title]) item.title = title;
    if (header.alpha != 0) header.alpha = 0;
    header.userInteractionEnabled = NO;
    header.accessibilityElementsHidden = YES;
    if (!objc_getAssociatedObject(page, &NEINDuoListTopKey)) {
        objc_setAssociatedObject(page, &NEINDuoListTopKey, @(page.additionalSafeAreaInsets.top),
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    NEINDuoSetListTop(page, 0);
    return YES;
}

static char NEINDuoBarInsetKey;

// The same for pages other than chat rooms, whose insets NEINDuoTightenChat sets.
static void NEINDuoFitUprightPage(UIViewController *page) {
    if (!page) return;
    UIView *view = page.viewIfLoaded;
    BOOL upright = !NEINDuoBarShown && view.window && view.superview;
    NEINDuoLiftChatListTitle(page, upright);
    NSArray<NSValue *> *saved = objc_getAssociatedObject(page, &NEINDuoBarInsetKey);
    UIEdgeInsets current = page.additionalSafeAreaInsets;
    UIEdgeInsets line = NEINDuoLineInsets(current, saved);
    if (!upright) {
        if (!saved) return;
        if (!UIEdgeInsetsEqualToEdgeInsets(current, line)) NEINDuoSetOwnInsets(page, line);
        objc_setAssociatedObject(page, &NEINDuoBarInsetKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return;
    }
    UIEdgeInsets target = line;
    NEINDuoUndoStaleBarInset(view, current, &target);
    if (!UIEdgeInsetsEqualToEdgeInsets(current, target)) NEINDuoSetOwnInsets(page, target);
    objc_setAssociatedObject(page, &NEINDuoBarInsetKey,
                             @[[NSValue valueWithUIEdgeInsets:line], [NSValue valueWithUIEdgeInsets:target]],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// A pushed chat room's own bar, and the detail column's, sits 8pt from the
// top, which has no status bar beside it, and its input bar 20pt from the bottom instead of above the home
// indicator. LINE sets its insets per layout mode, so they are set to these
// places rather than offset from LINE's, and LINE's come back once it is no
// longer pushed.
static void NEINDuoTightenChat(UIViewController *page, UIView *chatBar, UIView *content) {
    NSArray<NSValue *> *saved = objc_getAssociatedObject(page, &NEINDuoChatInsetsKey);
    UIEdgeInsets current = page.additionalSafeAreaInsets;
    // LINE setting its insets again makes them the ones to restore.
    UIEdgeInsets line = NEINDuoLineInsets(current, saved);
    if (!chatBar) {
        if (!saved) return;
        if (!UIEdgeInsetsEqualToEdgeInsets(current, line)) page.additionalSafeAreaInsets = line;
        objc_setAssociatedObject(page, &NEINDuoChatInsetsKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return;
    }
    UIView *view = page.viewIfLoaded;
    UIView *parent = view.superview;
    CGFloat height = CGRectGetHeight(content.bounds);
    if (!view.window || !parent || height <= 0) return;
    // What the parents give, so passing again lands on the same insets.
    CGFloat top = parent.safeAreaInsets.top;
    UINavigationController *navigation = page.navigationController;
    if (navigation && !navigation.navigationBarHidden) {
        UINavigationBar *bar = navigation.navigationBar;
        top = MAX(top, CGRectGetMaxY([bar convertRect:bar.bounds toView:view]));
    }
    UIEdgeInsets target = line;
    if (NEINDuoBarShown) {
        target.top = 8 + height - top;
        target.bottom = 16 - parent.safeAreaInsets.bottom;
    } else {
        // The system top bar takes the place of LINE's faded one, and already
        // insets the room for itself.
        target.top = 0;
        NEINDuoUndoStaleBarInset(view, current, &target);
    }
    if (!UIEdgeInsetsEqualToEdgeInsets(current, target)) page.additionalSafeAreaInsets = target;
    objc_setAssociatedObject(page, &NEINDuoChatInsetsKey,
                             @[[NSValue valueWithUIEdgeInsets:line], [NSValue valueWithUIEdgeInsets:target]],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// A pushed chat room's own bar repeats the vertical bar's back button; it fades
// out, leaving LINE its hidden state, and the title moves into its place.
static void NEINDuoHideChatBack(UIViewController *page, UIView *chatBar, UIView *content) {
    NSArray<UIView *> *views = objc_getAssociatedObject(page, &NEINDuoChatBackKey);
    if (!chatBar) {
        if (!views) return;
        views[0].alpha = 1;
        views[0].userInteractionEnabled = YES;
        views[0].accessibilityElementsHidden = NO;
        views[1].transform = CGAffineTransformIdentity;
        objc_setAssociatedObject(page, &NEINDuoChatBackKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return;
    }
    if (!views) {
        UIView *back = nil;
        UIView *title = nil;
        for (UIView *view in content.subviews) {
            if (!back && [view isKindOfClass:UIControl.class] &&
                    ![NSStringFromClass(view.class) containsString:@"BadgedButton"]) {
                back = view;
            } else if (back && [view isKindOfClass:UIStackView.class] &&
                       (!title || CGRectGetMinX(view.frame) < CGRectGetMinX(title.frame))) {
                title = view;
            }
        }
        if (!back || !title || CGRectGetMinX(title.frame) <= CGRectGetMinX(back.frame)) return;
        views = @[back, title];
        objc_setAssociatedObject(page, &NEINDuoChatBackKey, views, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    UIView *back = views[0];
    UIView *title = views[1];
    if (back.alpha != 0) back.alpha = 0;
    back.userInteractionEnabled = NO;
    back.accessibilityElementsHidden = YES;
    // The back button's width follows the unread count, so the shift is redone
    // from the untransformed position.
    CGFloat shift = CGRectGetMinX(back.frame) - (title.center.x - CGRectGetWidth(title.bounds) / 2);
    if (title.transform.tx != shift) title.transform = CGAffineTransformMakeTranslation(shift, 0);
}

static char NEINDuoChatTitleKey;

// A chat room's title stops short of its own buttons, which stay laid out
// while faded for the vertical bar; it runs to the bar's edge instead.
static void NEINDuoWidenChatTitle(UIViewController *page, UIView *content) {
    NSArray<NSLayoutConstraint *> *saved = objc_getAssociatedObject(page, &NEINDuoChatTitleKey);
    if (!content) {
        if (!saved) return;
        saved[1].active = NO;
        saved[0].active = YES;
        objc_setAssociatedObject(page, &NEINDuoChatTitleKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return;
    }
    if (!saved) {
        UIStackView *title = nil;
        UIStackView *buttons = nil;
        for (UIView *view in content.subviews) {
            if (![view isKindOfClass:UIStackView.class]) continue;
            NSMutableArray<UIView *> *found = [NSMutableArray new];
            NEINDuoFindClass(view, @"BadgedButton", found);
            if (found.count) buttons = (UIStackView *)view;
            else if (!title || CGRectGetMinX(view.frame) < CGRectGetMinX(title.frame)) title = (UIStackView *)view;
        }
        NSLayoutConstraint *line = nil;
        for (NSLayoutConstraint *constraint in content.constraints) {
            BOOL forward = constraint.firstItem == title && constraint.secondItem == buttons &&
                constraint.firstAttribute == NSLayoutAttributeTrailing &&
                constraint.secondAttribute == NSLayoutAttributeLeading;
            BOOL reverse = constraint.firstItem == buttons && constraint.secondItem == title &&
                constraint.firstAttribute == NSLayoutAttributeLeading &&
                constraint.secondAttribute == NSLayoutAttributeTrailing;
            if (constraint.active && (forward || reverse)) line = constraint;
        }
        if (!title || !buttons || !line) return;
        NSLayoutConstraint *ours = [title.trailingAnchor constraintEqualToAnchor:content.trailingAnchor];
        ours.priority = line.priority;
        saved = @[line, ours];
        objc_setAssociatedObject(page, &NEINDuoChatTitleKey, saved, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    NSLayoutConstraint *line = saved[0];
    NSLayoutConstraint *ours = saved[1];
    // The title is shifted into the hidden back button's place, so its frame
    // ends that much further out.
    UIView *title = ours.firstItem;
    CGFloat constant = -16 - title.transform.tx;
    if (ours.constant != constant) ours.constant = constant;
    // LINE turns its own back on when it lays the bar out anew.
    if (line.active) line.active = NO;
    if (!ours.active) ours.active = YES;
}

static char NEINDuoLiftedBarKey;

static UIStackView *NEINDuoChatTitleStack(UIView *content) {
    UIStackView *title = nil;
    for (UIView *view in content.subviews) {
        if (![view isKindOfClass:UIStackView.class]) continue;
        NSMutableArray<UIView *> *found = [NSMutableArray new];
        NEINDuoFindClass(view, @"BadgedButton", found);
        if (!found.count && (!title || CGRectGetMinX(view.frame) < CGRectGetMinX(title.frame))) {
            title = (UIStackView *)view;
        }
    }
    return title;
}

static void NEINDuoCollectLabels(UIView *view, NSMutableArray<UILabel *> *labels) {
    for (UIView *subview in view.subviews) {
        if (subview.hidden || subview.alpha < 0.01) continue;
        if ([subview isKindOfClass:UILabel.class]) [labels addObject:(UILabel *)subview];
        else NEINDuoCollectLabels(subview, labels);
    }
}

// Upright, without the vertical bar, the system top bar carries a pushed chat
// room's back button, title and buttons, as other apps' rooms do, and LINE's
// own bar fades out beneath it.
static void NEINDuoLiftChatBar(UIViewController *page, UIView *chatBar, UIView *content) {
    NSDictionary *saved = objc_getAssociatedObject(page, &NEINDuoLiftedBarKey);
    UINavigationItem *item = page.navigationItem;
    if (!chatBar) {
        if (!saved) return;
        ((UIView *)saved[@"bar"]).alpha = 1;
        [saved[@"shade"] removeFromSuperview];
        item.title = saved[@"title"];
        item.titleView = saved[@"titleView"];
        objc_setAssociatedObject(page, &NEINDuoLiftedBarKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return;
    }
    if (!saved) {
        // The messages sit deep in the room's view, out of the top bar's scroll
        // edge effect, and LINE's bar draws no background for it, so a blur
        // keeps the title legible over them.
        UIVisualEffectView *shade = [[UIVisualEffectView alloc] initWithEffect:
            [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterial]];
        shade.userInteractionEnabled = NO;
        UILabel *label = [UILabel new];
        label.font = [UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
        label.textColor = UIColor.labelColor;
        label.lineBreakMode = NSLineBreakByTruncatingTail;
        NSMutableDictionary *values = [NSMutableDictionary dictionaryWithObject:chatBar forKey:@"bar"];
        values[@"shade"] = shade;
        values[@"label"] = label;
        values[@"title"] = item.title;
        values[@"titleView"] = item.titleView;
        saved = values;
        objc_setAssociatedObject(page, &NEINDuoLiftedBarKey, values, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    if (chatBar.alpha != 0) chatBar.alpha = 0;
    UIView *shade = saved[@"shade"];
    UILabel *label = saved[@"label"];
    // LINE picks the status bar's style for its own bar, which the blur and
    // title now stand in for.
    UIStatusBarStyle status = page.preferredStatusBarStyle;
    UIUserInterfaceStyle style = status == UIStatusBarStyleLightContent ? UIUserInterfaceStyleDark
        : status == UIStatusBarStyleDarkContent ? UIUserInterfaceStyleLight : UIUserInterfaceStyleUnspecified;
    if (shade.overrideUserInterfaceStyle != style) shade.overrideUserInterfaceStyle = style;
    if (label.overrideUserInterfaceStyle != style) label.overrideUserInterfaceStyle = style;
    UIView *root = page.view;
    if (shade.superview != root && chatBar.superview == root) [root insertSubview:shade belowSubview:chatBar];
    UINavigationBar *navigationBar = page.navigationController.navigationBar;
    CGFloat bottom = page.navigationController.navigationBarHidden ? root.safeAreaInsets.top
        : CGRectGetMaxY([navigationBar convertRect:navigationBar.bounds toView:root]);
    CGRect frame = CGRectMake(0, 0, CGRectGetWidth(root.bounds), MAX(bottom, 0));
    if (!CGRectEqualToRect(shade.frame, frame)) shade.frame = frame;
    NSMutableArray<UILabel *> *labels = [NSMutableArray new];
    NEINDuoCollectLabels(NEINDuoChatTitleStack(content), labels);
    [labels sortUsingComparator:^NSComparisonResult(UILabel *a, UILabel *b) {
        CGFloat x = [a convertPoint:CGPointZero toView:content].x;
        CGFloat y = [b convertPoint:CGPointZero toView:content].x;
        return x < y ? NSOrderedAscending : x > y ? NSOrderedDescending : NSOrderedSame;
    }];
    NSMutableArray<NSString *> *parts = [NSMutableArray new];
    for (UILabel *source in labels) {
        if (source.text.length) [parts addObject:source.text];
    }
    NSString *title = [parts componentsJoinedByString:@" "];
    if (title.length && ![item.title isEqualToString:title]) item.title = title;
    if (title.length && ![label.text isEqualToString:title]) label.text = title;
    if (item.titleView != label) item.titleView = label;
}

static NSArray<UIBarButtonItem *> *NEINDuoPageOwnButtons(UIViewController *page, UIView *chatBar) {
    // Home packs them into one custom-view bar button item.
    for (UIBarButtonItem *item in page.navigationItem.rightBarButtonItems) {
        if (![NSStringFromClass(item.customView.class) hasSuffix:@"BarButtonItemGroupContainerView"]) continue;
        NSMutableArray<UIButton *> *buttons = [NSMutableArray new];
        NEINDuoCollectButtons(item.customView, buttons);
        NEINDuoSortByX((NSMutableArray<UIControl *> *)buttons, item.customView);
        if (buttons.count && !item.hidden) item.hidden = YES;
        return NEINDuoMirrorControls(buttons);
    }
    UIView *root = page.viewIfLoaded;
    if (!root) return @[];
    // Chat rooms draw their own bar instead of the navigation bar.
    if (chatBar) {
        NSMutableArray<UIView *> *found = [NSMutableArray new];
        // Covers ChatNumberBadgedButton too, which a community chat's threads use.
        NEINDuoFindClass(chatBar, @"BadgedButton", found);
        NSMutableArray<UIControl *> *controls = [NSMutableArray new];
        for (UIView *view in found) {
            if ([view isKindOfClass:UIControl.class] && !view.hidden) [controls addObject:(UIControl *)view];
        }
        NEINDuoSortByX(controls, root);
        for (UIControl *control in controls) control.alpha = 0;
        return NEINDuoMirrorControls(controls);
    }
    // The chat list puts the same kind of container in its own header pill.
    NSArray<UIButton *> *buttons = NEINDuoChatListButtons(page);
    if (buttons) {
        NSMutableArray<UIControl *> *controls = [buttons mutableCopy];
        NEINDuoSortByX(controls, root);
        objc_setAssociatedObject(page, &NEINDuoFadedKey, NEINDuoFadeSources(controls, root),
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return NEINDuoMirrorControls(controls);
    }
    return @[];
}

static char NEINDuoMoreItemKey;
static char NEINDuoMoreSourcesKey;

// A chat room's buttons overflow the vertical bar beside the back button once
// the keyboard shortens it, so they always share one "more" button whose menu
// runs them; the menu is built on opening, from the buttons of the latest pass.
static UIBarButtonItem *NEINDuoChatMoreItem(UIViewController *page, NSArray<UIBarButtonItem *> *items) {
    UIBarButtonItem *more = objc_getAssociatedObject(page, &NEINDuoMoreItemKey);
    if (!more) {
        more = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"ellipsis"] menu:nil];
        more.accessibilityIdentifier = [NEINDuoMirrorIdentifier stringByAppendingString:@".more"];
        __weak UIBarButtonItem *weakMore = more;
        more.menu = [UIMenu menuWithChildren:@[[UIDeferredMenuElement elementWithUncachedProvider:
            ^(void (^completion)(NSArray<UIMenuElement *> *)) {
                NSMutableArray<UIMenuElement *> *actions = [NSMutableArray new];
                for (UIBarButtonItem *item in objc_getAssociatedObject(weakMore, &NEINDuoMoreSourcesKey)) {
                    NEINDuoMirrorTarget *target = objc_getAssociatedObject(item, &NEINDuoMirrorTargetKey);
                    // LINE draws its icons white for its own bar; the vertical bar tints them as
                    // templates, and the menu needs the same to match.
                    UIImage *image = [item.image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
                    UIAction *action = [UIAction actionWithTitle:item.title ?: @"" image:image identifier:nil
                                                         handler:^(UIAction *sender) {
                        (void)sender;
                        if (target.source) {
                            [target.source sendActionsForControlEvents:UIControlEventTouchUpInside];
                        } else {
                            [target.presented dismissViewControllerAnimated:YES completion:nil];
                        }
                    }];
                    if (!item.enabled) action.attributes = UIMenuElementAttributesDisabled;
                    [actions addObject:action];
                }
                completion(actions);
            }]]];
        objc_setAssociatedObject(page, &NEINDuoMoreItemKey, more, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    objc_setAssociatedObject(more, &NEINDuoMoreSourcesKey, items, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return more;
}

// LINE's buttons on one page, faded or hidden in place once mirrored; a pushed
// page's own back button gives way to the system one.
static NSArray<UIBarButtonItem *> *NEINDuoPageButtons(UIViewController *page, BOOL inDetail) {
    if (!page) return @[];
    NSMutableArray<UIView *> *chatBars = [NSMutableArray new];
    if (page.viewIfLoaded) NEINDuoFindClass(page.view, @"ChatNavigationBar", chatBars);
    NSArray<UIBarButtonItem *> *own = NEINDuoPageOwnButtons(page, chatBars.firstObject);
    UINavigationController *navigation = page.navigationController;
    // A pushed chat room keeps only the system back button, in the vertical bar;
    // the one LINE adds once the split folds would sit in a top bar of its own.
    BOOL pushed = navigation && navigation.viewControllers.firstObject != page;
    UIView *chatBar = pushed || inDetail ? chatBars.firstObject : nil;
    if (chatBar && NEINDuoBarShown && own.count > 1) own = @[NEINDuoChatMoreItem(page, own)];
    NSMutableArray<UIView *> *contents = [NSMutableArray new];
    if (chatBar) NEINDuoFindClass(chatBar, @"ChatNavigationBarContentView", contents);
    UIView *content = NEINDuoBarShown ? contents.firstObject : nil;
    NEINDuoHideChatBack(page, pushed && content ? chatBar : nil, content);
    NEINDuoWidenChatTitle(page, content);
    NEINDuoLiftChatBar(page, pushed && !NEINDuoBarShown ? chatBar : nil, contents.firstObject);
    NEINDuoTightenChat(page, chatBar, contents.firstObject);
    NEINDuoChatRoomPage = pushed && chatBar ? page : nil;
    if (pushed && chatBar) {
        NEINDuoLeadingIcons(page.navigationItem);
        return own;
    }
    // Pages with their own title bar (chat rooms) keep their back button there.
    if (!navigation || navigation.viewControllers.firstObject == page || navigation.navigationBarHidden ||
            NEINDuoShownBar(page)) {
        return own;
    }
    NSArray<UIControl *> *icons = NEINDuoLeadingIcons(page.navigationItem);
    // With LINE's button hidden the system back button returns, and the
    // vertical bar already shows that one.
    if (!page.navigationItem.hidesBackButton) return own;
    NSArray<UIBarButtonItem *> *back = NEINDuoMirrorControls(icons);
    return back.count ? [back arrayByAddingObjectsFromArray:own] : own;
}

// The primary column of the unfolded split keeps its own buttons at its top.
static void NEINDuoRestorePage(UIViewController *page) {
    for (UIBarButtonItem *item in page.navigationItem.rightBarButtonItems) {
        if (item.hidden && [NSStringFromClass(item.customView.class)
                               hasSuffix:@"BarButtonItemGroupContainerView"]) {
            item.hidden = NO;
        }
    }
    for (UIView *view in objc_getAssociatedObject(page, &NEINDuoFadedKey)) view.alpha = 1;
    objc_setAssociatedObject(page, &NEINDuoFadedKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    NEINDuoRestoreLeading(page.navigationItem);
    NEINDuoChatListButtons(page);
    NEINDuoFitUprightPage(page);
}

static UIViewController *NEINDuoTopPage(UIViewController *controller) {
    UINavigationController *navigation = [controller isKindOfClass:UINavigationController.class]
        ? (UINavigationController *)controller : controller.navigationController;
    return navigation.topViewController ?: controller;
}

static BOOL NEINDuoPrimaryOnly;
static UIUserInterfaceSizeClass NEINDuoSavedSizeClass;
static BOOL NEINDuoFitting;

static BOOL NEINDuoIsChatTab(UIViewController *tab) {
    UIViewController *root = [tab isKindOfClass:UINavigationController.class]
        ? ((UINavigationController *)tab).viewControllers.firstObject : tab;
    return [NSStringFromClass(root.class) hasSuffix:@"ChatTabPageViewController"];
}

// Unfolded, only the chat tab keeps a detail column; the others collapse the
// split to one column, as folded, instead of leaving a stale chat or an empty
// page. UIKit caps the primary column's width, so widening it cannot do this.
static void NEINDuoFitPrimaryColumn(UITabBarController *tabs) {
    UISplitViewController *split = tabs.splitViewController;
    if (!split) return;
    // The window's size class is the split's own before the override; the
    // upright unfolded window is regular too, but keeps one column.
    UIWindow *window = split.viewIfLoaded.window;
    BOOL wide = window ? window.traitCollection.horizontalSizeClass != UIUserInterfaceSizeClassCompact &&
                             window.bounds.size.width >= window.bounds.size.height
                       : !split.collapsed;
    BOOL primaryOnly = wide && (NEINDuoPrimaryOnly || !split.collapsed) &&
        tabs.selectedViewController && !NEINDuoIsChatTab(tabs.selectedViewController);
    // LINE overrides the split to regular against a compact push from its root,
    // so its own override is put back rather than removed.
    id<UITraitOverrides> overrides = split.traitOverrides;
    // Unfolding, LINE sets its override again over the compact one.
    if (NEINDuoPrimaryOnly && wide && !NEINDuoFitting &&
            (![overrides containsTrait:UITraitHorizontalSizeClass.class] ||
             overrides.horizontalSizeClass != UIUserInterfaceSizeClassCompact)) {
        NEINDuoPrimaryOnly = NO;
    }
    if (primaryOnly == NEINDuoPrimaryOnly) return;
    NEINDuoPrimaryOnly = primaryOnly;
    if (primaryOnly) {
        NEINDuoSavedSizeClass = [overrides containsTrait:UITraitHorizontalSizeClass.class]
            ? overrides.horizontalSizeClass : UIUserInterfaceSizeClassUnspecified;
        // Collapsing stacks the open chat onto the selected tab; with the chat
        // tab selected meanwhile it lands there, as folding leaves it.
        UIViewController *selected = tabs.selectedViewController;
        UIViewController *chatTab = nil;
        for (UIViewController *tab in tabs.viewControllers) {
            if (NEINDuoIsChatTab(tab)) chatTab = tab;
        }
        NEINDuoFitting = YES;
        if (chatTab) tabs.selectedViewController = chatTab;
        overrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
        [split updateTraitsIfNeeded];
        [split.view layoutIfNeeded];
        tabs.selectedViewController = selected;
        NEINDuoFitting = NO;
    } else if (wide) {
        if (NEINDuoSavedSizeClass != UIUserInterfaceSizeClassUnspecified) {
            overrides.horizontalSizeClass = NEINDuoSavedSizeClass;
        } else {
            [overrides removeTrait:UITraitHorizontalSizeClass.class];
        }
        // Expanded later, the chat room would still top the chat tab's stack
        // here and get its bar shown, which UIKit aborts on once the room
        // moves into the detail column.
        NEINDuoFitting = YES;
        [split updateTraitsIfNeeded];
        [split.view layoutIfNeeded];
        NEINDuoFitting = NO;
    }
    // Folded, LINE sets the override for the new screen itself.
}

// The detail column while the split shows both columns, nil when collapsed.
static UIViewController *NEINDuoDetailColumn(UITabBarController *tabs) {
    UISplitViewController *split = tabs.splitViewController;
    if (!split || split.collapsed || NEINDuoPrimaryOnly) return nil;
    UIViewController *detail = split.style != UISplitViewControllerStyleUnspecified
        ? [split viewControllerForColumn:UISplitViewControllerColumnSecondary] : nil;
    if (!detail && split.viewControllers.count > 1) detail = split.viewControllers.lastObject;
    return detail == tabs ? nil : detail;
}

static void NEINDuoSelectTab(UITabBarController *tabs, NSUInteger index) {
    NSArray<UIViewController *> *controllers = tabs.viewControllers;
    if (index >= controllers.count) return;
    UIViewController *controller = controllers[index];
    id<UITabBarControllerDelegate> delegate = tabs.delegate;
    UIViewController *current = tabs.selectedViewController;
    UINavigationController *navigation = [current isKindOfClass:UINavigationController.class]
        ? (UINavigationController *)current : nil;
    // LINE refuses every tab over a pushed page, where its own bar is never on
    // screen; these buttons are, so they switch anyway.
    if ([delegate respondsToSelector:@selector(tabBarController:shouldSelectViewController:)] &&
            ![delegate tabBarController:tabs shouldSelectViewController:controller] &&
            navigation.viewControllers.count < 2) {
        return;
    }
    tabs.selectedViewController = controller;
    if ([delegate respondsToSelector:@selector(tabBarController:didSelectViewController:)]) {
        [delegate tabBarController:tabs didSelectViewController:controller];
    }
}

// LINE's tabs the proxy shows, as indices into its tab bar. Opening settings
// through LINE's router adds a Settings tab with no artwork, which the
// vertical bar would show as an empty tab.
static NSArray<NSNumber *> *NEINDuoProxyTabIndices(UITabBarController *tabs) {
    NSArray<UITabBarItem *> *items = tabs.tabBar.items;
    NSMutableArray<NSNumber *> *indices = [NSMutableArray new];
    for (NSUInteger i = 0; i < items.count; i++) {
        if (!items[i].image && NEINIsSettingsTabTitle(items[i].title)) continue;
        [indices addObject:@(i)];
    }
    return indices;
}

// LINE's tab bar lives in the primary column, which the vertical bar never
// serves, and bar button stand-ins fold into one overflow button once the
// keyboard shortens the bar. A tab bar controller wrapped around the split
// puts a system tab bar at the bar's bottom instead; its empty tabs only pass
// taps on to LINE's tabs.
@interface NEINDuoTabProxy : NSObject <UITabBarControllerDelegate>
@property(nonatomic, weak) UITabBarController *tabs;
@end

@implementation NEINDuoTabProxy
- (BOOL)tabBarController:(UITabBarController *)proxy shouldSelectTab:(UITab *)tab {
    NSUInteger index = [proxy.tabs indexOfObjectIdenticalTo:tab];
    UITabBarController *tabs = self.tabs;
    NSArray<NSNumber *> *indices = tabs ? NEINDuoProxyTabIndices(tabs) : @[];
    if (index < indices.count) NEINDuoSelectTab(tabs, indices[index].unsignedIntegerValue);
    // The selection follows LINE's, which may refuse the tab.
    return NO;
}
@end

static char NEINDuoTabProxyKey;
static __weak UITabBarController *NEINDuoProxyTabs;
static BOOL NEINDuoProxyTried;

// NO while the split is not on screen yet, to try again on a later pass.
static BOOL NEINDuoInstallTabProxy(UITabBarController *tabs) {
    UISplitViewController *split = tabs.splitViewController;
    UIViewController *root = split.parentViewController;
    UIView *splitView = split.viewIfLoaded;
    UIView *box = splitView.superview;
    if (!split || !root) return YES;
    if (!box || !splitView.window) return NO;
    UITabBarController *proxy = [UITabBarController new];
    NEINDuoTabProxy *forwarder = [NEINDuoTabProxy new];
    forwarder.tabs = tabs;
    objc_setAssociatedObject(proxy, &NEINDuoTabProxyKey, forwarder, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    proxy.delegate = forwarder;
    // Set as view controllers, the tabs would grow one for the split once it is
    // a child; UITab lists only its own.
    NSMutableArray<UITab *> *items = [NSMutableArray new];
    for (NSUInteger i = 0; i < NEINDuoProxyTabIndices(tabs).count; i++) {
        UIViewController *page = [UIViewController new];
        NSString *identifier = [NSString stringWithFormat:@"nein.duo.tab%lu", (unsigned long)i];
        [items addObject:[[UITab alloc] initWithTitle:@"" image:nil identifier:identifier
                                viewControllerProvider:^UIViewController *(__unused UITab *tab) { return page; }]];
    }
    proxy.tabs = items;
    NEINDuoProxyTabs = proxy;
    UIViewAutoresizing fill = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    NSUInteger index = [box.subviews indexOfObjectIdenticalTo:splitView];
    [split willMoveToParentViewController:nil];
    [splitView removeFromSuperview];
    [split removeFromParentViewController];
    [root addChildViewController:proxy];
    proxy.view.frame = splitView.frame;
    proxy.view.autoresizingMask = fill;
    [box insertSubview:proxy.view atIndex:index];
    [proxy didMoveToParentViewController:root];
    [proxy.view layoutIfNeeded];
    [proxy addChildViewController:split];
    splitView.frame = proxy.view.bounds;
    splitView.autoresizingMask = fill;
    // Above the empty tabs' pages, below the tab bar.
    UIView *content = proxy.selectedViewController.viewIfLoaded;
    while (content && content.superview != proxy.view) content = content.superview;
    if (content) [proxy.view insertSubview:splitView aboveSubview:content];
    else [proxy.view insertSubview:splitView atIndex:MIN((NSUInteger)1, proxy.view.subviews.count)];
    [split didMoveToParentViewController:proxy];
    // Moved mid-appearance, the split would never get viewDidAppear, and chat
    // rooms would keep scrolling to the bottom.
    [split beginAppearanceTransition:YES animated:NO];
    [split endAppearanceTransition];
    return YES;
}

static char NEINDuoTabAppearanceKey;

// LINE colors its tab titles alike whether selected or not, leaving the
// selected image to mark the tab; the proxy would tint the selected title
// with the system blue instead. Only the titles carry over, as LINE's
// backgrounds belong to its own bar.
static void NEINDuoSyncTabTitleColors(UITabBar *source, UITabBar *target) {
    UITabBarAppearance *line = source.standardAppearance;
    if (!line || objc_getAssociatedObject(target, &NEINDuoTabAppearanceKey) == line) return;
    UITabBarAppearance *appearance = [target.standardAppearance copy] ?: [UITabBarAppearance new];
    NSArray<UITabBarItemAppearance *> *from = @[line.stackedLayoutAppearance, line.inlineLayoutAppearance,
                                                line.compactInlineLayoutAppearance];
    NSArray<UITabBarItemAppearance *> *to = @[appearance.stackedLayoutAppearance, appearance.inlineLayoutAppearance,
                                              appearance.compactInlineLayoutAppearance];
    for (NSUInteger i = 0; i < from.count; i++) {
        to[i].normal.titleTextAttributes = from[i].normal.titleTextAttributes;
        to[i].selected.titleTextAttributes = from[i].selected.titleTextAttributes;
    }
    target.standardAppearance = appearance;
    target.scrollEdgeAppearance = appearance;
    objc_setAssociatedObject(target, &NEINDuoTabAppearanceKey, line, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static void NEINDuoSyncTabProxy(UITabBarController *tabs, UITabBarController *proxy) {
    NSArray<UITabBarItem *> *all = tabs.tabBar.items;
    NSArray<NSNumber *> *indices = NEINDuoProxyTabIndices(tabs);
    NSArray<UITab *> *items = proxy.tabs;
    // Setting the proxy's tabs would drop the split from its children, so a
    // passing mismatch (mid-fold) waits instead.
    if (items.count != indices.count) return;
    NSUInteger selected = [indices indexOfObject:@(tabs.selectedIndex)];
    for (NSUInteger i = 0; i < indices.count; i++) {
        UITabBarItem *source = all[indices[i].unsignedIntegerValue];
        UITab *item = items[i];
        NSString *title = [source.title stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        if (![item.title isEqualToString:title]) item.title = title;
        // UITab has no selected image of its own.
        UIImage *image = i == selected && source.selectedImage ? source.selectedImage : source.image;
        if (item.image != image) item.image = image;
        NSString *badge = source.badgeValue.length ? source.badgeValue : nil;
        if (badge != item.badgeValue && ![badge isEqualToString:item.badgeValue]) item.badgeValue = badge;
    }
    if (selected < items.count && proxy.selectedTab != items[selected]) proxy.selectedTab = items[selected];
    NEINDuoSyncTabTitleColors(tabs.tabBar, proxy.tabBar);
}

// The bottom tab bar lives in the primary column, which the vertical bar never
// serves; stand-ins for its tabs go to the detail column's toolbar, which the
// vertical bar shows as its own group at the bottom.
static NSArray<UIBarButtonItem *> *NEINDuoTabButtons(UITabBarController *tabs) {
    NSArray<UITabBarItem *> *tabItems = tabs.tabBar.items;
    NSMutableArray<UIBarButtonItem *> *items = objc_getAssociatedObject(tabs, &NEINDuoTabItemsKey);
    if (!items) {
        items = [NSMutableArray new];
        objc_setAssociatedObject(tabs, &NEINDuoTabItemsKey, items, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    __weak UITabBarController *weakTabs = tabs;
    while (items.count < tabItems.count) {
        NSUInteger index = items.count;
        UIBarButtonItem *item = [[UIBarButtonItem alloc] initWithPrimaryAction:
            [UIAction actionWithTitle:@"" image:nil identifier:nil handler:^(UIAction *action) {
                (void)action;
                UITabBarController *target = weakTabs;
                if (target) NEINDuoSelectTab(target, index);
            }]];
        item.accessibilityIdentifier = NEINDuoTabIdentifier;
        [items addObject:item];
    }
    if (items.count > tabItems.count) {
        [items removeObjectsInRange:NSMakeRange(tabItems.count, items.count - tabItems.count)];
    }
    for (NSUInteger i = 0; i < tabItems.count; i++) {
        UITabBarItem *tab = tabItems[i];
        UIBarButtonItem *item = items[i];
        // A selected bar button item takes the system's tinted fill, which LINE's
        // tab bar never shows; its selected image marks the tab instead.
        BOOL selected = i == tabs.selectedIndex;
        UIImage *image = selected && tab.selectedImage ? tab.selectedImage : tab.image;
        if (item.image != image) item.image = image;
        NSString *title = [tab.title stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        if (![item.title isEqualToString:title]) {
            item.title = title;
            item.accessibilityLabel = title;
        }
        NSString *badge = tab.badgeValue.length ? tab.badgeValue : nil;
        NSString *shown = item.badge.stringValue;
        if (badge != shown && ![badge isEqualToString:shown]) {
            item.badge = badge ? [UIBarButtonItemBadge badgeWithString:badge] : nil;
        }
    }
    return items;
}

// A page under a pushed one, which keeps what NEIN gave it until it is back on top.
static BOOL NEINDuoIsCovered(UIViewController *page) {
    UINavigationController *navigation = page.navigationController;
    return navigation && navigation.topViewController != page &&
        [navigation.viewControllers containsObject:page];
}

static void NEINDuoShowTabToolbar(UIViewController *page, UITabBarController *tabs) {
    UIViewController *previous = NEINDuoToolbarPage;
    if (previous && previous != page && NEINDuoIsCovered(previous)) {
        NEINDuoToolbarPage = nil;
    } else if (previous && previous != page) {
        if ([previous.toolbarItems.firstObject.accessibilityIdentifier isEqualToString:NEINDuoTabIdentifier]) {
            previous.toolbarItems = nil;
        }
        // A popped page has left its navigation controller already.
        UINavigationController *navigation = previous.navigationController ?: NEINDuoToolbarNavigation;
        if (navigation && navigation != page.navigationController) navigation.toolbarHidden = YES;
        NEINDuoToolbarPage = nil;
    }
    if (!page) return;
    NSArray<UIBarButtonItem *> *items = [NEINDuoTabButtons(tabs) copy];
    if (![page.toolbarItems isEqualToArray:items]) page.toolbarItems = items;
    if (page.navigationController.toolbarHidden) page.navigationController.toolbarHidden = NO;
    NEINDuoToolbarPage = page;
    NEINDuoToolbarNavigation = page.navigationController;
}

static BOOL NEINDuoOwnBarChange;
static void (*NEINDuoOriginalSetBarHidden)(id, SEL, BOOL, BOOL);
static void (*NEINDuoOriginalSetBarHiddenPlain)(id, SEL, BOOL);

// LINE hides the bar again whenever a page that hides it reappears; while the
// page hosts the vertical bar that would empty the bar for a moment.
static BOOL NEINDuoKeepsBar(UINavigationController *navigation, BOOL hidden) {
    return hidden && !NEINDuoOwnBarChange &&
        NEINDuoShownBar(navigation.topViewController) == navigation;
}

static void NEINDuoSetNavigationBarHidden(UINavigationController *self, SEL selector, BOOL hidden,
                                          BOOL animated) {
    if (!NEINDuoKeepsBar(self, hidden)) NEINDuoOriginalSetBarHidden(self, selector, hidden, animated);
}

static void NEINDuoSetNavigationBarHiddenPlain(UINavigationController *self, SEL selector, BOOL hidden) {
    if (!NEINDuoKeepsBar(self, hidden)) NEINDuoOriginalSetBarHiddenPlain(self, selector, hidden);
}

// NEIN's own bar changes, which NEINDuoKeepsBar lets through.
static void NEINDuoForceBarHidden(UINavigationController *navigation, BOOL hidden) {
    NEINDuoOwnBarChange = YES;
    navigation.navigationBarHidden = hidden;
    NEINDuoOwnBarChange = NO;
}

// The vertical bar only shows a navigation bar's items, so pages that hide
// theirs get it back while they host the group; it stays without a title. A
// page covered by a pushed one keeps that until it is back on top.
static void NEINDuoHideShownBar(UIViewController *page) {
    UINavigationController *navigation = NEINDuoShownBar(page);
    if (navigation.topViewController == page) NEINDuoForceBarHidden(navigation, YES);
    for (UIBarButtonItem *item in objc_getAssociatedObject(page, &NEINDuoHiddenLeftItemsKey)) {
        item.hidden = NO;
    }
    NEINDuoSetShownBar(page, nil);
    objc_setAssociatedObject(page, &NEINDuoHiddenLeftItemsKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static void NEINDuoShowNavigationBar(UIViewController *page) {
    UIViewController *previous = NEINDuoShownBarPage;
    if (previous && previous != page) {
        if (!NEINDuoIsCovered(previous)) NEINDuoHideShownBar(previous);
        NEINDuoShownBarPage = nil;
    }
    UINavigationController *navigation = page.navigationController;
    UINavigationController *shown = page ? NEINDuoShownBar(page) : nil;
    if (shown && shown != navigation) {
        // Unfolding moves a page pushed on the folded stack into the detail
        // column. Left visible, the old stack's bar would still lay out the
        // page's item, which now belongs to the new bar, and UIKit aborts.
        if (navigation && ![shown.viewControllers containsObject:page] && !shown.navigationBarHidden &&
                !NEINDuoShownBar(shown.topViewController)) {
            NEINDuoForceBarHidden(shown, YES);
        }
        NEINDuoSetShownBar(page, navigation);
    }
    if (page && NEINDuoShownBar(page)) {
        NEINDuoShownBarPage = page;
        if (navigation.navigationBarHidden) navigation.navigationBarHidden = NO;
        return;
    }
    if (!page || !navigation.navigationBarHidden) return;
    NEINDuoSetShownBar(page, navigation);
    // Custom views cannot move into the vertical bar and would pull a top bar back in.
    NSMutableArray<UIBarButtonItem *> *hidden = [NSMutableArray new];
    for (UIBarButtonItem *item in page.navigationItem.leftBarButtonItems) {
        if (item.customView && !item.hidden) {
            item.hidden = YES;
            [hidden addObject:item];
        }
    }
    objc_setAssociatedObject(page, &NEINDuoHiddenLeftItemsKey, hidden, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    NEINDuoShownBarPage = page;
    navigation.navigationBarHidden = NO;
}

// The tab bar wrapped around the split, installed as soon as it can be and kept
// in step with LINE's tabs; nil until then.
static UITabBarController *NEINDuoUpdateTabProxy(UITabBarController *tabs, BOOL possible,
                                                 BOOL expanded) {
    if (!NEINDuoProxyTried && possible) {
        NEINDuoProxyTried = YES;
        // Moving the split mid-layout would upset the pass in progress.
        __weak UITabBarController *weakTabs = tabs;
        dispatch_async(dispatch_get_main_queue(), ^{
            UITabBarController *target = weakTabs;
            if (target && !NEINDuoInstallTabProxy(target)) NEINDuoProxyTried = NO;
        });
    }
    UITabBarController *proxy = NEINDuoProxyTabs;
    if (!proxy) return nil;
    NEINDuoSyncTabProxy(tabs, proxy);
    // The split expanding again, after one column or a narrow window, drops
    // the proxy's tabs from the vertical bar until the tab bar is shown anew.
    if (expanded) {
        __weak UITabBarController *weakProxy = proxy;
        dispatch_async(dispatch_get_main_queue(), ^{
            UITabBarController *target = weakProxy;
            if (!target || target.tabBarHidden) return;
            target.tabBarHidden = YES;
            [target.view layoutIfNeeded];
            target.tabBarHidden = NO;
        });
    }
    return proxy;
}

static BOOL NEINDuoHadDetail;
static BOOL NEINDuoHidProxyTabBar;

// The page at the window's trailing edge hosts the mirrored buttons: the
// selected tab's top page, or the detail column's while both columns show.
static __weak UIView *NEINDuoLiftedTabs;
static __weak UIView *NEINDuoWidenedInput;
static char NEINDuoInputEdgeKey;

// With the tab group lifted or the keyboard up, the strip the input bar's safe
// area keeps for the vertical bar is empty, so its buttons run to the edge on
// the bar's side, the side with the larger inset; nil restores LINE's.
static void NEINDuoWidenInputBar(UIView *input) {
    NSLayoutAttribute side = NSLayoutAttributeNotAnAttribute;
    if (input) {
        UIEdgeInsets safe = input.safeAreaInsets;
        BOOL rtl = input.effectiveUserInterfaceLayoutDirection == UIUserInterfaceLayoutDirectionRightToLeft;
        CGFloat leading = rtl ? safe.right : safe.left;
        CGFloat trailing = rtl ? safe.left : safe.right;
        if (leading != trailing) side = leading > trailing ? NSLayoutAttributeLeading : NSLayoutAttributeTrailing;
    }
    UIView *previous = NEINDuoWidenedInput;
    NSArray<NSLayoutConstraint *> *saved = objc_getAssociatedObject(previous, &NEINDuoInputEdgeKey);
    if (previous && (previous != input || saved[0].firstAttribute != side)) {
        saved[1].active = NO;
        saved[0].active = YES;
        objc_setAssociatedObject(previous, &NEINDuoInputEdgeKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    NEINDuoWidenedInput = nil;
    if (!input || side == NSLayoutAttributeNotAnAttribute) return;
    saved = objc_getAssociatedObject(input, &NEINDuoInputEdgeKey);
    if (!saved) {
        NSLayoutConstraint *line = nil;
        for (NSLayoutConstraint *constraint in input.constraints) {
            if (constraint.active && [constraint.firstItem isKindOfClass:UIStackView.class] &&
                    constraint.secondItem == input.safeAreaLayoutGuide && constraint.firstAttribute == side) {
                line = constraint;
            }
        }
        if (!line) return;
        UIView *stack = line.firstItem;
        NSLayoutConstraint *ours = side == NSLayoutAttributeLeading
            ? [stack.leadingAnchor constraintEqualToAnchor:input.leadingAnchor constant:line.constant]
            : [stack.trailingAnchor constraintEqualToAnchor:input.trailingAnchor constant:line.constant];
        ours.priority = line.priority;
        saved = @[line, ours];
        objc_setAssociatedObject(input, &NEINDuoInputEdgeKey, saved, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    if (saved[0].active) saved[0].active = NO;
    if (!saved[1].active) saved[1].active = YES;
    NEINDuoWidenedInput = input;
}

// The vertical bar pins its tab group to the bottom, over a chat room's input
// bar, and UIKit offers no placement for it, so its glass is shifted above it.
// shortcut: matches UIKit's private vertical bar class names; recheck after iOS updates.
static void NEINDuoLiftTabGroup(UIViewController *page) {
    UIView *glass = nil;
    UIView *widened = nil;
    CGFloat lift = 0;
    UIWindow *window = page.viewIfLoaded.window;
    if (window && page == NEINDuoChatRoomPage && NEINDuoBarShown) {
        NSMutableArray<UIView *> *found = [NSMutableArray new];
        NEINDuoFindClass(window, @"ToolbarCustomViewRepresentable__", found);
        glass = found.firstObject.superview;
        while (glass && ![NSStringFromClass(glass.class) hasSuffix:@"GlassInteractionView"]) glass = glass.superview;
        NSMutableArray<UIView *> *inputs = [NSMutableArray new];
        NEINDuoFindClass(page.view, @"ChatInputBar", inputs);
        for (UIView *input in inputs) {
            CGRect frame = [input convertRect:input.bounds toView:window];
            if (CGRectGetMinX(frame) < 0) continue;
            // Raised by the keyboard, the input bar is clear of the shortened bar;
            // at the bottom, only once the tab group is lifted off it.
            if (CGRectGetMaxY(frame) < CGRectGetMaxY(window.bounds) - 1) {
                widened = input;
            } else if (glass) {
                CGFloat bottom = CGRectGetMaxY([glass convertRect:glass.bounds toView:window]) - glass.transform.ty;
                lift = MAX(0, bottom - (CGRectGetMinY(frame) - 8));
                widened = input;
            }
            break;
        }
    }
    NEINDuoWidenInputBar(widened);
    UIView *previous = NEINDuoLiftedTabs;
    if (previous && previous != glass) previous.transform = CGAffineTransformIdentity;
    NEINDuoLiftedTabs = lift > 0 ? glass : nil;
    if (glass && glass.transform.ty != -lift) glass.transform = CGAffineTransformMakeTranslation(0, -lift);
}

static void NEINDuoUpdateVerticalBar(UITabBarController *tabs, UIViewController *homeTab,
                                     UINavigationItem *home, BOOL gear) {
    // The tab switch while the split collapses lays the tab bar out again.
    if (NEINDuoFitting) return;
    BOOL wasPrimaryOnly = NEINDuoPrimaryOnly;
    NEINDuoFitPrimaryColumn(tabs);
    // Only the phone idiom trades the vertical bar for top and bottom bars. The
    // primary column, away from the bar's edge, reports no bar at all.
    UITraitCollection *traits = (tabs.splitViewController ?: tabs).traitCollection;
    NEINDuoBarShown = traits.userInterfaceIdiom != UIUserInterfaceIdiomPhone ||
        traits.verticalBarEdge != UIVerticalBarEdgeUnspecified;
    UIViewController *detail = NEINDuoDetailColumn(tabs);
    UIViewController *selected = NEINDuoTopPage(tabs.selectedViewController);
    UIViewController *page = detail ? NEINDuoTopPage(detail) : selected;
    if (detail) {
        NEINDuoRestorePage(selected);
        // Unfolding a chat room pushed over the chat list leaves the list the
        // bar it hosted the vertical bar's group in.
        if (selected != page && NEINDuoShownBar(selected)) NEINDuoHideShownBar(selected);
        if (home) NEINDuoRestorePage(NEINDuoTopPage(homeTab));
        // Folding stacks the detail column's pages onto the selected tab, whose
        // bar LINE hid for the chat list; the page on top keeps the bar it had.
        NEINDuoDetailTopPage = page;
        NEINDuoDetailBarHidden = page.navigationController.navigationBarHidden;
    } else if (NEINDuoDetailTopPage && selected == NEINDuoDetailTopPage) {
        NEINDuoDetailTopPage = nil;
        UINavigationController *navigation = selected.navigationController;
        if (navigation.navigationBarHidden != NEINDuoDetailBarHidden) {
            NEINDuoForceBarHidden(navigation, NEINDuoDetailBarHidden);
        }
    }
    NSArray<UIBarButtonItem *> *mirrored = NEINDuoPageButtons(page, detail != nil);
    if (page != NEINDuoChatRoomPage) NEINDuoFitUprightPage(page);
    // Without a parent to take the proxy's place in, the stand-ins stay.
    BOOL proxyPossible = tabs.splitViewController.parentViewController != nil;
    BOOL expanded = (wasPrimaryOnly && !NEINDuoPrimaryOnly) || (detail && !NEINDuoHadDetail);
    NEINDuoHadDetail = detail != nil;
    UITabBarController *proxy = NEINDuoUpdateTabProxy(tabs, proxyPossible, expanded);
    // The vertical bar shows only the outermost tab bar, the proxy's once it
    // wraps the split; LINE's stays hidden then.
    BOOL hideTabs = detail || proxy;
    if (hideTabs && !NEINDuoHidTabBar && !tabs.tabBarHidden) {
        tabs.tabBarHidden = YES;
        NEINDuoHidTabBar = YES;
    } else if (!hideTabs && NEINDuoHidTabBar) {
        tabs.tabBarHidden = NO;
        NEINDuoHidTabBar = NO;
    }
    // A bottom tab bar would cover a chat room's input bar, as LINE's own
    // never does.
    BOOL hideProxyTabs = !NEINDuoBarShown && page && page == NEINDuoChatRoomPage;
    if (proxy && hideProxyTabs != NEINDuoHidProxyTabBar) {
        proxy.tabBarHidden = hideProxyTabs;
        NEINDuoHidProxyTabBar = hideProxyTabs;
    }
    // Unfolded, the gear stays on Home in the primary column like its buttons.
    UINavigationItem *gearHost = detail && gear ? home : nil;
    BOOL pageGear = !detail && gear && tabs.selectedViewController == homeTab;
    UINavigationItem *host = page.navigationItem;
    UINavigationItem *previous = NEINDuoHomeHost;
    if (previous && previous != host && !NEINDuoIsCovered(NEINDuoHomeHostPage)) {
        NEINDuoRemoveHomeButtons(previous);
        NEINDuoRestoreLeading(previous);
    }
    NEINDuoHomeHostPage = page;
    NEINDuoHomeHost = host;
    UINavigationItem *previousGear = NEINDuoGearHost;
    if (previousGear && previousGear != gearHost && previousGear != host) {
        NEINDuoRemoveHomeButtons(previousGear);
    }
    NEINDuoGearHost = gearHost;
    if (gearHost) NEINDuoInstallHomeButtons(gearHost, tabs, @[], YES);
    if (host) NEINDuoInstallHomeButtons(host, tabs, mirrored, pageGear);
    // A room's long title would fold its buttons into the gear in a top bar.
    if (!NEINDuoBarShown && page && page == NEINDuoChatRoomPage &&
            host.pinnedTrailingGroup.representativeItem) {
        host.pinnedTrailingGroup.representativeItem = nil;
    }
    // A pushed chat room covers LINE's tab bar, so it gets the stand-ins as well.
    // Stand-ins shown before the proxy arrives stay in the vertical bar beneath
    // its tabs, so they wait for it.
    UIViewController *toolbarPage = !proxyPossible && (detail || (page && page == NEINDuoChatRoomPage))
        ? page : nil;
    NEINDuoShowTabToolbar(toolbarPage, tabs);
    NEINDuoLiftTabGroup(page);
    NEINDuoShowNavigationBar(mirrored.count || pageGear || detail ? page : nil);
}

static char NEINDuoModalCloseKey;
static char NEINDuoCarriedCloseKey;
static char NEINDuoTakenCloseKey;

// Mid-push the bar applies hidden only once the transition ends, drawing the
// button and its glass backing until then.
static void NEINDuoVanish(NSArray<UIBarButtonItem *> *items, NSArray<UIControl *> *controls) {
    for (UIBarButtonItem *item in items) {
        if (![controls containsObject:(UIControl *)item.customView]) continue;
        item.customView.alpha = 0;
        if (!item.hidesSharedBackground) item.hidesSharedBackground = YES;
    }
}

// A presented page's close button sits at its leading edge, away from the
// vertical bar, or at its trailing edge as a custom view the bar leaves out;
// it moves into the page's pinned group unless LINE uses that.
static void NEINDuoMoveModalClose(UINavigationController *navigation) {
    UIViewController *page = navigation.topViewController;
    if (!page || !navigation.presentingViewController || navigation.navigationBarHidden) return;
    UINavigationItem *item = page.navigationItem;
    UIBarButtonItemGroup *pinned = item.pinnedTrailingGroup;
    BOOL ours = pinned && objc_getAssociatedObject(pinned, &NEINDuoModalCloseKey);
    if (pinned && !ours) return;
    // A pushed page's leading button is its back button, which the vertical
    // bar shows already.
    NSArray<UIControl *> *controls = navigation.viewControllers.firstObject == page ? NEINDuoLeadingIcons(item) : @[];
    if (!controls.count) {
        controls = NEINDuoIconControls(item.rightBarButtonItems);
        NEINDuoVanish(item.rightBarButtonItems, controls);
        if (!controls.count) controls = objc_getAssociatedObject(item, &NEINDuoTakenCloseKey) ?: @[];
    }
    if (!controls.count) return;
    // The stand-ins carried over before the push become the mirrors in place.
    NSArray<UIBarButtonItem *> *carried = objc_getAssociatedObject(item, &NEINDuoCarriedCloseKey);
    if (carried) {
        objc_setAssociatedObject(item, &NEINDuoCarriedCloseKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        for (NSUInteger i = 0; carried.count == controls.count && i < controls.count; i++) {
            if (objc_getAssociatedObject(controls[i], &NEINDuoMirrorItemKey)) continue;
            ((NEINDuoMirrorTarget *)objc_getAssociatedObject(carried[i], &NEINDuoMirrorTargetKey)).source = controls[i];
            objc_setAssociatedObject(controls[i], &NEINDuoMirrorItemKey, carried[i],
                                     OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }
    }
    NSArray<UIBarButtonItem *> *mirrored = NEINDuoMirrorControls(controls);
    if (ours && [pinned.barButtonItems isEqualToArray:mirrored]) return;
    UIBarButtonItemGroup *group = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:mirrored
                                                                     representativeItem:nil];
    objc_setAssociatedObject(group, &NEINDuoModalCloseKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    item.pinnedTrailingGroup = group;
}

// LINE gives a pushed presented page its close button only after the push,
// and a group set mid-push waits in the page's own bar until it ends; stand-ins
// for the previous page's buttons hold the place from the start instead.
static void NEINDuoCarryModalClose(UINavigationController *navigation, UIViewController *page) {
    UINavigationItem *item = page.navigationItem;
    if (!navigation.presentingViewController || navigation.navigationBarHidden || item.pinnedTrailingGroup) {
        return;
    }
    UIBarButtonItemGroup *previous = navigation.topViewController.navigationItem.pinnedTrailingGroup;
    if (!previous.barButtonItems.count || !objc_getAssociatedObject(previous, &NEINDuoModalCloseKey)) return;
    NSMutableArray<UIBarButtonItem *> *carried = [NSMutableArray new];
    for (UIBarButtonItem *old in previous.barButtonItems) {
        UIBarButtonItem *stand = NEINDuoNewMirrorItem(nil, navigation);
        stand.image = old.image;
        stand.title = old.title;
        stand.accessibilityLabel = old.accessibilityLabel;
        [carried addObject:stand];
    }
    UIBarButtonItemGroup *group = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:carried
                                                                     representativeItem:nil];
    objc_setAssociatedObject(group, &NEINDuoModalCloseKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(item, &NEINDuoCarriedCloseKey, carried, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    item.pinnedTrailingGroup = group;
}

static void (*NEINDuoOriginalPush)(id, SEL, UIViewController *, BOOL);

static void (*NEINDuoOriginalSetRightItem)(id, SEL, UIBarButtonItem *, BOOL);
static void (*NEINDuoOriginalSetRightItems)(id, SEL, NSArray<UIBarButtonItem *> *, BOOL);

// Even hidden, the bar draws a button's glass backing mid-push, so the bar
// never gets the buttons the vertical bar shows.
static NSArray<UIBarButtonItem *> *NEINDuoTakeRightIcons(UINavigationItem *item, NSArray<UIBarButtonItem *> *items) {
    NSArray<UIControl *> *taken = NEINDuoIconControls(items);
    objc_setAssociatedObject(item, &NEINDuoTakenCloseKey, taken.count ? taken : nil,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    if (!taken.count) return items;
    NSMutableArray<UIBarButtonItem *> *kept = [NSMutableArray new];
    for (UIBarButtonItem *right in items) {
        if (![taken containsObject:(UIControl *)right.customView]) [kept addObject:right];
    }
    return kept;
}

// Changing a pushed page's bar items once it hosts makes the vertical bar
// redraw after the push animation; doing it first lets the push animate to them.
static void NEINDuoPushViewController(UINavigationController *self, SEL selector,
                                      UIViewController *page, BOOL animated) {
    if (NEINDuoHomeTabs && page && self.viewControllers.count) {
        [page loadViewIfNeeded];
        if (!page.navigationItem.hidesBackButton) NEINDuoLeadingIcons(page.navigationItem);
        NEINDuoCarryModalClose(self, page);
        // LINE can give the page its close button before the push, outside
        // any presented page's top item.
        UINavigationItem *item = page.navigationItem;
        if (NEINDuoOriginalSetRightItems && self.presentingViewController && !self.navigationBarHidden &&
                item.rightBarButtonItems.count) {
            NSArray<UIBarButtonItem *> *kept = NEINDuoTakeRightIcons(item, item.rightBarButtonItems);
            if (kept.count != item.rightBarButtonItems.count) {
                NEINDuoOriginalSetRightItems(item, @selector(setRightBarButtonItems:animated:), kept, NO);
            }
        }
        // The tab stand-ins likewise go along before the push, not after it.
        UIViewController *toolbarPage = NEINDuoToolbarPage;
        if (toolbarPage && toolbarPage == self.topViewController && !page.toolbarItems.count) {
            page.toolbarItems = toolbarPage.toolbarItems;
        }
    }
    NEINDuoOriginalPush(self, selector, page, animated);
}

// LINE sets a presented page's close button after the push, and again from a
// timer with a new button each time; the bar draws a button's backing as soon
// as it gets one, so it is hidden before LINE hands it over.
static UINavigationController *NEINDuoPresentedNavigation(UINavigationItem *item) {
    if (!NEINDuoHomeTabs) return nil;
    // The Home tabs can be out of the window after a fold, so every window is
    // searched.
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) continue;
        for (UIWindow *window in ((UIWindowScene *)scene).windows) {
            for (UIViewController *presented = window.rootViewController.presentedViewController;
                 presented; presented = presented.presentedViewController) {
                if (![presented isKindOfClass:UINavigationController.class]) continue;
                UINavigationController *navigation = (UINavigationController *)presented;
                if (navigation.topViewController.navigationItem == item) return navigation;
            }
        }
    }
    return nil;
}

static void NEINDuoSetRightItems(UINavigationItem *self, SEL selector, NSArray<UIBarButtonItem *> *items,
                                 BOOL animated) {
    UINavigationController *navigation = items.count ? NEINDuoPresentedNavigation(self) : nil;
    if (navigation && !navigation.navigationBarHidden) {
        items = NEINDuoTakeRightIcons(self, items);
    } else if (navigation) {
        objc_setAssociatedObject(self, &NEINDuoTakenCloseKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    NEINDuoOriginalSetRightItems(self, selector, items, animated);
    if (navigation) NEINDuoMoveModalClose(navigation);
}

static void NEINDuoSetRightItem(UINavigationItem *self, SEL selector, UIBarButtonItem *item, BOOL animated) {
    if (item) {
        NEINDuoSetRightItems(self, @selector(setRightBarButtonItems:animated:), @[item], animated);
    } else {
        NEINDuoOriginalSetRightItem(self, selector, item, animated);
    }
}

// LINE sets them through the plain setters too, which skip the animated ones.
static void NEINDuoSetRightItemsPlain(UINavigationItem *self, SEL __unused selector,
                                      NSArray<UIBarButtonItem *> *items) {
    NEINDuoSetRightItems(self, @selector(setRightBarButtonItems:animated:), items, NO);
}

static void NEINDuoSetRightItemPlain(UINavigationItem *self, SEL __unused selector, UIBarButtonItem *item) {
    NEINDuoSetRightItem(self, @selector(setRightBarButtonItem:animated:), item, NO);
}

// Opening a chat in the detail column changes the host without a tab layout.
static void NEINDuoNavigationDidLayoutSubviews(UINavigationController *self, SEL selector) {
    NEINDuoOriginalNavigationLayout(self, selector);
    UITabBarController *tabs = NEINDuoHomeTabs;
    NEINDuoMoveModalClose(self);
    if (tabs) NEINDuoUpdateHomeButtons(tabs);
}

static NEINDuoTabBarLayoutIMP NEINDuoOriginalControlMoveToWindow;
static BOOL NEINDuoRefreshQueued;

// LINE replaces the chat list's header buttons after launch without any layout
// pass above, leaving mirrors of buttons no longer on screen; a control moving
// windows refreshes once per main queue turn.
static void NEINDuoQueueRefresh(void) {
    if (NEINDuoRefreshQueued || !NEINDuoHomeTabs) return;
    NEINDuoRefreshQueued = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        NEINDuoRefreshQueued = NO;
        UITabBarController *tabs = NEINDuoHomeTabs;
        if (!tabs) return;
        NEINDuoUpdateHomeButtons(tabs);
        // Presented pages swap in new close buttons the same way.
        for (UIViewController *presented = tabs.view.window.rootViewController.presentedViewController;
             presented; presented = presented.presentedViewController) {
            if ([presented isKindOfClass:UINavigationController.class]) {
                NEINDuoMoveModalClose((UINavigationController *)presented);
            }
        }
    });
}

static void NEINDuoControlDidMoveToWindow(UIControl *self, SEL selector) {
    NEINDuoOriginalControlMoveToWindow(self, selector);
    NEINDuoQueueRefresh();
}

static NEINDuoTabBarLayoutIMP NEINDuoOriginalSafeAreaChange;

typedef CGFloat (*NEINDuoRowHeightIMP)(id, SEL, UITableView *, NSIndexPath *);
static NEINDuoRowHeightIMP NEINDuoOriginalMenuRowHeight;

// Folding with a chat menu open moves it out of the window for a moment, while
// it still carries the inner screen's column insets; its media row then sizes
// from a negative width and UITableView asserts. It is laid out again once it
// lands, so an empty row in between is never seen.
static CGFloat NEINDuoMenuRowHeight(id self, SEL selector, UITableView *table, NSIndexPath *path) {
    CGFloat height = NEINDuoOriginalMenuRowHeight(self, selector, table, path);
    return height < 0 && height != UITableViewAutomaticDimension ? 0 : height;
}

// A chat room's insets are measured from what its parents give, which can
// change mid-fold with no layout pass above; it is measured again then.
static void NEINDuoSafeAreaDidChange(UIViewController *self, SEL selector) {
    NEINDuoOriginalSafeAreaChange(self, selector);
    if (objc_getAssociatedObject(self, &NEINDuoChatInsetsKey)) NEINDuoQueueRefresh();
}

static NSInteger (*NEINDuoOriginalVerticalBarBehavior)(id, SEL);

// LINE's login flow shows no bar items, so the vertical bar there would only
// reserve an empty strip at the trailing edge.
static UIVerticalBarBehavior NEINDuoVerticalBarBehavior(UIViewController *self, SEL selector) {
    NSString *name = NSStringFromClass(self.class);
    for (NSString *module in @[@"LineSecondaryLoginUI.", @"LineAccountAuthenticationUICore.",
                               @"LineRegistrationUI."]) {
        if ([name hasPrefix:module]) return UIVerticalBarBehaviorDisabled;
    }
    return NEINDuoOriginalVerticalBarBehavior(self, selector);
}

// The hooks only the system vertical bar needs; each one degrades on its own.
static void NEINDuoInstallVerticalBarHooks(void) {
    if (!NEINHookOwnMethod(UIViewController.class, @selector(preferredVerticalBarBehavior),
                           @encode(NSInteger), 2, NULL, NULL, (IMP)NEINDuoVerticalBarBehavior,
                           (IMP *)&NEINDuoOriginalVerticalBarBehavior)) {
        NSLog(@"[NEINIPhoneDuo] Vertical bar behavior unavailable; login keeps the bar's strip.");
    }
    if (!NEINHookOwnMethod(UINavigationController.class, @selector(viewDidLayoutSubviews),
                           "v", 2, NULL, NULL, (IMP)NEINDuoNavigationDidLayoutSubviews,
                           (IMP *)&NEINDuoOriginalNavigationLayout)) {
        NSLog(@"[NEINIPhoneDuo] Navigation layout unavailable; Home buttons stay on Home.");
    }
    if (!NEINHookOwnMethod(UINavigationController.class, @selector(setNavigationBarHidden:animated:),
                           "v", 4, @encode(BOOL), @encode(BOOL),
                           (IMP)NEINDuoSetNavigationBarHidden,
                           (IMP *)&NEINDuoOriginalSetBarHidden) ||
            !NEINHookOwnMethod(UINavigationController.class, @selector(setNavigationBarHidden:),
                               "v", 3, @encode(BOOL), NULL,
                               (IMP)NEINDuoSetNavigationBarHiddenPlain,
                               (IMP *)&NEINDuoOriginalSetBarHiddenPlain)) {
        NSLog(@"[NEINIPhoneDuo] Navigation bar hiding unavailable; bars blink on return.");
    }
    if (!NEINHookOwnMethod(UINavigationController.class, @selector(pushViewController:animated:),
                           "v", 4, "@", @encode(BOOL), (IMP)NEINDuoPushViewController,
                           (IMP *)&NEINDuoOriginalPush)) {
        NSLog(@"[NEINIPhoneDuo] Navigation push unavailable; back buttons swap after pushes.");
    }
    if (!NEINHookOwnMethod(UINavigationItem.class, @selector(setRightBarButtonItem:animated:),
                           "v", 4, "@", @encode(BOOL), (IMP)NEINDuoSetRightItem,
                           (IMP *)&NEINDuoOriginalSetRightItem) ||
            !NEINHookOwnMethod(UINavigationItem.class, @selector(setRightBarButtonItems:animated:),
                               "v", 4, "@", @encode(BOOL), (IMP)NEINDuoSetRightItems,
                               (IMP *)&NEINDuoOriginalSetRightItems) ||
            !NEINHookOwnMethod(UINavigationItem.class, @selector(setRightBarButtonItem:),
                               "v", 3, "@", NULL, (IMP)NEINDuoSetRightItemPlain, NULL) ||
            !NEINHookOwnMethod(UINavigationItem.class, @selector(setRightBarButtonItems:),
                               "v", 3, "@", NULL, (IMP)NEINDuoSetRightItemsPlain, NULL)) {
        NSLog(@"[NEINIPhoneDuo] Bar item changes unavailable; close buttons move on the next layout.");
    }
    if (!NEINHookOwnMethod(UIControl.class, @selector(didMoveToWindow), "v", 2, NULL, NULL,
                           (IMP)NEINDuoControlDidMoveToWindow,
                           (IMP *)&NEINDuoOriginalControlMoveToWindow)) {
        NSLog(@"[NEINIPhoneDuo] Control window moves unavailable; swapped buttons wait for a layout.");
    }
    if (!NEINHookOwnMethod(UIViewController.class, @selector(viewSafeAreaInsetsDidChange), "v", 2,
                           NULL, NULL, (IMP)NEINDuoSafeAreaDidChange,
                           (IMP *)&NEINDuoOriginalSafeAreaChange)) {
        NSLog(@"[NEINIPhoneDuo] Safe area changes unavailable; chat rooms wait for a layout.");
    }
    Class list = NSClassFromString(@"LineChatTabUI.ChatTabPageViewController");
    if (!list || !NEINHookOwnMethod(list, @selector(setAdditionalSafeAreaInsets:), "v", 3,
                                    @encode(UIEdgeInsets), NULL, (IMP)NEINDuoSetListInsets,
                                    (IMP *)&NEINDuoOriginalSetListInsets)) {
        NSLog(@"[NEINIPhoneDuo] Chat list insets unavailable; its title stays in its header.");
    }
    Class menu = NSClassFromString(@"LineMessagingUI.ChatMenuViewController");
    if (!menu || !NEINHookOwnMethod(menu, @selector(tableView:heightForRowAtIndexPath:),
                                    @encode(CGFloat), 4, "@", "@", (IMP)NEINDuoMenuRowHeight,
                                    (IMP *)&NEINDuoOriginalMenuRowHeight)) {
        NSLog(@"[NEINIPhoneDuo] Chat menu rows unavailable; folding with it open may crash.");
    }
}

#pragma clang attribute pop
