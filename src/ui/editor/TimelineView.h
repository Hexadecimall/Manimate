#pragma once

#include "Timeline.h"
#include "Types.h"

#include <QWidget>

namespace mn::ui {

class EditorState;

/// The timeline: a ruler, one lane per track, and a clip for every animation.
///
/// Clips sit at absolute times and may overlap freely, because that is how a
/// person thinks about timing. Manim has no such clock, so the exporter's
/// solver turns whatever is arranged here back into ordered play calls.
class TimelineView : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineView(EditorState *state, QWidget *parent = nullptr);

    /// Pixels per second. Changed by zooming.
    double scale() const { return m_scale; }
    void setScale(double pixelsPerSecond);

    /// How far the view is scrolled along, in pixels. Never negative: there is
    /// nothing before zero.
    double offset() const { return m_offset; }
    void setOffset(double pixels);

    /// Scroll so the playhead is on screen, if it is not already.
    void revealPlayhead();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void zoomChanged(double pixelsPerSecond);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    /// What the pointer is doing to a clip.
    enum class Grab { None, Move, TrimStart, TrimEnd, Scrub };

    double timeAt(double x) const;
    double xAt(double time) const;
    int trackAt(double y) const;
    QRectF clipRect(const Clip &clip) const;

    /// The clip under `point`, and which part of it.
    ClipId clipAt(const QPointF &point, Grab *how) const;

    /// Spacing of ruler labels that keeps them readable at the current zoom.
    double rulerStep() const;

    void updateCursorFor(const QPointF &point);

    EditorState *m_state;
    double m_scale = 90.0;
    double m_offset = 0.0;

    Grab m_grab = Grab::None;
    ClipId m_grabbed = kInvalidClipId;
    double m_grabTimeOffset = 0.0;
    double m_grabStart = 0.0;
    double m_grabDuration = 0.0;
    bool m_grabMoved = false;

    ClipId m_hovered = kInvalidClipId;
};

} // namespace mn::ui
