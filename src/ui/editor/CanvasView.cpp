#include "CanvasView.h"

#include "EditorState.h"
#include "SceneRenderer.h"
#include "Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace mn::ui {
namespace {

constexpr int kMargin = 18;
constexpr double kHandleSize = 6.0;

} // namespace

CanvasView::CanvasView(EditorState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(320, 200);

    connect(m_state, &EditorState::documentChanged, this, QOverload<>::of(&QWidget::update));
    connect(m_state, &EditorState::selectionChanged, this, QOverload<>::of(&QWidget::update));
    connect(m_state, &EditorState::playheadChanged, this, [this] { update(); });
}

void CanvasView::setGuidesVisible(bool visible)
{
    if (m_guides == visible)
        return;
    m_guides = visible;
    update();
}

QRectF CanvasView::frameRect() const
{
    const QRectF available = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    return SceneRenderer::frameRectFor(m_state->document(), available);
}

QPointF CanvasView::toScene(const QPointF &widgetPoint) const
{
    const QTransform toPixels = SceneRenderer::sceneToPixels(m_state->document(), frameRect());
    return toPixels.inverted().map(widgetPoint);
}

void CanvasView::paintEvent(QPaintEvent *)
{
    const theme::Palette &p = theme::palette();
    const Document &document = m_state->document();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // The surround is darker than the frame, so the video's edge is obvious
    // even when the scene itself is black.
    painter.fillRect(rect(), theme::mix(p.window, QColor(Qt::black), 0.45));

    const QRectF frame = frameRect();

    // A soft shadow under the frame, so it reads as a thing being looked at.
    painter.setPen(Qt::NoPen);
    for (int i = 6; i >= 1; --i) {
        QColor shade(0, 0, 0);
        shade.setAlphaF(0.16 * (1.0 - double(i) / 7.0));
        painter.setBrush(shade);
        painter.drawRect(frame.adjusted(-i, -i + 1, i, i + 2));
    }

    SceneRenderer::render(painter, document, m_state->playhead(), frame);

    painter.setPen(QPen(p.borderStrong, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(frame.adjusted(-0.5, -0.5, 0.5, 0.5));

    if (m_guides) {
        painter.save();
        painter.setClipRect(frame);
        QColor guide = p.textFaint;
        guide.setAlphaF(0.28);
        painter.setPen(QPen(guide, 1.0, Qt::DashLine));
        painter.drawLine(QPointF(frame.center().x(), frame.top()),
                         QPointF(frame.center().x(), frame.bottom()));
        painter.drawLine(QPointF(frame.left(), frame.center().y()),
                         QPointF(frame.right(), frame.center().y()));
        painter.restore();
    }

    // Nothing in the scene yet: say what to do about it.
    if (document.objects.isEmpty()) {
        QFont hint = theme::font(1);
        hint.setPixelSize(13);
        painter.setFont(hint);
        painter.setPen(p.textFaint);
        painter.drawText(frame, Qt::AlignCenter,
                         tr("Double-click a shape in the library to add it"));
    }

    // The selection, drawn over the scene.
    const ObjectId selected = m_state->selectedObject();
    if (selected != kInvalidObjectId) {
        const evaluator::ObjectState state =
            evaluator::evaluateObject(document, selected, m_state->playhead());
        const QTransform toPixels = SceneRenderer::sceneToPixels(document, frame);
        const QRectF bounds = SceneRenderer::boundsInPixels(state, toPixels);

        if (!bounds.isNull()) {
            const QRectF outline = bounds.adjusted(-4, -4, 4, 4);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(p.text, 1.2));
            painter.drawRect(outline);

            painter.setBrush(p.text);
            painter.setPen(Qt::NoPen);
            for (const QPointF &corner : {outline.topLeft(), outline.topRight(),
                                          outline.bottomLeft(), outline.bottomRight()}) {
                painter.drawRect(QRectF(corner.x() - kHandleSize / 2.0, corner.y() - kHandleSize / 2.0,
                                        kHandleSize, kHandleSize));
            }

            if (!state.visible) {
                // Selected but not on screen at this moment: say so rather than
                // leave the user wondering where it went.
                painter.setPen(p.textMuted);
                painter.drawText(outline.adjusted(0, -22, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                                 tr("not visible at this time"));
            }
        }
    }
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus();

    const QPointF scenePoint = toScene(event->position());
    const auto states = evaluator::evaluate(m_state->document(), m_state->playhead());
    const ObjectId hit = SceneRenderer::objectAt(states, scenePoint);

    if (hit == kInvalidObjectId) {
        m_state->clearSelection();
        return;
    }

    m_state->selectObject(hit);

    const evaluator::ObjectState state =
        evaluator::evaluateObject(m_state->document(), hit, m_state->playhead());
    const QPointF position = state.params.value(QStringLiteral("position")).toPointF();
    m_grabOffset = scenePoint - position;
    m_dragging = hit;
    m_dragMoved = false;
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging == kInvalidObjectId) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // The whole drag is one undo step, recorded when it first actually moves.
    if (!m_dragMoved) {
        m_state->beginEdit();
        m_dragMoved = true;
    }

    QPointF target = toScene(event->position()) - m_grabOffset;
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        // Shift snaps to a quarter unit, which lines things up with the grid.
        target.setX(std::round(target.x() * 4.0) / 4.0);
        target.setY(std::round(target.y() * 4.0) / 4.0);
    }
    m_state->moveObjectTo(m_dragging, target);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_dragging = kInvalidObjectId;
    m_dragMoved = false;
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    const ObjectId selected = m_state->selectedObject();

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        m_state->deleteSelection();
        return;
    }

    if (selected == kInvalidObjectId) {
        QWidget::keyPressEvent(event);
        return;
    }

    // Arrow keys nudge; holding shift nudges further.
    const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.5 : 0.1;
    QPointF delta;
    switch (event->key()) {
    case Qt::Key_Left:
        delta = QPointF(-step, 0);
        break;
    case Qt::Key_Right:
        delta = QPointF(step, 0);
        break;
    case Qt::Key_Up:
        delta = QPointF(0, step);
        break;
    case Qt::Key_Down:
        delta = QPointF(0, -step);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    const SceneObject *object = m_state->document().findObject(selected);
    if (!object)
        return;

    m_state->beginEdit();
    m_state->moveObjectTo(selected, object->params.value(QStringLiteral("position")).toPointF() + delta);
}

} // namespace mn::ui
