#pragma once

#include "SceneEvaluator.h"

#include <QPointF>
#include <QVariantMap>
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
    /// Which corner of the selection is being dragged, if any.
    enum class Handle { None, TopLeft, TopRight, BottomLeft, BottomRight };

    QRectF frameRect() const;
    QPointF toScene(const QPointF &widgetPoint) const;

    /// The selection's outline in widget coordinates, or a null rect.
    QRectF selectionOutline() const;

    /// The handle under `widgetPoint`, if the pointer is close enough to one.
    Handle handleAt(const QPointF &widgetPoint) const;

    /// Resize the selected object so its half-extent matches `scenePoint`,
    /// measured from the object's own centre.
    void resizeTo(const QPointF &scenePoint);

    static Qt::CursorShape cursorFor(Handle handle);

    EditorState *m_state;
    bool m_guides = true;

    /// Drag in progress: which object, and where it was grabbed relative to
    /// its own position, so it does not jump under the pointer.
    ObjectId m_dragging = kInvalidObjectId;
    QPointF m_grabOffset;
    bool m_dragMoved = false;

    /// Resize in progress, and the object's size when it started.
    Handle m_resizing = Handle::None;
    QPointF m_resizeStartExtent;
    QVariantMap m_resizeStartParams;
};

} // namespace mn::ui
