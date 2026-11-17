#import <UIKit/UIKit.h>
#include <assert.h>

// Real UIKit tab bar controllers (Mac Catalyst). Calls made here come from the
// main executable, so they see what LINE's own code would see; the bar's items
// show what UIKit really holds.
#include "../hooks/NEINVisibleTabModel.h"
#include "../hooks/NEINDuoTabRemoval.h"

static UIViewController *NEINTestTab(NSString *title) {
    UINavigationController *navigation =
        [[UINavigationController alloc] initWithRootViewController:[UIViewController new]];
    navigation.tabBarItem.title = title;
    return navigation;
}

static NSString *NEINTestBarTitles(UITabBarController *tabs) {
    // What UIKit holds; tabBar.items read from here is LINE's unfiltered view.
    NSArray *items = NEINDuoOriginalBarItems(tabs.tabBar, @selector(items));
    return [[items valueForKey:@"title"] componentsJoinedByString:@","];
}

int main(void) {
    @autoreleasepool {
        (void)NEINVisibleTabDestination;
        NSArray<UIViewController *> *line = @[
            NEINTestTab(@"主頁"), NEINTestTab(@"聊天"), NEINTestTab(@"LINE VOOM"),
            NEINTestTab(@"逛逛"), NEINTestTab(@"通話"), NEINTestTab(@"錢包")];
        UITabBarController *before = [UITabBarController new];
        before.viewControllers = line;
        assert(before.tabBar.items.count == 6);

        assert(NEINInstallDuoTabRemoval());
        UITabBarController *tabs = [UITabBarController new];
        [tabs setViewControllers:line animated:NO];
        assert([NEINTestBarTitles(tabs) isEqualToString:@"主頁,聊天,通話,錢包"]);
        // LINE keeps its own list and indices.
        assert([tabs.viewControllers isEqualToArray:line]);
        assert(tabs.tabBar.items.count == 6 && tabs.tabBar.items[4] == line[4].tabBarItem);
        assert(tabs.selectedIndex == 0);
        tabs.selectedIndex = 4;
        assert(tabs.selectedViewController == line[4] && tabs.selectedIndex == 4);
        assert(tabs.tabBar.selectedItem == line[4].tabBarItem);
        // Removed tabs cannot be selected; out-of-range requests reach UIKit unchanged.
        tabs.selectedIndex = 2;
        assert(tabs.selectedViewController == line[4]);
        tabs.selectedIndex = 5;
        assert(tabs.selectedViewController == line[5] && tabs.selectedIndex == 5);

        // LINE appends a tab to the list it reads back.
        NSArray<UIViewController *> *more =
            [tabs.viewControllers arrayByAddingObject:NEINTestTab(@"設定")];
        tabs.viewControllers = more;
        assert([NEINTestBarTitles(tabs) isEqualToString:@"主頁,聊天,通話,錢包,設定"]);
        assert([tabs.viewControllers isEqualToArray:more]);
        tabs.selectedIndex = 6;
        assert(tabs.selectedViewController == more[6] && tabs.selectedIndex == 6);

        // Tabs titled after they were handed over are removed on the next layout.
        UIViewController *late = NEINTestTab(@"");
        NSArray<UIViewController *> *untitled = @[
            NEINTestTab(@"Home"), late, NEINTestTab(@"Chats"), NEINTestTab(@"Calls")];
        UITabBarController *lateTabs = [UITabBarController new];
        lateTabs.viewControllers = untitled;
        lateTabs.selectedIndex = 2;
        assert([NEINTestBarTitles(lateTabs) isEqualToString:@"Home,,Chats,Calls"]);
        late.tabBarItem.title = @"News";
        [lateTabs viewDidLayoutSubviews];
        assert([NEINTestBarTitles(lateTabs) isEqualToString:@"Home,Chats,Calls"]);
        assert(lateTabs.selectedViewController == untitled[2] && lateTabs.selectedIndex == 2);
        [lateTabs viewDidLayoutSubviews];
        assert([NEINTestBarTitles(lateTabs) isEqualToString:@"Home,Chats,Calls"]);
        assert(lateTabs.tabBar.items.count == 4);

        // Other tab bars, and lists without promotional tabs, are left alone.
        NSArray<UIViewController *> *other = @[NEINTestTab(@"VOOM"), NEINTestTab(@"Wallet")];
        UITabBarController *otherTabs = [UITabBarController new];
        otherTabs.viewControllers = other;
        [otherTabs viewDidLayoutSubviews];
        assert(otherTabs.tabBar.items.count == 2 && !NEINDuoUnfilteredTabs(otherTabs));
        tabs.viewControllers = @[line[0], line[1], line[4]];
        assert(!NEINDuoUnfilteredTabs(tabs) && tabs.tabBar.items.count == 3);
        tabs.selectedIndex = 2;
        assert(tabs.selectedViewController == line[4]);
        puts("iPhone Duo tab removal tests passed (Catalyst UIKit).");
    }
    return 0;
}
