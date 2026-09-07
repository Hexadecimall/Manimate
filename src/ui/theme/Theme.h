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
    /// The deepest surface: what shows behind and between panels. Slate
    /// rather than neutral grey — a trace of blue keeps a near-black interface
    /// from looking muddy.
    QColor window{0x14, 0x16, 0x1B};

    /// A panel's own background.
    QColor surface{0x1D, 0x21, 0x27};

    /// Raised within a panel: headers, toolbars, clips.
    QColor surfaceRaised{0x25, 0x2A, 0x32};
    QColor surfaceHover{0x2E, 0x34, 0x3E};
    QColor surfaceActive{0x38, 0x3F, 0x4B};

    /// Panels are separated by lines DARKER than they are, which is what makes
    /// a dense editing interface read as panels rather than as boxes.
    QColor border{0x0D, 0x0F, 0x13};

    /// A lighter line, for outlines inside a panel.
    QColor borderStrong{0x3E, 0x45, 0x52};

    QColor text{0xDC, 0xE0, 0xE7};
    QColor textMuted{0x91, 0x99, 0xA6};
    QColor textFaint{0x60, 0x68, 0x75};

    /// The interface's accent. Warm rather than cold, so it reads as an
    /// editing application; Manim's own palette still colours the scene.
    QColor accent{0xE5, 0x2E, 0x22};
    QColor accentHover{0xF5, 0x47, 0x3A};
    QColor accentPressed{0xBE, 0x24, 0x19};
    QColor onAccent{0xFA, 0xEC, 0xEA};

    QColor danger{0xE5, 0x48, 0x4D};
    QColor success{0x83, 0xC1, 0x67};
    QColor warning{0xF0, 0xC2, 0x4B};
    QColor violet{0x9A, 0x72, 0xAC};
    QColor teal{0x5C, 0xD0, 0xB3};

    /// Manim's own colours. Fixed, not derived from the interface's accent:
    /// the logo means Manim, so it must not change when the theme does.
    QColor manimBlue{0x58, 0xC4, 0xDD};
    QColor manimGreen{0x83, 0xC1, 0x67};
    QColor manimRed{0xFC, 0x62, 0x55};
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
