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
    QColor window{0x0E, 0x0F, 0x12};
    QColor surface{0x14, 0x16, 0x1A};
    QColor surfaceRaised{0x1B, 0x1E, 0x24};
    QColor surfaceHover{0x23, 0x27, 0x2F};
    QColor surfaceActive{0x2B, 0x30, 0x3A};
    QColor border{0x2A, 0x2F, 0x38};
    QColor borderStrong{0x3A, 0x41, 0x4D};

    QColor text{0xE6, 0xE9, 0xEF};
    QColor textMuted{0x9A, 0xA3, 0xB2};
    QColor textFaint{0x6B, 0x74, 0x83};

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
