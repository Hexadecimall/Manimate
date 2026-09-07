#include "MacWindow.h"

#include <QGuiApplication>
#include <QWidget>

#ifdef Q_OS_MACOS
#include <AppKit/AppKit.h>

namespace mn::ui::mac {

void disableNativeShadow(QWidget *window)
{
    if (!window)
        return;

    // winId() is an NSView only under the Cocoa platform. Offscreen and
    // minimal hand back something else entirely, and messaging that as if it
    // were an object crashes — which is what happened on a headless runner.
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
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
