// iPhone Duo gets the system vertical bar on wide layouts, LINE's iPad-style
// split layout on the unfolded inner display, and a camera preview kept inside
// the short Duo windows. Home gets a settings gear when LINE shows none in its
// navigation bar.
#import <UIKit/UIKit.h>
#include "NEINDuoSplitLayout.h"
#include "NEINDuoCameraLayout.h"
#include "NEINDuoHomeButtons.h"

static void NEINInstallIPhoneDuoCompat(void) {
    NSString *model = NEINDuoMachineModel();
    NEINInstallDuoSplitLayout(model);
    if (@available(iOS 27.1, *)) {
        if ([model isEqualToString:NEINDuoModelIdentifier]) NEINInstallDuoHomeSettings();
    }
    NEINInstallDuoCameraLayout();
}
