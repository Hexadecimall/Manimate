#include "LibraryPanel.h"

#include "Catalog.h"
#include "EditorState.h"
#include "SceneEvaluator.h"
#include "SceneRenderer.h"
#include "SceneOutliner.h"
#include "SegmentedTabs.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <QHBoxLayout>
#include <QShortcut>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

constexpr int kIdRole = Qt::UserRole + 1;
constexpr int kIsAnimationRole = Qt::UserRole + 2;
constexpr int kIconSize = 18;

QTreeWidget *makeTree()
{
    auto *tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    tree->setIconSize(QSize(kIconSize, kIconSize));

    // The stylesheet paints the row's own background, but the indentation
    // column beside it is painted by the style from the palette, and came out
    // as a square block next to the rounded selection. Nothing should be drawn
    // there, so the colour it would use is made invisible.
    QPalette rowPalette = tree->palette();
    rowPalette.setColor(QPalette::Highlight, Qt::transparent);
    rowPalette.setColor(QPalette::Inactive, QPalette::Highlight, Qt::transparent);
    tree->setPalette(rowPalette);
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

QIcon LibraryPanel::shapeIcon(const QString &specId)
{
    const catalog::MobjectSpec *spec = catalog::findMobject(specId);
    if (!spec)
        return {};

    // Evaluate a default instance and draw it exactly as the canvas would,
    // scaled to fit the icon.
    evaluator::ObjectState state;
    state.type = spec->id;
    state.params = catalog::defaultParams(*spec);

    // Text shapes draw their own string, which is unreadable at icon size, so
    // they get a letter standing for the kind of text instead.
    if (spec->shape == catalog::ShapeKind::Text || spec->shape == catalog::ShapeKind::MathText) {
        state.params.insert(spec->shape == catalog::ShapeKind::Text ? QStringLiteral("text")
                                                                    : QStringLiteral("tex"),
                            spec->shape == catalog::ShapeKind::Text ? QStringLiteral("T")
                                                                    : QStringLiteral("x"));
    }

    QPainterPath path = SceneRenderer::shapeOf(state);
    if (path.isEmpty())
        return {};

    const QRectF bounds = path.boundingRect();
    if (bounds.isEmpty())
        return {};

    const qreal ratio = 3.0;   // drawn oversized so it stays crisp on any display
    QPixmap pixmap(int(kIconSize * ratio), int(kIconSize * ratio));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(ratio);

    const qreal inset = 2.0;
    const qreal usable = kIconSize - inset * 2.0;
    const qreal scale = qMin(usable / bounds.width(), usable / bounds.height());

    QTransform transform;
    transform.translate(kIconSize / 2.0, kIconSize / 2.0);
    transform.scale(scale, -scale);   // y up, as in the scene
    transform.translate(-bounds.center().x(), -bounds.center().y());

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool isText = spec->shape == catalog::ShapeKind::Text
                        || spec->shape == catalog::ShapeKind::MathText;
    if (isText) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::palette().textMuted);
    } else {
        painter.setPen(QPen(theme::palette().textMuted, 1.3));
        painter.setBrush(Qt::NoBrush);
    }
    painter.drawPath(transform.map(path));
    painter.end();

    return QIcon(pixmap);
}

QIcon LibraryPanel::animationIcon(const QString &animationId)
{
    const catalog::AnimationSpec *spec = catalog::findAnimation(animationId);
    if (!spec)
        return {};

    const theme::Palette &p = theme::palette();
    QColor ink = p.violet;
    if (spec->isEntrance)
        ink = p.accent;
    else if (spec->isExit)
        ink = p.danger;
    else if (spec->effect == catalog::Effect::Wait)
        ink = p.textFaint;

    const qreal ratio = 3.0;
    QPixmap pixmap(int(kIconSize * ratio), int(kIconSize * ratio));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(ratio);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const QPointF centre(kIconSize / 2.0, kIconSize / 2.0);
    constexpr qreal r = 5.0;

    switch (spec->effect) {
    case catalog::Effect::Draw:
        // Three quarters of a circle: an outline being drawn.
        painter.drawArc(QRectF(centre.x() - r, centre.y() - r, r * 2, r * 2), 90 * 16, -270 * 16);
        break;
    case catalog::Effect::FadeIn:
    case catalog::Effect::FadeOut: {
        QLinearGradient gradient(centre.x() - r, 0, centre.x() + r, 0);
        const bool in = spec->effect == catalog::Effect::FadeIn;
        gradient.setColorAt(in ? 0.0 : 1.0, QColor(ink.red(), ink.green(), ink.blue(), 30));
        gradient.setColorAt(in ? 1.0 : 0.0, ink);
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(centre, r, r);
        break;
    }
    case catalog::Effect::Grow:
        painter.drawEllipse(centre, r, r);
        painter.drawEllipse(centre, r * 0.42, r * 0.42);
        break;
    case catalog::Effect::Shift:
        painter.drawLine(QPointF(centre.x() - r, centre.y()), QPointF(centre.x() + r, centre.y()));
        painter.drawLine(QPointF(centre.x() + r, centre.y()), QPointF(centre.x() + r - 3, centre.y() - 3));
        painter.drawLine(QPointF(centre.x() + r, centre.y()), QPointF(centre.x() + r - 3, centre.y() + 3));
        break;
    case catalog::Effect::Rotate:
        painter.drawArc(QRectF(centre.x() - r, centre.y() - r, r * 2, r * 2), 40 * 16, 280 * 16);
        painter.drawLine(QPointF(centre.x() + r * 0.75, centre.y() - r * 0.6),
                         QPointF(centre.x() + r * 0.2, centre.y() - r * 0.9));
        break;
    case catalog::Effect::Scale:
        painter.drawRect(QRectF(centre.x() - r, centre.y() - r, r * 2, r * 2));
        painter.drawLine(QPointF(centre.x() - r * 0.4, centre.y() + r * 0.4),
                         QPointF(centre.x() + r * 0.4, centre.y() - r * 0.4));
        break;
    case catalog::Effect::Recolor:
        painter.setBrush(ink);
        painter.drawEllipse(centre, r, r);
        break;
    case catalog::Effect::Wait:
        painter.drawEllipse(centre, r, r);
        painter.drawLine(centre, QPointF(centre.x(), centre.y() - r * 0.6));
        painter.drawLine(centre, QPointF(centre.x() + r * 0.5, centre.y()));
        break;
    }
    painter.end();

    return QIcon(pixmap);
}

QWidget *LibraryPanel::makePage(QLineEdit **searchOut, QTreeWidget **treeOut)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);

    auto *search = new QLineEdit;
    search->setPlaceholderText(tr("Search"));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);

    QTreeWidget *tree = makeTree();
    layout->addWidget(tree, 1);

    *searchOut = search;
    *treeOut = tree;
    return page;
}

LibraryPanel::LibraryPanel(EditorState *state, QWidget *parent)
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

    m_tabs = new SegmentedTabs;
    layout->addWidget(m_tabs, 1);

    QWidget *shapesPage = makePage(&m_search, &m_shapes);
    QWidget *animationsPage = makePage(&m_animationSearch, &m_animations);

    m_tabs->addPage(tr("Shapes"), shapesPage);
    m_tabs->addPage(tr("Animations"), animationsPage);
    m_tabs->addPage(tr("Scene"), new SceneOutliner(m_state));

    build();

    connect(m_search, &QLineEdit::textChanged, this, &LibraryPanel::applySearch);
    connect(m_animationSearch, &QLineEdit::textChanged, this, &LibraryPanel::applySearch);
    // Only itemDoubleClicked: a double-click emits itemActivated as well, so
    // connecting both added everything twice. Return is handled separately.
    connect(m_shapes, &QTreeWidget::itemDoubleClicked, this, &LibraryPanel::activate);
    connect(m_animations, &QTreeWidget::itemDoubleClicked, this, &LibraryPanel::activate);

    for (QTreeWidget *tree : {m_shapes, m_animations}) {
        auto *enter = new QShortcut(QKeySequence(Qt::Key_Return), tree);
        enter->setContext(Qt::WidgetShortcut);
        connect(enter, &QShortcut::activated, this,
                [this, tree] { activate(tree->currentItem()); });
    }
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
            item->setIcon(0, shapeIcon(spec.id));
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
            item->setIcon(0, animationIcon(spec.id));
            item->setToolTip(0, tr("Animate the selected object with %1").arg(spec.pythonName));
        }
    }
    m_animations->expandAll();
}

void LibraryPanel::applySearch(const QString &needle)
{
    QTreeWidget *target = sender() == m_animationSearch ? m_animations : m_shapes;
    for (QTreeWidget *tree : {target}) {
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
