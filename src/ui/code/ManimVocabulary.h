#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace mn::ui {

/// The names Manim brings into scope, grouped by what they are.
///
/// The highlighter colours by group and the completer offers the same names, so
/// both agree on what Manim is without either owning the list. Groups are kept
/// separate rather than merged into one blob because a colour is a different
/// kind of thing from an animation, and reads better in a different colour.
namespace manim {

/// Mobjects: things that exist in a scene. Circle, Text, Axes, Cube.
const QStringList &mobjects();

/// Animations: things that happen to mobjects. Create, Write, Transform.
const QStringList &animations();

/// Scene base classes. Scene, ThreeDScene, MovingCameraScene.
const QStringList &scenes();

/// Colour constants. WHITE, BLUE_E, PURE_RED.
const QStringList &colors();

/// Direction and numeric constants. UP, ORIGIN, PI, DEGREES, MED_LARGE_BUFF.
const QStringList &constants();

/// Rate functions used to shape an animation over time. smooth, linear.
const QStringList &rateFunctions();

/// Methods a Scene subclass calls on itself. play, wait, add.
const QStringList &sceneMethods();

/// Methods most mobjects have. shift, move_to, set_color, next_to.
const QStringList &mobjectMethods();

/// Every group at once, for completion.
const QStringList &all();

/// Fast membership tests for the highlighter, which asks per word per repaint.
bool isMobject(const QString &word);
bool isAnimation(const QString &word);
bool isScene(const QString &word);
bool isColor(const QString &word);
bool isConstant(const QString &word);
bool isRateFunction(const QString &word);

} // namespace manim

/// Python's own keywords and builtins, so the editor knows the language the
/// Manim names are written in.
namespace python {

const QStringList &keywords();
const QStringList &builtins();
const QStringList &softKeywords();

bool isKeyword(const QString &word);
bool isBuiltin(const QString &word);

} // namespace python
} // namespace mn::ui
