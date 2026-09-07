#include "WindowButton.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>

namespace mn::ui {
namespace {

constexpr int kButtonWidth = 30;
constexpr int kButtonHeight = 26;
constexpr qreal kPanelRadius = 6.0;
constexpr qreal kGlyphArm = 4.2;

} // namespace

bool WindowButton::controlsBelongOnTheLeft()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

WindowButton::WindowButton(Kind kind, QWidget *parent)
    : QAbstractButton(parent)
    , m_kind(kind)
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
    return {kButtonWidth, kButtonHeight};
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
    const theme::Palette &p = theme::palette();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool hovered = underMouse();

    // The panel appears under the pointer; close is the only one that colours,
    // because it is the only one worth hesitating over.
    if (hovered) {
        QColor panel = m_kind == Kind::Close ? p.danger : p.surfaceHover;
        if (isDown())
            panel = panel.darker(120);

        QPainterPath shape;
        shape.addRoundedRect(QRectF(rect()).adjusted(1, 2, -1, -2), kPanelRadius, kPanelRadius);
        painter.fillPath(shape, panel);
    }

    QColor ink = p.textFaint;
    if (m_groupHovered)
        ink = p.textMuted;
    if (hovered)
        ink = m_kind == Kind::Close ? QColor(Qt::white) : p.text;

    const QPointF center = QRectF(rect()).center();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    switch (m_kind) {
    case Kind::Close:
        painter.drawLine(QPointF(center.x() - kGlyphArm, center.y() - kGlyphArm),
                         QPointF(center.x() + kGlyphArm, center.y() + kGlyphArm));
        painter.drawLine(QPointF(center.x() - kGlyphArm, center.y() + kGlyphArm),
                         QPointF(center.x() + kGlyphArm, center.y() - kGlyphArm));
        break;

    case Kind::Minimize:
        painter.drawLine(QPointF(center.x() - kGlyphArm - 0.6, center.y()),
                         QPointF(center.x() + kGlyphArm + 0.6, center.y()));
        break;

    case Kind::Maximize:
        if (m_restore) {
            // Two overlapping outlines: the window stepping back out of full size.
            const qreal a = kGlyphArm - 0.8;
            painter.drawRect(QRectF(center.x() - a - 0.8, center.y() - a + 1.6, a * 2, a * 2));
            painter.drawPolyline(QPolygonF({QPointF(center.x() - a + 1.6, center.y() - a + 1.6),
                                            QPointF(center.x() - a + 1.6, center.y() - a),
                                            QPointF(center.x() + a + 0.8, center.y() - a),
                                            QPointF(center.x() + a + 0.8, center.y() + a - 1.6),
                                            QPointF(center.x() + a - 1.6, center.y() + a - 1.6)}));
        } else {
            painter.drawRoundedRect(
                QRectF(center.x() - kGlyphArm, center.y() - kGlyphArm, kGlyphArm * 2, kGlyphArm * 2),
                1.5, 1.5);
        }
        break;
    }
}

} // namespace mn::ui
