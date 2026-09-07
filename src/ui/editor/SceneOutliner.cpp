#include "SceneOutliner.h"

#include "Catalog.h"
#include "Document.h"
#include "EditorState.h"
#include "LibraryPanel.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

constexpr int kObjectIdRole = Qt::UserRole + 1;

} // namespace

SceneOutliner::SceneOutliner(EditorState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    const theme::Palette &p = theme::palette();
    setAutoFillBackground(true);
    QPalette background = palette();
    background.setColor(QPalette::Window, p.surface);
    setPalette(background);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_list = new QListWidget;
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setIconSize(QSize(18, 18));
    // Several rows at once, so they can be grouped together.
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setUniformItemSizes(true);
    m_list->installEventFilter(this);

    // As in the library: the stylesheet draws the row, so the style must not
    // draw anything of its own beside it.
    QPalette rowPalette = m_list->palette();
    rowPalette.setColor(QPalette::Highlight, Qt::transparent);
    rowPalette.setColor(QPalette::Inactive, QPalette::Highlight, Qt::transparent);
    m_list->setPalette(rowPalette);
    layout->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        if (m_updating)
            return;
        const QListWidgetItem *item = m_list->currentItem();
        m_state->selectObject(item ? ObjectId(item->data(kObjectIdRole).toULongLong())
                                   : kInvalidObjectId);
    });
    connect(m_list, &QListWidget::itemChanged, this, &SceneOutliner::itemChanged);
    connect(m_list, &QListWidget::customContextMenuRequested, this,
            &SceneOutliner::showContextMenu);

    connect(m_state, &EditorState::documentChanged, this, &SceneOutliner::rebuild);
    connect(m_state, &EditorState::selectionChanged, this, &SceneOutliner::syncSelectionFromState);

    rebuild();
}

void SceneOutliner::rebuild()
{
    m_updating = true;
    m_list->clear();

    // Topmost first, matching how the canvas stacks them.
    QVector<const SceneObject *> objects;
    for (const SceneObject &object : m_state->document().objects)
        objects.append(&object);
    std::sort(objects.begin(), objects.end(), [](const SceneObject *a, const SceneObject *b) {
        return a->zOrder > b->zOrder;
    });

    for (const SceneObject *object : std::as_const(objects)) {
        // A child is indented under the group that holds it.
        QString label = object->name;
        int depth = 0;
        ObjectId ancestor = object->parentId;
        while (ancestor != kInvalidObjectId && depth < 8) {
            const SceneObject *parent = m_state->document().findObject(ancestor);
            if (!parent)
                break;
            ++depth;
            ancestor = parent->parentId;
        }
        if (depth > 0)
            label.prepend(QString(depth * 4, QLatin1Char(' ')));

        auto *item = new QListWidgetItem(label, m_list);
        item->setData(kObjectIdRole, qulonglong(object->id));
        item->setIcon(LibraryPanel::shapeIcon(object->type));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(object->visible ? Qt::Checked : Qt::Unchecked);

        if (const catalog::MobjectSpec *spec = catalog::findMobject(object->type))
            item->setToolTip(spec->pythonName);
    }

    m_updating = false;
    syncSelectionFromState();
}

void SceneOutliner::syncSelectionFromState()
{
    m_updating = true;
    const ObjectId selected = m_state->selectedObject();

    m_list->clearSelection();
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (ObjectId(item->data(kObjectIdRole).toULongLong()) == selected) {
            m_list->setCurrentItem(item);
            break;
        }
    }
    m_updating = false;
}

void SceneOutliner::itemChanged(QListWidgetItem *item)
{
    if (m_updating || !item)
        return;

    const ObjectId id = ObjectId(item->data(kObjectIdRole).toULongLong());
    SceneObject *object = m_state->documentForWriting().findObject(id);
    if (!object)
        return;

    const bool visible = item->checkState() == Qt::Checked;
    const QString typed = item->text().trimmed();
    if (object->visible == visible && object->name == typed)
        return;

    m_state->beginEdit();
    object->visible = visible;
    if (!typed.isEmpty())
        object->name = typed;
    m_state->setModified(true);
    Q_EMIT m_state->documentChanged();
}

bool SceneOutliner::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) {
            keyPressEvent(key);
            if (key->isAccepted())
                return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SceneOutliner::keyPressEvent(QKeyEvent *event)
{
    // Backspace as well as Delete: on a Mac keyboard backspace is the delete
    // key, and expecting the forward-delete key is expecting a full-size one.
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        const QListWidgetItem *item = m_list->currentItem();
        if (item) {
            m_state->removeObject(ObjectId(item->data(kObjectIdRole).toULongLong()));
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

QVector<ObjectId> SceneOutliner::selectedObjects() const
{
    QVector<ObjectId> ids;
    for (const QListWidgetItem *item : m_list->selectedItems())
        ids.append(ObjectId(item->data(kObjectIdRole).toULongLong()));
    return ids;
}

void SceneOutliner::showContextMenu(const QPoint &position)
{
    QListWidgetItem *item = m_list->itemAt(position);
    if (!item)
        return;

    const ObjectId id = ObjectId(item->data(kObjectIdRole).toULongLong());
    const QVector<ObjectId> selection = selectedObjects();
    const SceneObject *object = m_state->document().findObject(id);

    QMenu menu(this);
    QAction *rename = menu.addAction(tr("Rename"));
    QAction *hide = menu.addAction(item->checkState() == Qt::Checked ? tr("Hide") : tr("Show"));
    menu.addSeparator();

    QAction *group = menu.addAction(selection.size() > 1
                                        ? tr("Group %1 objects").arg(selection.size())
                                        : tr("Group"));
    group->setEnabled(selection.size() > 1);

    QAction *ungroup = menu.addAction(tr("Ungroup"));
    ungroup->setEnabled(object && object->parentId != kInvalidObjectId);

    menu.addSeparator();
    QAction *remove = menu.addAction(tr("Delete"));

    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(position));
    if (chosen == rename) {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_list->editItem(item);
    } else if (chosen == hide) {
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    } else if (chosen == group) {
        m_state->groupObjects(selection);
    } else if (chosen == ungroup) {
        m_state->ungroupObject(id);
    } else if (chosen == remove) {
        for (const ObjectId selected : selection)
            m_state->removeObject(selected);
    }
}

} // namespace mn::ui
