#pragma once

#include "SceneEvaluator.h"

#include <QPointF>
#include <QWidget>

namespace mn::ui {

class EditorState;

/// The viewer: the frame as it stands at the playhead, with the selected
/// object outlined and draggable.
///
/// The frame is letterboxed inside the widget at the project's aspect ratio, so
/// what is shown is exactly what will be rendered — nothing is visible here
/// that would fall outside the video.
class CanvasView : public QWidget
{
    Q_OBJECT

public:
    explicit CanvasView(EditorState *state, QWidget *parent = nullptr);

    /// Show the safe-area guides and the centre lines.
    void setGuidesVisible(bool visible);
    bool guidesVisible() const { return m_guides; }

    QSize sizeHint() const override { return {960, 540}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRectF frameRect() const;
    QPointF toScene(const QPointF &widgetPoint) const;

    EditorState *m_state;
    bool m_guides = true;

    /// Drag in progress: which object, and where it was grabbed relative to
    /// its own position, so it does not jump under the pointer.
    ObjectId m_dragging = kInvalidObjectId;
    QPointF m_grabOffset;
    bool m_dragMoved = false;
};

} // namespace mn::ui
