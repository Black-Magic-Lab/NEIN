#import <UIKit/UIKit.h>
#import <objc/message.h>
#include <assert.h>

// Real UIKit navigation items (Mac Catalyst) driven by direct layout calls;
// the controllers have no window, so the gear's action opens nothing.
#include "../hooks/NEINDuoHomeButtons.h"

@interface NEINTestBarButtonItemGroupContainerView : UIView
@end
@implementation NEINTestBarButtonItemGroupContainerView
@end

// No UIApplication runs here, so record the forwarded event directly.
@interface NEINTestButton : UIButton
@property(nonatomic) NSUInteger sent;
@end
@implementation NEINTestButton
- (void)sendActionsForControlEvents:(UIControlEvents)events {
    if (events == UIControlEventTouchUpInside) self.sent++;
}
@end

// Without a window the split never adopts its column controllers as children.
@interface NEINTestChatNavigationBar : UIView
@end
@implementation NEINTestChatNavigationBar
@end

@interface NEINTestChatBadgedButton : NEINTestButton
@end
@implementation NEINTestChatBadgedButton
@end

@interface NEINTestPresentedNavigation : UINavigationController
@end
@implementation NEINTestPresentedNavigation
- (UIViewController *)presentingViewController {
    return self.viewControllers.firstObject;
}
@end

@interface NEINTestChatNumberBadgedButton : NEINTestButton
@end
@implementation NEINTestChatNumberBadgedButton
@end

@interface NEINTestChatTabPageViewController : UIViewController
@end
@implementation NEINTestChatTabPageViewController
@end

@interface NEINTestTabs : UITabBarController
@property(nonatomic, weak) UISplitViewController *testSplit;
@end
@implementation NEINTestTabs
- (UISplitViewController *)splitViewController {
    return self.testSplit;
}
@end

@interface NEINTestSplit : UISplitViewController
@property(nonatomic) BOOL testCollapsed;
@end
@implementation NEINTestSplit
- (BOOL)isCollapsed {
    return self.testCollapsed;
}
@end

static UIViewController *NEINTestTab(NSString *title, UIViewController *root) {
    UINavigationController *navigation =
        [[UINavigationController alloc] initWithRootViewController:root];
    navigation.tabBarItem.title = title;
    return navigation;
}

static UIBarButtonItem *NEINTestItem(NSString *label) {
    UIBarButtonItem *item = [[UIBarButtonItem alloc]
        initWithImage:[UIImage systemImageNamed:@"star"]
                style:UIBarButtonItemStylePlain target:nil action:NULL];
    item.accessibilityLabel = label;
    return item;
}

API_AVAILABLE(ios(27.1))
static void NEINRunHomeSettingsTests(void) {
    @autoreleasepool {
        UIViewController *home = [UIViewController new];
        UIViewController *chats = [UIViewController new];
        UITabBarController *tabs = [UITabBarController new];
        tabs.viewControllers = @[NEINTestTab(@" 主頁 ", home), NEINTestTab(@"聊天", chats)];
        UINavigationItem *item = home.navigationItem;

        // NEINIPhoneDuo.m installs the hook on iPhone Duo only.
        [tabs viewDidLayoutSubviews];
        assert(!item.pinnedTrailingGroup);
        assert(![tabs respondsToSelector:NEINDuoOpenSettingsSelector()]);

        assert(NEINInstallDuoHomeSettings());
        [tabs viewDidLayoutSubviews];
        UIBarButtonItemGroup *group = item.pinnedTrailingGroup;
        assert(group.barButtonItems.count == 1 && !group.representativeItem);
        UIBarButtonItem *gear = group.barButtonItems.firstObject;
        assert([gear.accessibilityIdentifier isEqualToString:@"nein.duo.settings"]);
        assert([gear.accessibilityLabel isEqualToString:NEINLineSettingsTitle()]);
        assert(gear.target == tabs && gear.action == NEINDuoOpenSettingsSelector());
        assert(gear.visibilityPriority == UIBarButtonItemVisibilityPriorityHigh);
        // Without a window the shared settings route returns before opening.
        ((void (*)(id, SEL, id))objc_msgSend)(tabs, gear.action, gear);
        assert(!tabs.presentedViewController);
        [tabs viewDidLayoutSubviews];
        assert(item.pinnedTrailingGroup == group);
        assert(!NEINDuoHasLineSettings(item));

        // LINE's own settings item wins; the gear returns when it disappears.
        item.rightBarButtonItems = @[NEINTestItem(@"Settings")];
        [tabs viewDidLayoutSubviews];
        assert(!item.pinnedTrailingGroup);
        item.rightBarButtonItems = nil;
        [tabs viewDidLayoutSubviews];
        gear = item.pinnedTrailingGroup.barButtonItems.firstObject;
        assert([gear.accessibilityIdentifier isEqualToString:@"nein.duo.settings"]);
        item.trailingItemGroups = @[[[UIBarButtonItemGroup alloc]
            initWithBarButtonItems:@[NEINTestItem(@"設定")] representativeItem:nil]];
        [tabs viewDidLayoutSubviews];
        assert(!item.pinnedTrailingGroup);
        item.trailingItemGroups = @[];

        // A group LINE pins is wrapped, kept stable and restored as it was.
        UIBarButtonItem *search = NEINTestItem(@"Search");
        UIBarButtonItemGroup *line = [[UIBarButtonItemGroup alloc]
            initWithBarButtonItems:@[search] representativeItem:nil];
        item.pinnedTrailingGroup = line;
        [tabs viewDidLayoutSubviews];
        group = item.pinnedTrailingGroup;
        assert(group != line);
        assert(group.barButtonItems.count == 2 && group.barButtonItems[0] == search);
        assert([group.barButtonItems[1].accessibilityIdentifier
                   isEqualToString:@"nein.duo.settings"]);
        assert([group.representativeItem.accessibilityIdentifier
                   isEqualToString:@"nein.duo.settings.representative"]);
        [tabs viewDidLayoutSubviews];
        assert(item.pinnedTrailingGroup == group);
        item.leftBarButtonItems = @[NEINTestItem(@"설정")];
        [tabs viewDidLayoutSubviews];
        assert(item.pinnedTrailingGroup == line && line.barButtonItems.firstObject == search);
        item.leftBarButtonItems = nil;

        // Pages pushed on Home and tab bars without a Home tab stay untouched.
        UIViewController *pushed = [UIViewController new];
        [home.navigationController pushViewController:pushed animated:NO];
        [tabs viewDidLayoutSubviews];
        assert(!pushed.navigationItem.pinnedTrailingGroup);
        UIViewController *other = [UIViewController new];
        UITabBarController *otherTabs = [UITabBarController new];
        otherTabs.viewControllers = @[NEINTestTab(@"Wallet", other)];
        [otherTabs viewDidLayoutSubviews];
        assert(!other.navigationItem.pinnedTrailingGroup);
        // LINE's custom-view buttons are mirrored before the gear, in screen order.
        UIViewController *lineHome = [UIViewController new];
        UIViewController *lineHomeTab = NEINTestTab(@"主頁", lineHome);
        NEINTestTabs *lineTabs = [NEINTestTabs new];
        lineTabs.viewControllers = @[lineHomeTab, NEINTestTab(@"聊天", [NEINTestChatTabPageViewController new])];
        UIImage *chatImage = [UIImage systemImageNamed:@"bubble"];
        UIImage *chatSelected = [UIImage systemImageNamed:@"bubble.fill"];
        lineTabs.viewControllers[1].tabBarItem.image = chatImage;
        lineTabs.viewControllers[1].tabBarItem.selectedImage = chatSelected;
        UIView *container = [[NEINTestBarButtonItemGroupContainerView alloc]
            initWithFrame:CGRectMake(0, 0, 114, 40)];
        NSArray<NSString *> *labels = @[@"Keep筆記", @"提醒", @"加入好友"];
        NSMutableArray<NEINTestButton *> *sources = [NSMutableArray new];
        for (NSUInteger i = 0; i < labels.count; i++) {
            NEINTestButton *button = [NEINTestButton buttonWithType:UIButtonTypeCustom];
            button.frame = CGRectMake(2 + 37.0 * i, 8, 36, 24);
            [button setImage:[UIImage systemImageNamed:@"star"] forState:UIControlStateNormal];
            button.accessibilityLabel = labels[i];
            [sources addObject:button];
        }
        for (NEINTestButton *button in sources.reverseObjectEnumerator) [container addSubview:button];
        UIBarButtonItem *lineItem = [[UIBarButtonItem alloc] initWithCustomView:container];
        lineHome.navigationItem.rightBarButtonItems = @[lineItem];
        [lineTabs viewDidLayoutSubviews];
        UIBarButtonItemGroup *mirrored = lineHome.navigationItem.pinnedTrailingGroup;
        assert(mirrored.barButtonItems.count == 4 && lineItem.hidden);
        for (NSUInteger i = 0; i < labels.count; i++) {
            assert([mirrored.barButtonItems[i].accessibilityLabel isEqualToString:labels[i]]);
            assert(mirrored.barButtonItems[i].image == sources[i].currentImage);
        }
        assert([mirrored.barButtonItems[3].accessibilityIdentifier isEqualToString:@"nein.duo.settings"]);
        [mirrored.barButtonItems[1].primaryAction performWithSender:nil target:nil];
        assert(sources[1].sent == 1 && sources[0].sent == 0);
        assert(!NEINDuoHasLineSettings(lineHome.navigationItem));

        // Badged images follow the source without rebuilding the group.
        UIImage *badged = [UIImage systemImageNamed:@"bell.badge"];
        [sources[1] setImage:badged forState:UIControlStateNormal];
        [lineTabs viewDidLayoutSubviews];
        assert(lineHome.navigationItem.pinnedTrailingGroup == mirrored);
        assert(mirrored.barButtonItems[1].image == badged);

        // Two columns on the chat tab: the primary column keeps Home's own buttons
        // and the gear, the detail column's toolbar holds tab stand-ins, and the
        // tab bar hides.
        UIViewController *empty = [UIViewController new];
        UINavigationController *detail = [[UINavigationController alloc] initWithRootViewController:empty];
        NEINTestSplit *split = [[NEINTestSplit alloc] initWithStyle:UISplitViewControllerStyleDoubleColumn];
        [split setViewController:lineTabs forColumn:UISplitViewControllerColumnPrimary];
        [split setViewController:detail forColumn:UISplitViewControllerColumnSecondary];
        lineTabs.testSplit = split;
        lineTabs.selectedIndex = 1;
        [lineTabs viewDidLayoutSubviews];
        assert(!lineItem.hidden && lineTabs.tabBarHidden);
        NSArray<UIBarButtonItem *> *homeItems = lineHome.navigationItem.pinnedTrailingGroup.barButtonItems;
        assert(homeItems.count == 1);
        assert([homeItems[0].accessibilityIdentifier isEqualToString:@"nein.duo.settings"]);
        assert(!empty.navigationItem.pinnedTrailingGroup);
        NSArray<UIBarButtonItem *> *tabItems = empty.toolbarItems;
        assert(tabItems.count == 2 && !detail.toolbarHidden);
        assert([tabItems[0].accessibilityIdentifier isEqualToString:@"nein.duo.tab"]);
        assert([tabItems[1].accessibilityLabel isEqualToString:@"聊天"]);
        assert(!tabItems[1].selected && tabItems[1].image == lineTabs.tabBar.items[1].selectedImage);
        assert(tabItems[0].image == lineTabs.tabBar.items[0].image);

        // An opened chat keeps its own pinned items and gets the toolbar too; the
        // page it covers keeps its own.
        UIViewController *chat = [UIViewController new];
        UIBarButtonItem *call = NEINTestItem(@"Call");
        UIBarButtonItemGroup *chatGroup = [[UIBarButtonItemGroup alloc]
            initWithBarButtonItems:@[call] representativeItem:nil];
        chat.navigationItem.pinnedTrailingGroup = chatGroup;
        [detail pushViewController:chat animated:NO];
        [detail viewDidLayoutSubviews];
        assert(empty.toolbarItems.count == 2 && chat.toolbarItems.count == 2);
        assert(chat.navigationItem.pinnedTrailingGroup == chatGroup);

        // A tab stand-in switches tabs; other tabs than chat collapse the split
        // to one column and bring the tab bar back.
        [chat.toolbarItems[0].primaryAction performWithSender:nil target:nil];
        assert(lineTabs.selectedIndex == 0);
        [lineTabs viewDidLayoutSubviews];
        assert(split.traitOverrides.horizontalSizeClass == UIUserInterfaceSizeClassCompact);
        assert(!lineTabs.tabBarHidden);
        assert(!chat.toolbarItems.count && detail.toolbarHidden);
        assert(lineHome.navigationItem.pinnedTrailingGroup.barButtonItems.count == 4);
        lineTabs.selectedIndex = 1;
        [lineTabs viewDidLayoutSubviews];
        assert(![split.traitOverrides containsTrait:UITraitHorizontalSizeClass.class] && lineTabs.tabBarHidden);
        assert(chat.toolbarItems[1].image == lineTabs.tabBar.items[1].selectedImage);
        assert(chat.toolbarItems[1].image != chatImage && !chat.toolbarItems[1].selected);
        assert(lineHome.navigationItem.pinnedTrailingGroup.barButtonItems.count == 1);

        // A chat room that hides the navigation bar gets it back while it hosts,
        // with LINE's own bar buttons mirrored and faded.
        UIViewController *room = [UIViewController new];
        room.view.frame = CGRectMake(0, 0, 600, 600);
        UIView *bar = [[NEINTestChatNavigationBar alloc] initWithFrame:CGRectMake(0, 0, 600, 44)];
        [room.view addSubview:bar];
        NSMutableArray<NEINTestButton *> *roomButtons = [NSMutableArray new];
        for (NSString *label in @[@"搜尋", @"通話", @"選單"]) {
            Class buttonClass = roomButtons.count == 1
                ? NEINTestChatNumberBadgedButton.class : NEINTestChatBadgedButton.class;
            NEINTestButton *button = [buttonClass buttonWithType:UIButtonTypeCustom];
            button.frame = CGRectMake(400 + 46.0 * roomButtons.count, 7, 30, 30);
            [button setImage:[UIImage systemImageNamed:@"star"] forState:UIControlStateNormal];
            button.accessibilityLabel = label;
            [roomButtons addObject:button];
            [bar addSubview:button];
        }
        roomButtons[2].hidden = YES;
        UIBarButtonItem *back = [[UIBarButtonItem alloc] initWithCustomView:[UIView new]];
        room.navigationItem.leftBarButtonItems = @[back];
        [detail pushViewController:room animated:NO];
        detail.navigationBarHidden = YES;
        [detail viewDidLayoutSubviews];
        NSArray<UIBarButtonItem *> *roomItems = room.navigationItem.pinnedTrailingGroup.barButtonItems;
        assert(roomItems.count == 2);
        assert([roomItems[0].accessibilityLabel isEqualToString:@"搜尋"]);
        assert([roomItems[1].accessibilityLabel isEqualToString:@"通話"]);
        assert(roomButtons[0].alpha == 0 && roomButtons[1].alpha == 0);
        assert(!detail.navigationBarHidden && back.hidden);
        assert(room.toolbarItems.count == 2 && chat.toolbarItems.count == 2);
        [roomItems[1].primaryAction performWithSender:nil target:nil];
        assert(roomButtons[1].sent == 1);

        // Covered by a pushed page, the room keeps its group, toolbar and bar,
        // and LINE hiding the bar on the way back is ignored.
        UIBarButtonItemGroup *roomGroup = room.navigationItem.pinnedTrailingGroup;
        UIViewController *roomMenu = [UIViewController new];
        [detail pushViewController:roomMenu animated:NO];
        [detail viewDidLayoutSubviews];
        assert(room.navigationItem.pinnedTrailingGroup == roomGroup && room.toolbarItems.count == 2);
        assert(roomMenu.toolbarItems.count == 2);
        [detail popViewControllerAnimated:NO];
        detail.navigationBarHidden = YES;
        [detail setNavigationBarHidden:YES animated:YES];
        assert(!detail.navigationBarHidden);
        [detail viewDidLayoutSubviews];
        assert(room.navigationItem.pinnedTrailingGroup == roomGroup && room.toolbarItems.count == 2);

        // Collapsing brings Home's buttons back to the vertical bar and the tab bar back.
        lineTabs.selectedIndex = 0;
        split.testCollapsed = YES;
        [lineTabs viewDidLayoutSubviews];
        assert(lineHome.navigationItem.pinnedTrailingGroup.barButtonItems.count == 4);
        assert(lineItem.hidden && !lineTabs.tabBarHidden);
        assert(!room.navigationItem.pinnedTrailingGroup && !room.toolbarItems.count);
        assert(detail.navigationBarHidden && !back.hidden && detail.toolbarHidden);

        // The chat list's header pill is mirrored and faded, minus the calendar
        // LINE drops later; its title stays.
        NEINTestChatTabPageViewController *list = [NEINTestChatTabPageViewController new];
        list.view.frame = CGRectMake(0, 0, 466, 678);
        UIButton *titleButton = [UIButton buttonWithType:UIButtonTypeSystem];
        titleButton.frame = CGRectMake(16, 20, 66, 44);
        [list.view addSubview:titleButton];
        // Five buttons at launch reach past the screen's middle.
        UIView *capsule = [[UIView alloc] initWithFrame:CGRectMake(150, 20, 216, 44)];
        [list.view addSubview:capsule];
        UIView *pill = [[NEINTestBarButtonItemGroupContainerView alloc]
            initWithFrame:CGRectMake(4, 4, 208, 36)];
        [capsule addSubview:pill];
        NSMutableArray<NEINTestButton *> *pillButtons = [NSMutableArray new];
        for (NSString *label in @[@"編輯", @"行事曆", @"開始聊天"]) {
            NEINTestButton *button = [NEINTestButton buttonWithType:UIButtonTypeCustom];
            button.frame = CGRectMake(8 + 40.0 * pillButtons.count, 10, 36, 24);
            [button setImage:[UIImage systemImageNamed:@"star"] forState:UIControlStateNormal];
            button.accessibilityLabel = label;
            [pillButtons addObject:button];
            [pill addSubview:button];
        }
        UISearchBar *searchBar = [[UISearchBar alloc] initWithFrame:CGRectMake(16, 80, 350, 39)];
        [list.view addSubview:searchBar];
        NEINTestTabs *listTabs = [NEINTestTabs new];
        UIViewController *listTab = NEINTestTab(@"聊天", list);
        listTabs.viewControllers = @[NEINTestTab(@"主頁", [UIViewController new]), listTab];
        listTabs.selectedViewController = listTab;
        ((UINavigationController *)listTab).navigationBarHidden = YES;
        [listTabs viewDidLayoutSubviews];
        NSArray<UIBarButtonItem *> *listItems = list.navigationItem.pinnedTrailingGroup.barButtonItems;
        assert(listItems.count == 2 && !pillButtons[1].superview);
        assert([listItems[1].accessibilityLabel isEqualToString:@"開始聊天"]);
        assert(capsule.alpha == 0 && titleButton.alpha == 1);
        assert(!((UINavigationController *)listTab).navigationBarHidden);

        // LINE replaces header buttons without a layout pass; a control moving
        // windows refreshes the mirrors on the next main queue turn.
        [pillButtons[0] removeFromSuperview];
        NEINTestButton *swapped = [NEINTestButton buttonWithType:UIButtonTypeCustom];
        swapped.frame = CGRectMake(8, 10, 36, 24);
        swapped.accessibilityLabel = @"相簿";
        [pill addSubview:swapped];
        [swapped didMoveToWindow];
        assert(list.navigationItem.pinnedTrailingGroup.barButtonItems.count == 2);
        assert([list.navigationItem.pinnedTrailingGroup.barButtonItems[0].accessibilityLabel
                   isEqualToString:@"編輯"]);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
        listItems = list.navigationItem.pinnedTrailingGroup.barButtonItems;
        assert(listItems.count == 2 && [listItems[0].accessibilityLabel isEqualToString:@"相簿"]);

        // Unfolded, the chat list in the primary column shows its own pill again.
        UIViewController *listDetail = [[UINavigationController alloc]
            initWithRootViewController:[UIViewController new]];
        NEINTestSplit *listSplit = [[NEINTestSplit alloc] initWithStyle:UISplitViewControllerStyleDoubleColumn];
        [listSplit setViewController:listTabs forColumn:UISplitViewControllerColumnPrimary];
        [listSplit setViewController:listDetail forColumn:UISplitViewControllerColumnSecondary];
        listTabs.testSplit = listSplit;
        [listTabs viewDidLayoutSubviews];
        assert(capsule.alpha == 1 && !list.navigationItem.pinnedTrailingGroup);
        NEINTestButton *calendar = [NEINTestButton buttonWithType:UIButtonTypeCustom];
        calendar.accessibilityLabel = @"行事曆";
        [pill addSubview:calendar];
        [listTabs viewDidLayoutSubviews];
        assert(!calendar.superview && swapped.superview == pill);
        assert(((UINavigationController *)listTab).navigationBarHidden);

        // A pushed page's own back button gives way to the system one, or moves
        // to the vertical bar when the page hides that.
        UIViewController *menu = [UIViewController new];
        NEINTestButton *menuBack = [NEINTestButton buttonWithType:UIButtonTypeCustom];
        [menuBack setImage:[UIImage systemImageNamed:@"chevron.backward"] forState:UIControlStateNormal];
        UIBarButtonItem *menuBackItem = [[UIBarButtonItem alloc] initWithCustomView:menuBack];
        menu.navigationItem.leftBarButtonItems = @[menuBackItem];
        UINavigationController *listNavigation = (UINavigationController *)listTab;
        listNavigation.navigationBarHidden = NO;
        listTabs.testSplit = nil;
        [listNavigation pushViewController:menu animated:NO];
        [listNavigation viewDidLayoutSubviews];
        assert(menuBackItem.hidden && !menu.navigationItem.pinnedTrailingGroup);
        menu.navigationItem.hidesBackButton = YES;
        [listNavigation viewDidLayoutSubviews];
        NSArray<UIBarButtonItem *> *menuItems = menu.navigationItem.pinnedTrailingGroup.barButtonItems;
        assert(menuBackItem.hidden && menuItems.count == 1);
        [menuItems[0].primaryAction performWithSender:nil target:nil];
        assert(menuBack.sent == 1);
        [listNavigation popViewControllerAnimated:NO];
        [listNavigation viewDidLayoutSubviews];
        assert(!menuBackItem.hidden && !menu.navigationItem.pinnedTrailingGroup);

        // A presented page's close button moves to the trailing edge.
        UIViewController *sheet = [UIViewController new];
        NEINTestButton *close = [NEINTestButton buttonWithType:UIButtonTypeCustom];
        [close setImage:[UIImage systemImageNamed:@"xmark"] forState:UIControlStateNormal];
        UIBarButtonItem *closeItem = [[UIBarButtonItem alloc] initWithCustomView:close];
        sheet.navigationItem.leftBarButtonItems = @[closeItem];
        NEINTestPresentedNavigation *modal = [[NEINTestPresentedNavigation alloc]
            initWithRootViewController:sheet];
        [modal viewDidLayoutSubviews];
        NSArray<UIBarButtonItem *> *sheetItems = sheet.navigationItem.pinnedTrailingGroup.barButtonItems;
        assert(closeItem.hidden && sheetItems.count == 1);
        [modal viewDidLayoutSubviews];
        assert(sheet.navigationItem.pinnedTrailingGroup.barButtonItems.firstObject == sheetItems[0]);
        [sheetItems[0].primaryAction performWithSender:nil target:nil];
        assert(close.sent == 1);
        puts("iPhone Duo Home settings tests passed (Catalyst UIKit).");
    }
}

int main(void) {
    if (@available(iOS 27.1, *)) NEINRunHomeSettingsTests();
    else puts("iPhone Duo Home settings tests skipped (requires the iOS 27.1 UIKit runtime).");
    return 0;
}
