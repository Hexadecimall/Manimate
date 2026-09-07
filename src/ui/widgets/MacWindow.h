#pragma once

#include <QtGlobal>

class QWidget;

namespace mn::ui::mac {

/// Turn off the shadow macOS draws for a window.
///
/// A translucent frameless window gets its shadow from its alpha mask, and
/// macOS caches that mask: after a resize or a move the old shape can be left
/// hanging in the air as an outline beside the window. The application paints
/// its own shadow anyway, so the native one is redundant as well as wrong.
#ifdef Q_OS_MACOS
void disableNativeShadow(QWidget *window);
#else
inline void disableNativeShadow(QWidget *) {}
#endif

} // namespace mn::ui::mac
