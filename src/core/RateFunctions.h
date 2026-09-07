#pragma once

#include <QString>
#include <QStringList>

namespace mn {

/// Manim's easing curves, reimplemented so the canvas moves the way the
/// rendered video will.
namespace rate {

/// Map a linear 0..1 through the named curve. An unknown name is linear.
double apply(const QString &name, double t);

/// Names the editor offers, in a sensible order.
const QStringList &names();

} // namespace rate
} // namespace mn
