#pragma once

#include <QIcon>
#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace mn::ui {

class EditorState;

/// The catalog, as somewhere to drag things from.
///
/// Shapes add themselves to the scene. Animations attach to whatever is
/// selected, at the playhead — which is why the panel dims them when nothing
/// is selected rather than letting a click quietly do nothing.
class LibraryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryPanel(EditorState *state, QWidget *parent = nullptr);

    /// A small preview of a catalog shape, drawn with the scene renderer so the
    /// icon and the canvas can never disagree about what a shape looks like.
    /// Shared, so the scene list shows the same picture as the library.
    static QIcon shapeIcon(const QString &specId);

    /// A glyph standing for what an animation does.
    static QIcon animationIcon(const QString &animationId);

private:
    void build();
    void applySearch(const QString &needle);
    void activate(QTreeWidgetItem *item);
    void refreshEnabledState();

    EditorState *m_state;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_shapes = nullptr;
    QTreeWidget *m_animations = nullptr;
};

} // namespace mn::ui
