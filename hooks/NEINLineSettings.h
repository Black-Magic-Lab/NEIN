// Opens LINE's own settings page from NEIN's Settings tab and the iPhone Duo
// Home gear.
#pragma once

#import <UIKit/UIKit.h>

static void NEINOpenLineSettings(UITabBarController *controller) {
    // Route inside this app. UIApplication.openURL could launch the original LINE.
    if (!controller.viewIfLoaded.window || controller.presentedViewController) return;
    id<UIApplicationDelegate> delegate = UIApplication.sharedApplication.delegate;
    SEL selector = @selector(application:openURL:options:);
    NSURL *url = [NSURL URLWithString:@"line://nv/settings"];
    if ([delegate respondsToSelector:selector] &&
        [delegate application:UIApplication.sharedApplication openURL:url options:@{}]) return;
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"NEIN"
        message:@"目前無法透過內部路由開啟設定，請稍後再試。"
        preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"好" style:UIAlertActionStyleDefault handler:nil]];
    [controller presentViewController:alert animated:YES completion:nil];
}
