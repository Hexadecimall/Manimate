#include "TimelineView.h"

#include "Catalog.h"
#include "Document.h"
#include "EditorState.h"
#include "Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>

namespace mn::ui {
namespace {

constexpr int kRulerHeight = 28;
constexpr int kTrackHeight = 42;
constexpr int kTrackGap = 5;
constexpr int kHeaderWidth = 116;
constexpr int kLeftPadding = 8;
constexpr double kTrimHandle = 6.0;
constexpr int kMinimumTracks = 3;

/// Clips are tinted by what the animation does, so the timeline is readable
/// at a glance: entrances, motion and exits look different.
QColor clipColour(const catalog::AnimationSpec *spec)
{
    const theme::Palette &p = theme::palette();
    if (!spec)
        return p.textFaint;
    // Desaturated against the accent, so a timeline full of clips does not
    // become the loudest thing on the screen.
    if (spec->isEntrance)
        return theme::mix(p.accent, p.textMuted, 0.42);
    if (spec->isExit)
        return theme::mix(p.danger, p.textMuted, 0.38);
    if (spec->effect == catalog::Effect::Wait)
        return p.textFaint;
    return theme::mix(p.violet, p.textMuted, 0.32);
}

QString formatTime(double seconds)
{
    const int whole = int(seconds);
    const int minutes = whole / 60;
    const int rest = whole % 60;
    if (minutes > 0)
        return QStringLiteral("%1:%2").arg(minutes).arg(rest, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1s").arg(seconds, 0, 'g', 3);
}

} // namespace

TimelineView::TimelineView(EditorState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    connect(m_state, &EditorState::documentChanged, this, [this] {
        // A new track changes how tall the view wants to be.
        updateGeometry();
        update();
    });
    connect(m_state, &EditorState::selectionChanged, this, QOverload<>::of(&QWidget::update));
    connect(m_state, &EditorState::playheadChanged, this, [this] { update(); });
}

QSize TimelineView::sizeHint() const
{
    return minimumSizeHint();
}

QSize TimelineView::minimumSizeHint() const
{
    // Tall enough for every track plus the empty one that accepts a drop, so
    // the scroll area around it knows when there is more than fits.
    const int tracks = qMax(kMinimumTracks, int(m_state->document().timeline.tracks.size()) + 1);
    return {600, kRulerHeight + tracks * (kTrackHeight + kTrackGap) + 10};
}

void TimelineView::setScale(double pixelsPerSecond)
{
    const double clamped = std::clamp(pixelsPerSecond, 12.0, 800.0);
    if (qFuzzyCompare(m_scale, clamped))
        return;
    m_scale = clamped;
    update();
    Q_EMIT zoomChanged(m_scale);
}

double TimelineView::timeAt(double x) const
{
    return qMax(0.0, (x - kHeaderWidth - kLeftPadding) / m_scale);
}

double TimelineView::xAt(double time) const
{
    return kHeaderWidth + kLeftPadding + time * m_scale;
}

int TimelineView::trackAt(double y) const
{
    const double local = y - kRulerHeight;
    if (local < 0)
        return -1;
    return int(local) / (kTrackHeight + kTrackGap);
}

QRectF TimelineView::clipRect(const Clip &clip) const
{
    const double top = kRulerHeight + clip.track * (kTrackHeight + kTrackGap) + 2;
    return QRectF(xAt(clip.start), top, qMax(6.0, clip.duration * m_scale), kTrackHeight - 4);
}

double TimelineView::rulerStep() const
{
    // Pick the coarsest step that still leaves labels at least this far apart.
    constexpr double kMinimumLabelSpacing = 56.0;
    for (const double candidate : {0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0}) {
        if (candidate * m_scale >= kMinimumLabelSpacing)
            return candidate;
    }
    return 120.0;
}

void TimelineView::paintEvent(QPaintEvent *)
{
    const theme::Palette &p = theme::palette();
    const Document &document = m_state->document();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), p.window);

    const double duration = m_state->timelineDuration();
    const int trackCount = qMax(kMinimumTracks, int(document.timeline.tracks.size()) + 1);

    // ------------------------------------------------------------- lanes ----
    for (int track = 0; track < trackCount; ++track) {
        const QRectF lane(kHeaderWidth, kRulerHeight + track * (kTrackHeight + kTrackGap),
                          width() - kHeaderWidth, kTrackHeight);
        painter.fillRect(lane, track % 2 == 0 ? theme::mix(p.window, p.surface, 0.55)
                                              : theme::mix(p.window, p.surface, 0.30));

    }

    // ------------------------------------------------------------ ruler ----
    const QRectF ruler(0, 0, width(), kRulerHeight);
    painter.fillRect(ruler, p.surfaceRaised);
    painter.setPen(QPen(p.border, 1.0));
    painter.drawLine(QPointF(0, kRulerHeight - 0.5), QPointF(width(), kRulerHeight - 0.5));

    QFont rulerFont = theme::font(1);
    rulerFont.setPixelSize(10);
    painter.setFont(rulerFont);

    const double step = rulerStep();
    for (double t = 0.0; xAt(t) < width(); t += step) {
        const double x = xAt(t);
        if (x < kHeaderWidth)
            continue;
        painter.setPen(QPen(p.border, 1.0));
        painter.drawLine(QPointF(x, kRulerHeight - 7), QPointF(x, kRulerHeight - 1));
        painter.setPen(p.textFaint);
        painter.drawText(QRectF(x + 4, 2, 60, kRulerHeight - 8), Qt::AlignLeft | Qt::AlignVCenter,
                         formatTime(t));

        // A faint line down the lanes, so timing can be read vertically.
        painter.setPen(QPen(theme::mix(p.window, p.border, 0.6), 1.0));
        painter.drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
    }

    // The end of the video, past which nothing is rendered.
    const double endX = xAt(duration);
    if (endX < width()) {
        painter.fillRect(QRectF(endX, kRulerHeight, width() - endX, height() - kRulerHeight),
                         QColor(0, 0, 0, 90));
        painter.setPen(QPen(p.borderStrong, 1.0));
        painter.drawLine(QPointF(endX, kRulerHeight), QPointF(endX, height()));
    }

    // ------------------------------------------------------------ clips ----
    QFont clipFont = theme::font(1, QFont::DemiBold);
    clipFont.setPixelSize(11);

    for (const Clip &clip : document.timeline.clips) {
        const QRectF box = clipRect(clip);
        if (box.right() < kHeaderWidth || box.left() > width())
            continue;

        const catalog::AnimationSpec *spec = catalog::findAnimation(clip.type);
        const SceneObject *object = document.findObject(clip.objectId);
        const bool selected = clip.id == m_state->selectedClip();
        const bool hovered = clip.id == m_hovered;

        const QColor accent = clipColour(spec);
        const QColor body = theme::mix(p.surfaceRaised, accent, hovered || selected ? 0.36 : 0.20);

        QPainterPath shape;
        shape.addRoundedRect(box, 5, 5);
        painter.fillPath(shape, body);

        // A bar of the animation's own colour down the leading edge.
        painter.save();
        painter.setClipPath(shape);
        painter.fillRect(QRectF(box.left(), box.top(), 3, box.height()), accent);

        // Trim handles, shown only while the pointer is on the clip.
        if (hovered) {
            const QColor handle = theme::mix(body, p.text, 0.30);
            painter.fillRect(QRectF(box.left() + 3, box.top(), 3, box.height()), handle);
            painter.fillRect(QRectF(box.right() - 4, box.top(), 3, box.height()), handle);
        }
        painter.restore();

        painter.setPen(QPen(selected ? p.text : theme::mix(accent, p.border, 0.55), selected ? 1.6 : 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(shape);

        const QRectF label = box.adjusted(10, 3, -7, -3);
        const QString title = spec ? spec->displayName : clip.type;

        painter.setFont(clipFont);
        painter.setPen(p.text);
        painter.drawText(QRectF(label.left(), label.top(), label.width(), label.height() / 2.0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QFontMetricsF(clipFont).elidedText(title, Qt::ElideRight, label.width()));

        if (object && label.height() > 22) {
            QFont subFont = theme::font(1);
            subFont.setPixelSize(10);
            painter.setFont(subFont);
            painter.setPen(p.textFaint);
            painter.drawText(
                QRectF(label.left(), label.center().y(), label.width(), label.height() / 2.0),
                Qt::AlignVCenter | Qt::AlignLeft,
                QFontMetricsF(subFont).elidedText(object->name, Qt::ElideRight, label.width()));
        }
    }

    // --------------------------------------------------------- playhead ----
    const double playX = xAt(m_state->playhead());
    if (playX >= kHeaderWidth) {
        painter.setPen(QPen(p.danger, 1.4));
        painter.drawLine(QPointF(playX, 0), QPointF(playX, height()));

        QPainterPath head;
        head.moveTo(playX - 6, 0);
        head.lineTo(playX + 6, 0);
        head.lineTo(playX, 9);
        head.closeSubpath();
        painter.fillPath(head, p.danger);
    }

    // Mask anything drawn over the track headers.
    painter.fillRect(QRectF(0, kRulerHeight, kHeaderWidth, height() - kRulerHeight), p.surface);
    for (int track = 0; track < trackCount; ++track) {
        const QRectF header(0, kRulerHeight + track * (kTrackHeight + kTrackGap), kHeaderWidth,
                            kTrackHeight);
        const bool real = track < document.timeline.tracks.size();

        int clipCount = 0;
        for (const Clip &clip : document.timeline.clips)
            clipCount += clip.track == track ? 1 : 0;

        QFont font = theme::font(1, QFont::DemiBold);
        font.setPixelSize(11);
        painter.setFont(font);
        painter.setPen(real ? p.text : p.textFaint);
        painter.drawText(header.adjusted(14, 4, -8, -header.height() / 2.0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         real ? document.timeline.tracks.at(track).name : tr("New track"));

        if (real) {
            QFont sub = theme::font(1);
            sub.setPixelSize(10);
            painter.setFont(sub);
            painter.setPen(p.textFaint);
            painter.drawText(header.adjusted(14, header.height() / 2.0, -8, -4),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             clipCount == 1 ? tr("1 clip") : tr("%1 clips").arg(clipCount));
        }
    }
    painter.setPen(QPen(p.border, 1.0));
    painter.drawLine(QPointF(kHeaderWidth - 0.5, 0), QPointF(kHeaderWidth - 0.5, height()));
}

ClipId TimelineView::clipAt(const QPointF &point, Grab *how) const
{
    for (const Clip &clip : m_state->document().timeline.clips) {
        const QRectF box = clipRect(clip);
        if (!box.contains(point))
            continue;

        if (how) {
            if (point.x() - box.left() <= kTrimHandle)
                *how = Grab::TrimStart;
            else if (box.right() - point.x() <= kTrimHandle)
                *how = Grab::TrimEnd;
            else
                *how = Grab::Move;
        }
        return clip.id;
    }
    if (how)
        *how = Grab::None;
    return kInvalidClipId;
}

void TimelineView::updateCursorFor(const QPointF &point)
{
    Grab how = Grab::None;
    const ClipId id = clipAt(point, &how);

    if (m_hovered != id) {
        m_hovered = id;
        update();
    }

    switch (how) {
    case Grab::TrimStart:
    case Grab::TrimEnd:
        setCursor(Qt::SizeHorCursor);
        break;
    case Grab::Move:
        setCursor(Qt::OpenHandCursor);
        break;
    default:
        setCursor(point.y() < kRulerHeight ? Qt::PointingHandCursor : Qt::ArrowCursor);
        break;
    }
}

void TimelineView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus();
    const QPointF point = event->position();

    // The ruler scrubs.
    if (point.y() < kRulerHeight) {
        m_grab = Grab::Scrub;
        m_state->setPlayhead(timeAt(point.x()));
        return;
    }

    Grab how = Grab::None;
    const ClipId id = clipAt(point, &how);
    if (id == kInvalidClipId) {
        m_state->selectClip(kInvalidClipId);
        m_state->setPlayhead(timeAt(point.x()));
        return;
    }

    const Clip *clip = m_state->document().findClip(id);
    if (!clip)
        return;

    m_state->selectClip(id);
    m_grab = how;
    m_grabbed = id;
    m_grabStart = clip->start;
    m_grabDuration = clip->duration;
    m_grabTimeOffset = timeAt(point.x()) - clip->start;
    m_grabMoved = false;
}

void TimelineView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF point = event->position();

    if (m_grab == Grab::None) {
        updateCursorFor(point);
        return;
    }

    if (m_grab == Grab::Scrub) {
        m_state->setPlayhead(timeAt(point.x()));
        return;
    }

    const Clip *clip = m_state->document().findClip(m_grabbed);
    if (!clip)
        return;

    // One undo step per drag, recorded the first time it actually changes.
    if (!m_grabMoved) {
        m_state->beginEdit();
        m_grabMoved = true;
    }

    const double time = timeAt(point.x());
    const bool snap = !event->modifiers().testFlag(Qt::AltModifier);
    auto maybeSnap = [snap](double value) {
        // Snap to tenths, which is fine enough to feel free and coarse enough
        // to line clips up exactly. Alt turns it off.
        return snap ? std::round(value * 10.0) / 10.0 : value;
    };

    switch (m_grab) {
    case Grab::Move: {
        const int track = qMax(0, trackAt(point.y()));
        m_state->setClipTiming(m_grabbed, maybeSnap(time - m_grabTimeOffset), clip->duration, track);
        setCursor(Qt::ClosedHandCursor);
        break;
    }
    case Grab::TrimStart: {
        const double end = m_grabStart + m_grabDuration;
        const double start = qMin(maybeSnap(time), end - 0.05);
        m_state->setClipTiming(m_grabbed, start, end - start, clip->track);
        break;
    }
    case Grab::TrimEnd: {
        const double end = qMax(maybeSnap(time), clip->start + 0.05);
        m_state->setClipTiming(m_grabbed, clip->start, end - clip->start, clip->track);
        break;
    }
    default:
        break;
    }
}

void TimelineView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_grab = Grab::None;
    m_grabbed = kInvalidClipId;
    m_grabMoved = false;
    unsetCursor();
}

void TimelineView::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (m_hovered != kInvalidClipId) {
        m_hovered = kInvalidClipId;
        update();
    }
}

void TimelineView::wheelEvent(QWheelEvent *event)
{
    if (!event->modifiers().testFlag(Qt::ControlModifier)
        && !event->modifiers().testFlag(Qt::MetaModifier)) {
        QWidget::wheelEvent(event);
        return;
    }

    // Zoom about the pointer, so the moment under it stays put.
    const double anchorTime = timeAt(event->position().x());
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    setScale(m_scale * factor);

    const double drift = xAt(anchorTime) - event->position().x();
    if (qAbs(drift) > 0.5)
        update();

    event->accept();
}

void TimelineView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        m_state->deleteSelection();
        return;
    case Qt::Key_Left:
        m_state->setPlayhead(m_state->playhead() - (event->modifiers() & Qt::ShiftModifier ? 1.0 : 0.1));
        return;
    case Qt::Key_Right:
        m_state->setPlayhead(m_state->playhead() + (event->modifiers() & Qt::ShiftModifier ? 1.0 : 0.1));
        return;
    case Qt::Key_Home:
        m_state->setPlayhead(0.0);
        return;
    case Qt::Key_End:
        m_state->setPlayhead(m_state->timelineDuration());
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

} // namespace mn::ui
