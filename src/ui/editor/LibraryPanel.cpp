#include "LibraryPanel.h"

#include "Catalog.h"
#include "EditorState.h"
#include "Theme.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

constexpr int kIdRole = Qt::UserRole + 1;
constexpr int kIsAnimationRole = Qt::UserRole + 2;

QTreeWidget *makeTree()
{
    auto *tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    tree->setIndentation(12);
    tree->setRootIsDecorated(true);
    tree->setFrameShape(QFrame::NoFrame);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setUniformRowHeights(true);
    tree->setExpandsOnDoubleClick(false);
    return tree;
}

QLabel *heading(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("role", "section");
    return label;
}

} // namespace

LibraryPanel::LibraryPanel(EditorState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    const theme::Palette &p = theme::palette();
    setStyleSheet(QStringLiteral("QWidget { background: %1; }").arg(p.surface.name()));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    layout->addSpacing(4);
    layout->addWidget(heading(tr("SHAPES")));
    m_shapes = makeTree();
    layout->addWidget(m_shapes, 3);

    layout->addSpacing(4);
    layout->addWidget(heading(tr("ANIMATIONS")));
    m_animations = makeTree();
    layout->addWidget(m_animations, 2);

    build();

    connect(m_search, &QLineEdit::textChanged, this, &LibraryPanel::applySearch);
    connect(m_shapes, &QTreeWidget::itemActivated, this, &LibraryPanel::activate);
    connect(m_shapes, &QTreeWidget::itemDoubleClicked, this, &LibraryPanel::activate);
    connect(m_animations, &QTreeWidget::itemActivated, this, &LibraryPanel::activate);
    connect(m_animations, &QTreeWidget::itemDoubleClicked, this, &LibraryPanel::activate);
    connect(m_state, &EditorState::selectionChanged, this, &LibraryPanel::refreshEnabledState);

    refreshEnabledState();
}

void LibraryPanel::build()
{
    for (const QString &category : catalog::mobjectCategories()) {
        auto *parent = new QTreeWidgetItem(m_shapes, {category});
        parent->setFlags(Qt::ItemIsEnabled);
        parent->setForeground(0, theme::palette().textFaint);
        for (const catalog::MobjectSpec &spec : catalog::mobjects()) {
            if (spec.category != category)
                continue;
            auto *item = new QTreeWidgetItem(parent, {spec.displayName});
            item->setData(0, kIdRole, spec.id);
            item->setData(0, kIsAnimationRole, false);
            item->setToolTip(0, tr("Add a %1 to the scene").arg(spec.pythonName));
        }
    }
    m_shapes->expandAll();

    for (const QString &category : catalog::animationCategories()) {
        auto *parent = new QTreeWidgetItem(m_animations, {category});
        parent->setFlags(Qt::ItemIsEnabled);
        parent->setForeground(0, theme::palette().textFaint);
        for (const catalog::AnimationSpec &spec : catalog::animations()) {
            if (spec.category != category)
                continue;
            auto *item = new QTreeWidgetItem(parent, {spec.displayName});
            item->setData(0, kIdRole, spec.id);
            item->setData(0, kIsAnimationRole, true);
            item->setToolTip(0, tr("Animate the selected object with %1").arg(spec.pythonName));
        }
    }
    m_animations->expandAll();
}

void LibraryPanel::applySearch(const QString &needle)
{
    for (QTreeWidget *tree : {m_shapes, m_animations}) {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *parent = tree->topLevelItem(i);
            int visibleChildren = 0;
            for (int j = 0; j < parent->childCount(); ++j) {
                QTreeWidgetItem *child = parent->child(j);
                const bool matches =
                    needle.isEmpty() || child->text(0).contains(needle, Qt::CaseInsensitive);
                child->setHidden(!matches);
                visibleChildren += matches ? 1 : 0;
            }
            parent->setHidden(visibleChildren == 0);
        }
        tree->expandAll();
    }
}

void LibraryPanel::refreshEnabledState()
{
    const bool hasObject = m_state->selectedObject() != kInvalidObjectId;
    m_animations->setEnabled(hasObject);
    m_animations->setToolTip(hasObject ? QString()
                                       : tr("Select an object to animate it"));
}

void LibraryPanel::activate(QTreeWidgetItem *item)
{
    if (!item)
        return;

    const QString id = item->data(0, kIdRole).toString();
    if (id.isEmpty())
        return;

    if (item->data(0, kIsAnimationRole).toBool()) {
        const ObjectId selected = m_state->selectedObject();
        if (selected != kInvalidObjectId)
            m_state->addClip(selected, id);
        return;
    }

    m_state->addObject(id);
}

} // namespace mn::ui
