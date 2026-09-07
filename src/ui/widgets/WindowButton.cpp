#include "WindowButton.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>

namespace mn::ui {
namespace {

constexpr int kTrafficDiameter = 12;
constexpr int kTrafficSpacing = 20;
constexpr int kGlyphWidth = 44;
constexpr int kGlyphHeight = 32;

} // namespace

WindowButton::Appearance WindowButton::nativeAppearance()
{
#ifdef Q_OS_MACOS
    return Appearance::Traffic;
#else
    return Appearance::Glyph;
#endif
}

WindowButton::WindowButton(Kind kind, Appearance appearance, QWidget *parent)
    : QAbstractButton(parent)
    , m_kind(kind)
    , m_appearance(appearance)
{
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);

    switch (kind) {
    case Kind::Close:
        setToolTip(tr("Close"));
        break;
    case Kind::Minimize:
        setToolTip(tr("Minimise"));
        break;
    case Kind::Maximize:
        setToolTip(tr("Zoom"));
        break;
    }
}

void WindowButton::setGroupHovered(bool hovered)
{
    if (m_groupHovered == hovered)
        return;
    m_groupHovered = hovered;
    update();
}

void WindowButton::setRestoreState(bool restore)
{
    if (m_restore == restore)
        return;
    m_restore = restore;
    update();
}

QSize WindowButton::sizeHint() const
{
    if (m_appearance == Appearance::Traffic)
        return {kTrafficSpacing, kTrafficSpacing};
    return {kGlyphWidth, kGlyphHeight};
}

void WindowButton::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    update();
}

void WindowButton::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    update();
}

void WindowButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_appearance == Appearance::Traffic)
        paintTraffic(painter);
    else
        paintGlyph(painter);
}

void WindowButton::paintTraffic(QPainter &painter)
{
    const theme::Palette &p = theme::palette();

    QColor fill;
    switch (m_kind) {
    case Kind::Close:
        fill = QColor(0xFF, 0x5F, 0x57);
        break;
    case Kind::Minimize:
        fill = QColor(0xFE, 0xBC, 0x2E);
        break;
    case Kind::Maximize:
        fill = QColor(0x28, 0xC8, 0x40);
        break;
    }

    const bool lit = m_groupHovered || underMouse();
    if (!lit)
        fill = theme::mix(p.surfaceActive, fill, 0.28);
    if (isDown())
        fill = fill.darker(125);

    const QRectF disc(QPointF((width() - kTrafficDiameter) / 2.0, (height() - kTrafficDiameter) / 2.0),
                      QSizeF(kTrafficDiameter, kTrafficDiameter));
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawEllipse(disc);

    if (!lit)
        return;

    // The glyph only appears once the group is hovered, as on macOS.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 160), 1.2, Qt::SolidLine, Qt::RoundCap));

    const QPointF center = disc.center();
    constexpr qreal arm = 2.6;
    switch (m_kind) {
    case Kind::Close:
        painter.drawLine(QPointF(center.x() - arm, center.y() - arm), QPointF(center.x() + arm, center.y() + arm));
        painter.drawLine(QPointF(center.x() - arm, center.y() + arm), QPointF(center.x() + arm, center.y() - arm));
        break;
    case Kind::Minimize:
        painter.drawLine(QPointF(center.x() - arm - 0.6, center.y()), QPointF(center.x() + arm + 0.6, center.y()));
        break;
    case Kind::Maximize:
        painter.drawLine(QPointF(center.x() - arm, center.y() + arm), QPointF(center.x() + arm, center.y() - arm));
        painter.drawLine(QPointF(center.x() - arm, center.y() + arm), QPointF(center.x() - arm, center.y() - 0.4));
        painter.drawLine(QPointF(center.x() - arm, center.y() + arm), QPointF(center.x() + 0.4, center.y() + arm));
        break;
    }
}

void WindowButton::paintGlyph(QPainter &painter)
{
    const theme::Palette &p = theme::palette();

    QColor background = Qt::transparent;
    QColor foreground = p.textMuted;

    if (underMouse()) {
        foreground = m_kind == Kind::Close ? QColor(Qt::white) : p.text;
        background = m_kind == Kind::Close ? p.danger : p.surfaceHover;
        if (isDown())
            background = background.darker(115);
    }

    if (background.alpha() > 0)
        painter.fillRect(rect(), background);

    painter.setPen(QPen(foreground, 1.2));
    painter.setBrush(Qt::NoBrush);

    const QPointF center = QRectF(rect()).center();
    constexpr qreal half = 5.0;

    switch (m_kind) {
    case Kind::Close:
        painter.drawLine(QPointF(center.x() - half, center.y() - half), QPointF(center.x() + half, center.y() + half));
        painter.drawLine(QPointF(center.x() - half, center.y() + half), QPointF(center.x() + half, center.y() - half));
        break;
    case Kind::Minimize:
        painter.drawLine(QPointF(center.x() - half, center.y()), QPointF(center.x() + half, center.y()));
        break;
    case Kind::Maximize:
        if (m_restore) {
            painter.drawRect(QRectF(center.x() - half, center.y() - half + 2.5, half * 2 - 2.5, half * 2 - 2.5));
            painter.drawPolyline(QPolygonF({QPointF(center.x() - half + 2.5, center.y() - half + 2.5),
                                            QPointF(center.x() - half + 2.5, center.y() - half),
                                            QPointF(center.x() + half, center.y() - half),
                                            QPointF(center.x() + half, center.y() + half - 2.5),
                                            QPointF(center.x() + half - 2.5, center.y() + half - 2.5)}));
        } else {
            painter.drawRect(QRectF(center.x() - half, center.y() - half, half * 2, half * 2));
        }
        break;
    }
}

} // namespace mn::ui
