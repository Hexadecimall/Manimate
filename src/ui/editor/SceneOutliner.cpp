#include "SceneOutliner.h"

#include "Catalog.h"
#include "Document.h"
#include "EditorState.h"
#include "LibraryPanel.h"
#include "Theme.h"

#include <QHBoxLayout>
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
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setUniformItemSizes(true);
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
        auto *item = new QListWidgetItem(object->name, m_list);
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
    if (object->visible == visible && object->name == item->text())
        return;

    m_state->beginEdit();
    object->visible = visible;
    if (!item->text().isEmpty())
        object->name = item->text();
    m_state->setModified(true);
    Q_EMIT m_state->documentChanged();
}

void SceneOutliner::showContextMenu(const QPoint &position)
{
    QListWidgetItem *item = m_list->itemAt(position);
    if (!item)
        return;

    const ObjectId id = ObjectId(item->data(kObjectIdRole).toULongLong());

    QMenu menu(this);
    QAction *rename = menu.addAction(tr("Rename"));
    QAction *hide = menu.addAction(item->checkState() == Qt::Checked ? tr("Hide") : tr("Show"));
    menu.addSeparator();
    QAction *remove = menu.addAction(tr("Delete"));

    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(position));
    if (chosen == rename) {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_list->editItem(item);
    } else if (chosen == hide) {
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    } else if (chosen == remove) {
        m_state->removeObject(id);
    }
}

} // namespace mn::ui
