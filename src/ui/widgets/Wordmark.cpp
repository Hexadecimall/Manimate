#include "Wordmark.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace mn::ui {
namespace {

constexpr int kGlyphGap = 7;
constexpr int kTextGap = 14;

QFont wordmarkFont(int glyphSize)
{
    QFont font = theme::font(1, QFont::DemiBold);
    font.setPixelSize(int(glyphSize * 0.78));
    font.setLetterSpacing(QFont::PercentageSpacing, 112.0);
    return font;
}

QFont subtitleFont(int glyphSize)
{
    QFont font = theme::font(1);
    font.setPixelSize(qMax(10, int(glyphSize * 0.34)));
    font.setLetterSpacing(QFont::PercentageSpacing, 108.0);
    return font;
}

} // namespace

Wordmark::Wordmark(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void Wordmark::setGlyphSize(int pixels)
{
    m_glyphSize = qMax(12, pixels);
    updateGeometry();
    update();
}

void Wordmark::setSubtitle(const QString &subtitle)
{
    m_subtitle = subtitle;
    updateGeometry();
    update();
}

QSize Wordmark::sizeHint() const
{
    const int glyphWidth = m_glyphSize * 3 + kGlyphGap * 2;
    const int textWidth = QFontMetrics(wordmarkFont(m_glyphSize)).horizontalAdvance(QStringLiteral("MANIMATE"));

    int height = m_glyphSize;
    if (!m_subtitle.isEmpty())
        height += QFontMetrics(subtitleFont(m_glyphSize)).height() + 2;

    return {glyphWidth + kTextGap + textWidth, qMax(m_glyphSize, height)};
}

void Wordmark::paintEvent(QPaintEvent *)
{
    const theme::Palette &p = theme::palette();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal size = m_glyphSize;
    const qreal stroke = qMax(1.6, size * 0.085);
    const qreal inset = stroke / 2.0;
    qreal x = 0.0;
    const qreal y = 0.0;

    // Circle, square, triangle: Manim's own primitives, in Manim's own colours,
    // which are fixed rather than taken from the interface's accent.
    painter.setBrush(Qt::NoBrush);

    painter.setPen(QPen(p.manimBlue, stroke));
    painter.drawEllipse(QRectF(x + inset, y + inset, size - stroke, size - stroke));
    x += size + kGlyphGap;

    painter.setPen(QPen(p.manimGreen, stroke));
    const qreal squareInset = size * 0.08;
    painter.drawRoundedRect(QRectF(x + inset + squareInset, y + inset + squareInset,
                                   size - stroke - squareInset * 2, size - stroke - squareInset * 2),
                            size * 0.12, size * 0.12);
    x += size + kGlyphGap;

    QPainterPath triangle;
    const qreal margin = size * 0.06;
    triangle.moveTo(x + size / 2.0, y + margin + inset);
    triangle.lineTo(x + size - margin - inset, y + size - margin - inset);
    triangle.lineTo(x + margin + inset, y + size - margin - inset);
    triangle.closeSubpath();
    painter.setPen(QPen(p.manimRed, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(triangle);

    x += size + kTextGap;

    // Shrink the name rather than let it run off the edge when the widget is
    // narrower than the text would like to be.
    QFont nameFont = wordmarkFont(m_glyphSize);
    const qreal available = width() - x;
    if (available > 0) {
        qreal needed = QFontMetrics(nameFont).horizontalAdvance(QStringLiteral("MANIMATE"));
        while (needed > available && nameFont.pixelSize() > 9) {
            nameFont.setPixelSize(nameFont.pixelSize() - 1);
            needed = QFontMetrics(nameFont).horizontalAdvance(QStringLiteral("MANIMATE"));
        }
    }
    const QFontMetrics nameMetrics(nameFont);

    qreal textTop = y;
    if (!m_subtitle.isEmpty()) {
        const QFontMetrics subtitleMetrics{subtitleFont(m_glyphSize)};
        textTop = y + (size - nameMetrics.height() - subtitleMetrics.height() - 2) / 2.0;
    } else {
        textTop = y + (size - nameMetrics.height()) / 2.0;
    }

    painter.setFont(nameFont);
    painter.setPen(p.text);
    const QRectF nameRect(x, textTop, width() - x, nameMetrics.height());
    painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("MANIMATE"));

    if (!m_subtitle.isEmpty()) {
        const QFont small = subtitleFont(m_glyphSize);
        painter.setFont(small);
        painter.setPen(p.textFaint);
        const QRectF subtitleRect(x, nameRect.bottom() + 1, width() - x, QFontMetrics(small).height());
        painter.drawText(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, m_subtitle);
    }
}

} // namespace mn::ui
