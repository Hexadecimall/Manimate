#include "CanvasView.h"

#include "EditorState.h"
#include "Catalog.h"
#include "Document.h"
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

    // In three dimensions, show which way the camera is facing.
    if (document.camera.enabled) {
        const Camera3D &camera = document.camera;
        const QPointF centre(frame.left() + 46, frame.bottom() - 46);
        constexpr double kArm = 26.0;

        const struct {
            double x, y, z;
            QColor colour;
            QString label;
        } axes[] = {
            {1, 0, 0, p.manimRed, QStringLiteral("x")},
            {0, 1, 0, p.manimGreen, QStringLiteral("y")},
            {0, 0, 1, p.manimBlue, QStringLiteral("z")},
        };

        QFont gizmoFont = theme::font(1, QFont::DemiBold);
        gizmoFont.setPixelSize(10);
        painter.setFont(gizmoFont);

        for (const auto &axis : axes) {
            const QPointF projected = SceneRenderer::project(camera, axis.x, axis.y, axis.z);
            // The projection is in scene units; y still points up on screen.
            const QPointF tip = centre + QPointF(projected.x(), -projected.y()) * kArm;

            painter.setPen(QPen(axis.colour, 1.6));
            painter.drawLine(centre, tip);
            painter.drawText(QRectF(tip.x() - 6, tip.y() - 8, 12, 16), Qt::AlignCenter, axis.label);
        }

        painter.setPen(p.textFaint);
        painter.drawText(QRectF(frame.left() + 12, frame.bottom() - 22, 200, 16),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         tr("3D  ·  tilt %1°  turn %2°  ·  drag to orbit")
                             .arg(qRound(camera.phi))
                             .arg(qRound(camera.theta)));
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
        const QRectF bounds = SceneRenderer::boundsInPixels(state, toPixels,
                                                            m_state->document().camera);

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

QRectF CanvasView::selectionOutline() const
{
    const ObjectId selected = m_state->selectedObject();
    if (selected == kInvalidObjectId)
        return {};

    const evaluator::ObjectState state =
        evaluator::evaluateObject(m_state->document(), selected, m_state->playhead());
    const QTransform toPixels = SceneRenderer::sceneToPixels(m_state->document(), frameRect());
    const QRectF bounds =
        SceneRenderer::boundsInPixels(state, toPixels, m_state->document().camera);
    if (bounds.isNull())
        return {};
    return bounds.adjusted(-4, -4, 4, 4);
}

CanvasView::Handle CanvasView::handleAt(const QPointF &widgetPoint) const
{
    const QRectF outline = selectionOutline();
    if (outline.isNull())
        return Handle::None;

    // A little larger than the handle is drawn, so it can actually be grabbed.
    constexpr double kGrab = 9.0;
    const struct {
        Handle handle;
        QPointF corner;
    } corners[] = {
        {Handle::TopLeft, outline.topLeft()},
        {Handle::TopRight, outline.topRight()},
        {Handle::BottomLeft, outline.bottomLeft()},
        {Handle::BottomRight, outline.bottomRight()},
    };

    for (const auto &entry : corners) {
        if (QLineF(widgetPoint, entry.corner).length() <= kGrab)
            return entry.handle;
    }
    return Handle::None;
}

Qt::CursorShape CanvasView::cursorFor(Handle handle)
{
    switch (handle) {
    case Handle::TopLeft:
    case Handle::BottomRight:
        return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:
        return Qt::SizeBDiagCursor;
    default:
        return Qt::ArrowCursor;
    }
}

void CanvasView::resizeTo(const QPointF &scenePoint)
{
    const ObjectId selected = m_state->selectedObject();
    const SceneObject *object = m_state->document().findObject(selected);
    if (!object)
        return;

    const catalog::MobjectSpec *spec = catalog::findMobject(object->type);
    if (!spec)
        return;

    // How far the pointer is from the object's centre, in scene units. The
    // object is sized so its own half-extent matches that.
    const QPointF centre = object->params.value(QStringLiteral("position")).toPointF();
    const double halfWidth = qMax(0.02, qAbs(scenePoint.x() - centre.x()));
    const double halfHeight = qMax(0.02, qAbs(scenePoint.y() - centre.y()));

    auto has = [spec](const QString &id) {
        for (const catalog::ParamSpec &param : spec->params) {
            if (param.id == id)
                return true;
        }
        return false;
    };
    auto startValue = [this](const QString &id, double fallback) {
        const QVariant value = m_resizeStartParams.value(id);
        return value.isValid() ? value.toDouble() : fallback;
    };

    // Each shape is sized through whatever parameter actually controls it, so
    // resizing edits the shape rather than piling a scale factor on top of it.
    if (has(QStringLiteral("width")) && has(QStringLiteral("height"))) {
        m_state->setObjectParam(selected, QStringLiteral("width"), halfWidth * 2.0);
        m_state->setObjectParam(selected, QStringLiteral("height"), halfHeight * 2.0);
        return;
    }

    const double extent = qMax(halfWidth, halfHeight);

    if (has(QStringLiteral("side_length"))) {
        m_state->setObjectParam(selected, QStringLiteral("side_length"), extent * 2.0);
        return;
    }
    if (has(QStringLiteral("radius"))) {
        m_state->setObjectParam(selected, QStringLiteral("radius"), extent);
        return;
    }
    if (has(QStringLiteral("outer_radius"))) {
        // Keep the inner radius in proportion, or a star inverts itself.
        const double startOuter = qMax(0.001, startValue(QStringLiteral("outer_radius"), 1.0));
        const double ratio = extent / startOuter;
        m_state->setObjectParam(selected, QStringLiteral("outer_radius"), extent);
        if (has(QStringLiteral("inner_radius"))) {
            m_state->setObjectParam(selected, QStringLiteral("inner_radius"),
                                    startValue(QStringLiteral("inner_radius"), 0.5) * ratio);
        }
        return;
    }
    if (has(QStringLiteral("length"))) {
        m_state->setObjectParam(selected, QStringLiteral("length"), extent * 2.0);
        return;
    }

    // Anything with no size of its own — text, a formula, a group — scales.
    const double startExtent = qMax(0.02, qMax(qAbs(m_resizeStartExtent.x()),
                                               qAbs(m_resizeStartExtent.y())));
    const double startScale = startValue(QStringLiteral("scale"), 1.0);
    m_state->setObjectParam(selected, QStringLiteral("scale"),
                            qBound(0.01, startScale * extent / startExtent, 100.0));
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus();

    // A corner handle resizes; anywhere else selects or drags.
    if (const Handle handle = handleAt(event->position()); handle != Handle::None) {
        const evaluator::ObjectState state = evaluator::evaluateObject(
            m_state->document(), m_state->selectedObject(), m_state->playhead());
        const QPainterPath shape = SceneRenderer::shapeOf(state, m_state->document().camera);
        const QRectF bounds = shape.boundingRect();

        m_resizing = handle;
        m_resizeStartExtent = QPointF(bounds.width() / 2.0, bounds.height() / 2.0);
        m_resizeStartParams = state.params;
        m_dragMoved = false;
        return;
    }

    const QPointF scenePoint = toScene(event->position());
    const auto states = evaluator::evaluate(m_state->document(), m_state->playhead());
    const ObjectId hit = SceneRenderer::objectAt(states, scenePoint, m_state->document().camera);

    if (hit == kInvalidObjectId) {
        m_state->clearSelection();

        // In three dimensions the empty canvas is a viewport: dragging it turns
        // the camera round the scene rather than doing nothing.
        if (m_state->document().camera.enabled) {
            m_orbiting = true;
            m_orbitFrom = event->position();
            m_state->beginEdit();
            setCursor(Qt::ClosedHandCursor);
        }
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
    if (m_orbiting) {
        const QPointF delta = event->position() - m_orbitFrom;
        m_orbitFrom = event->position();

        Camera3D camera = m_state->document().camera;
        camera.theta -= delta.x() * 0.4;
        // Past straight up or straight down the view turns inside out, so the
        // tilt stops there.
        camera.phi = qBound(0.0, camera.phi - delta.y() * 0.4, 180.0);

        // Not recorded: the whole drag is one step, opened when it began.
        m_state->setCamera(camera, false);
        return;
    }

    if (m_resizing != Handle::None) {
        if (!m_dragMoved) {
            m_state->beginEdit();
            m_dragMoved = true;
        }
        resizeTo(toScene(event->position()));
        return;
    }

    if (m_dragging == kInvalidObjectId) {
        // Show what a corner would do before it is grabbed.
        setCursor(cursorFor(handleAt(event->position())));
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
    if (m_orbiting) {
        m_orbiting = false;
        unsetCursor();
    }
    m_dragging = kInvalidObjectId;
    m_resizing = Handle::None;
    m_resizeStartParams.clear();
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
