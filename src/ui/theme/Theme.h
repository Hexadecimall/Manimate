#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QApplication;

namespace mn::theme {

/// Every colour the interface uses. Widgets pull from here rather than naming
/// colours inline, so the whole application can be retinted in one place.
///
/// The accent is Manim's own blue and the highlight colours are Manim's
/// palette, so the editor reads as part of the same toolchain as the videos it
/// produces.
struct Palette
{
    /// The deepest surface: what shows behind and between panels.
    QColor window{0x16, 0x17, 0x1A};

    /// A panel's own background.
    QColor surface{0x1F, 0x20, 0x24};

    /// Raised within a panel: headers, toolbars, clips.
    QColor surfaceRaised{0x26, 0x27, 0x2C};
    QColor surfaceHover{0x2F, 0x30, 0x37};
    QColor surfaceActive{0x38, 0x39, 0x42};

    /// Panels are separated by lines DARKER than they are, which is what makes
    /// a dense editing interface read as panels rather than as boxes.
    QColor border{0x0E, 0x0F, 0x11};

    /// A lighter line, for outlines inside a panel.
    QColor borderStrong{0x3B, 0x3D, 0x46};

    QColor text{0xDA, 0xDC, 0xE2};
    QColor textMuted{0x8E, 0x92, 0x9C};
    QColor textFaint{0x5E, 0x62, 0x6C};

    QColor accent{0x58, 0xC4, 0xDD};
    QColor accentHover{0x7E, 0xD4, 0xE6};
    QColor accentPressed{0x3F, 0xA9, 0xC2};
    QColor onAccent{0x08, 0x1A, 0x1F};

    QColor danger{0xFC, 0x62, 0x55};
    QColor success{0x83, 0xC1, 0x67};
    QColor warning{0xF0, 0xC2, 0x4B};
    QColor violet{0x9A, 0x72, 0xAC};
    QColor teal{0x5C, 0xD0, 0xB3};
};

const Palette &palette();

/// Corner radius shared by cards, buttons and inputs.
inline constexpr int kRadius = 8;
inline constexpr int kRadiusSmall = 6;

/// The application-wide stylesheet, built from the palette.
QString styleSheet();

/// Install the dark palette, stylesheet and default font.
void apply(QApplication &application);

/// Display font at a given point size and weight.
QFont font(int pointSize, QFont::Weight weight = QFont::Normal);

/// Mix two colours; `t` of 0 returns `a`, 1 returns `b`.
QColor mix(const QColor &a, const QColor &b, qreal t);

} // namespace mn::theme
