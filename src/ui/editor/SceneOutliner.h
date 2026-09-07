#pragma once

#include "Types.h"

#include <QWidget>

class QListWidget;
class QListWidgetItem;

namespace mn::ui {

class EditorState;

/// Everything in the scene, as a list.
///
/// The canvas can only offer what is visible at the playhead and big enough to
/// click; this is how an object that has not entered yet, or is hidden behind
/// another, can still be found and worked on.
class SceneOutliner : public QWidget
{
    Q_OBJECT

public:
    explicit SceneOutliner(EditorState *state, QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    void syncSelectionFromState();
    void itemChanged(QListWidgetItem *item);
    void showContextMenu(const QPoint &position);

    EditorState *m_state;
    QListWidget *m_list = nullptr;

    /// True while the list is being rebuilt, so its own signals are ignored.
    bool m_updating = false;
};

} // namespace mn::ui
