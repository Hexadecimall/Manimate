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

constexpr int kRulerHeight = 26;
constexpr int kTrackHeight = 34;
constexpr int kTrackGap = 4;
constexpr int kHeaderWidth = 96;
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
    if (spec->isEntrance)
        return p.accent;
    if (spec->isExit)
        return p.danger;
    if (spec->effect == catalog::Effect::Wait)
        return p.textFaint;
    return p.violet;
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
    setMinimumHeight(kRulerHeight + kTrackHeight * kMinimumTracks + 16);

    connect(m_state, &EditorState::documentChanged, this, QOverload<>::of(&QWidget::update));
    connect(m_state, &EditorState::selectionChanged, this, QOverload<>::of(&QWidget::update));
    connect(m_state, &EditorState::playheadChanged, this, [this] { update(); });
}

QSize TimelineView::sizeHint() const
{
    const int tracks = qMax(kMinimumTracks, int(m_state->document().timeline.tracks.size()) + 1);
    return {900, kRulerHeight + tracks * (kTrackHeight + kTrackGap) + 12};
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
        painter.fillRect(lane, track % 2 == 0 ? p.surface : theme::mix(p.surface, p.window, 0.4));

        const QRectF header(0, lane.top(), kHeaderWidth, lane.height());
        painter.fillRect(header, p.window);

        QFont font = theme::font(1);
        font.setPixelSize(11);
        painter.setFont(font);
        painter.setPen(track < document.timeline.tracks.size() ? p.textMuted : p.textFaint);
        painter.drawText(header.adjusted(12, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         track < document.timeline.tracks.size()
                             ? document.timeline.tracks.at(track).name
                             : tr("Drop here"));
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

        QColor accent = clipColour(spec);
        QColor body = theme::mix(p.surface, accent, hovered || selected ? 0.34 : 0.22);

        QPainterPath shape;
        shape.addRoundedRect(box, 4, 4);
        painter.fillPath(shape, body);

        // A bar of the animation's own colour down the leading edge.
        painter.save();
        painter.setClipPath(shape);
        painter.fillRect(QRectF(box.left(), box.top(), 3, box.height()), accent);
        painter.restore();

        painter.setPen(QPen(selected ? p.text : theme::mix(accent, p.border, 0.5), selected ? 1.6 : 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(shape);

        painter.setFont(clipFont);
        painter.setPen(p.text);
        const QRectF label = box.adjusted(9, 0, -6, 0);
        const QString title = spec ? spec->displayName : clip.type;
        const QString caption = object ? QStringLiteral("%1 · %2").arg(title, object->name) : title;
        painter.drawText(label, Qt::AlignVCenter | Qt::AlignLeft,
                         QFontMetricsF(clipFont).elidedText(caption, Qt::ElideRight, label.width()));
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
    painter.fillRect(QRectF(0, kRulerHeight, kHeaderWidth, height() - kRulerHeight), p.window);
    for (int track = 0; track < trackCount; ++track) {
        const QRectF header(0, kRulerHeight + track * (kTrackHeight + kTrackGap), kHeaderWidth,
                            kTrackHeight);
        QFont font = theme::font(1);
        font.setPixelSize(11);
        painter.setFont(font);
        painter.setPen(track < document.timeline.tracks.size() ? p.textMuted : p.textFaint);
        painter.drawText(header.adjusted(12, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         track < document.timeline.tracks.size()
                             ? document.timeline.tracks.at(track).name
                             : tr("New track"));
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
