#include "MacWindow.h"

#include <QWidget>

#ifdef Q_OS_MACOS
#include <AppKit/AppKit.h>

namespace mn::ui::mac {

void disableNativeShadow(QWidget *window)
{
    if (!window)
        return;

    window->winId();   // force the native handle into existence
    auto *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nsWindow = view.window;
    if (!nsWindow)
        return;

    nsWindow.hasShadow = NO;
    [nsWindow invalidateShadow];
}

} // namespace mn::ui::mac
#endif
