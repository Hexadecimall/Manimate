#include "InspectorPanel.h"

#include "Document.h"
#include "EditorState.h"
#include "RateFunctions.h"
#include "SegmentedTabs.h"
#include "Theme.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

constexpr int kMinimumFieldWidth = 54;

/// Let a field shrink with the panel. Without this the widest editor sets the
/// panel's minimum width and the rest of the column is pushed off the edge.
void makeShrinkable(QWidget *widget, int minimumWidth = kMinimumFieldWidth)
{
    widget->setMinimumWidth(minimumWidth);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QDoubleSpinBox *makeDoubleBox()
{
    auto *box = new QDoubleSpinBox;
    box->setKeyboardTracking(false);
    box->setButtonSymbols(QAbstractSpinBox::NoButtons);
    box->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    makeShrinkable(box);
    return box;
}

QFormLayout *makeForm()
{
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(6);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    return form;
}

QLabel *fieldLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("role", "field");
    label->setMinimumWidth(56);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return label;
}

/// A button showing a colour, which opens the colour picker.
class ColorButton : public QPushButton
{
public:
    explicit ColorButton(const QColor &colour, QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(26);
        makeShrinkable(this, 60);
        setColor(colour);
    }

    void setColor(const QColor &colour)
    {
        m_color = colour;
        const QString ink = colour.lightnessF() > 0.55 ? QStringLiteral("#101216")
                                                       : QStringLiteral("#E6E9EF");
        setStyleSheet(QStringLiteral("QPushButton {"
                                     "  background: %1;"
                                     "  color: %2;"
                                     "  border: 1px solid %3;"
                                     "  border-radius: 6px;"
                                     "  padding: 4px 8px;"
                                     "}")
                          .arg(colour.name(), ink, theme::palette().border.name()));
        setText(colour.name().toUpper());
    }

    QColor color() const { return m_color; }

private:
    QColor m_color;
};

} // namespace

QScrollArea *InspectorPanel::makePage()
{
    auto *page = new QScrollArea;
    page->setWidgetResizable(true);
    page->setFrameShape(QFrame::NoFrame);
    page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return page;
}

InspectorPanel::InspectorPanel(EditorState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    const theme::Palette &p = theme::palette();
    setAutoFillBackground(true);
    QPalette background = palette();
    background.setColor(QPalette::Window, p.surface);
    setPalette(background);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_tabs = new SegmentedTabs;
    outer->addWidget(m_tabs, 1);

    m_objectPage = makePage();
    m_clipPage = makePage();
    m_scenePage = makePage();

    m_tabs->addPage(tr("Object"), m_objectPage);
    m_tabs->addPage(tr("Animation"), m_clipPage);
    m_tabs->addPage(tr("Scene"), m_scenePage);

    connect(m_state, &EditorState::selectionChanged, this, &InspectorPanel::rebuild);
    connect(m_state, &EditorState::documentChanged, this, &InspectorPanel::rebuild);

    rebuild();
}

QWidget *InspectorPanel::editorFor(const catalog::ParamSpec &spec, const QVariant &value,
                                   const std::function<void(const QVariant &)> &onChanged)
{
    using catalog::ParamType;

    switch (spec.type) {
    case ParamType::Number: {
        QDoubleSpinBox *box = makeDoubleBox();
        box->setRange(spec.minimum, spec.maximum);
        box->setSingleStep(spec.step);
        box->setDecimals(spec.step >= 1.0 ? 0 : 2);
        box->setValue(value.toDouble());
        connect(box, &QDoubleSpinBox::valueChanged, this,
                [onChanged](double v) { onChanged(v); });
        return box;
    }
    case ParamType::Integer: {
        auto *box = new QSpinBox;
        box->setRange(int(spec.minimum), int(spec.maximum));
        box->setValue(value.toInt());
        box->setKeyboardTracking(false);
        box->setButtonSymbols(QAbstractSpinBox::NoButtons);
        box->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        makeShrinkable(box);
        connect(box, &QSpinBox::valueChanged, this, [onChanged](int v) { onChanged(v); });
        return box;
    }
    case ParamType::Boolean: {
        auto *box = new QCheckBox;
        box->setChecked(value.toBool());
        connect(box, &QCheckBox::toggled, this, [onChanged](bool v) { onChanged(v); });
        return box;
    }
    case ParamType::Color: {
        auto *button = new ColorButton(value.value<QColor>());
        connect(button, &QPushButton::clicked, this, [this, button, onChanged] {
            const QColor chosen = QColorDialog::getColor(button->color(), this, tr("Choose a colour"));
            if (!chosen.isValid())
                return;
            m_state->beginEdit();
            button->setColor(chosen);
            onChanged(chosen);
        });
        return button;
    }
    case ParamType::Point: {
        const QPointF point = value.toPointF();

        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        QDoubleSpinBox *x = makeDoubleBox();
        QDoubleSpinBox *y = makeDoubleBox();
        for (QDoubleSpinBox *box : {x, y}) {
            box->setRange(-1000, 1000);
            box->setSingleStep(0.1);
            box->setDecimals(2);
            box->setMinimumWidth(44);
        }
        x->setPrefix(QStringLiteral("x "));
        y->setPrefix(QStringLiteral("y "));
        x->setValue(point.x());
        y->setValue(point.y());

        auto emitPoint = [x, y, onChanged] { onChanged(QPointF(x->value(), y->value())); };
        connect(x, &QDoubleSpinBox::valueChanged, this, emitPoint);
        connect(y, &QDoubleSpinBox::valueChanged, this, emitPoint);

        layout->addWidget(x);
        layout->addWidget(y);
        return row;
    }
    case ParamType::Text: {
        auto *edit = new QLineEdit(value.toString());
        makeShrinkable(edit);
        connect(edit, &QLineEdit::textEdited, this,
                [onChanged](const QString &v) { onChanged(v); });
        return edit;
    }
    case ParamType::Choice: {
        auto *box = new QComboBox;
        makeShrinkable(box);
        box->addItems(spec.choices);
        box->setCurrentText(value.toString());
        connect(box, &QComboBox::currentTextChanged, this,
                [onChanged](const QString &v) { onChanged(v); });
        return box;
    }
    }
    return new QWidget;
}

QFormLayout *InspectorPanel::addGroup(QVBoxLayout *layout, const QString &title,
                                      const QString &subtitle)
{
    auto *card = new QWidget;
    card->setProperty("role", "group");

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 12);
    cardLayout->setSpacing(8);

    auto *heading = new QLabel(title.toUpper());
    heading->setProperty("role", "groupTitle");
    cardLayout->addWidget(heading);

    if (!subtitle.isEmpty()) {
        auto *note = new QLabel(subtitle);
        note->setProperty("role", "subtitle");
        note->setWordWrap(true);
        cardLayout->addWidget(note);
    }

    QFormLayout *form = makeForm();
    cardLayout->addLayout(form);

    layout->addWidget(card);
    return form;
}

void InspectorPanel::addObjectSection(QVBoxLayout *layout, ObjectId id)
{
    const SceneObject *object = m_state->document().findObject(id);
    if (!object)
        return;

    const catalog::MobjectSpec *spec = catalog::findMobject(object->type);
    QFormLayout *form = addGroup(layout, spec ? spec->displayName : tr("Object"));

    auto *nameEdit = new QLineEdit(object->name);
    makeShrinkable(nameEdit);
    connect(nameEdit, &QLineEdit::editingFinished, this, [this, id, nameEdit] {
        m_state->setObjectName(id, nameEdit->text());
    });
    form->addRow(fieldLabel(tr("Name")), nameEdit);

    if (!spec)
        return;

    for (const catalog::ParamSpec &param : spec->params) {
        const QVariant value = catalog::paramOr(object->params, spec->params, param.id);
        form->addRow(fieldLabel(param.label),
                     editorFor(param, value, [this, id, param](const QVariant &v) {
                         m_state->setObjectParam(id, param.id, v);
                     }));
    }
}

void InspectorPanel::addClipSection(QVBoxLayout *layout, ClipId id)
{
    const Clip *clip = m_state->document().findClip(id);
    if (!clip)
        return;

    const catalog::AnimationSpec *spec = catalog::findAnimation(clip->type);
    const SceneObject *object = m_state->document().findObject(clip->objectId);
    QFormLayout *form = addGroup(layout, spec ? spec->displayName : tr("Animation"),
                                 object ? tr("on %1").arg(object->name) : QString());

    QDoubleSpinBox *start = makeDoubleBox();
    start->setRange(0, 3600);
    start->setSingleStep(0.1);
    start->setDecimals(2);
    start->setSuffix(QStringLiteral(" s"));
    start->setValue(clip->start);

    QDoubleSpinBox *duration = makeDoubleBox();
    duration->setRange(0.05, 3600);
    duration->setSingleStep(0.1);
    duration->setDecimals(2);
    duration->setSuffix(QStringLiteral(" s"));
    duration->setValue(clip->duration);

    auto applyTiming = [this, id, start, duration] {
        const Clip *current = m_state->document().findClip(id);
        if (!current)
            return;
        m_state->beginEdit();
        m_state->setClipTiming(id, start->value(), duration->value(), current->track);
    };
    connect(start, &QDoubleSpinBox::valueChanged, this, applyTiming);
    connect(duration, &QDoubleSpinBox::valueChanged, this, applyTiming);

    form->addRow(fieldLabel(tr("Start")), start);
    form->addRow(fieldLabel(tr("Length")), duration);

    auto *rateBox = new QComboBox;
    makeShrinkable(rateBox);
    rateBox->addItems(rate::names());
    rateBox->setCurrentText(clip->rateFunc);
    connect(rateBox, &QComboBox::currentTextChanged, this,
            [this, id](const QString &name) { m_state->setClipRateFunction(id, name); });
    form->addRow(fieldLabel(tr("Easing")), rateBox);

    if (!spec)
        return;

    for (const catalog::ParamSpec &param : spec->params) {
        const QVariant value = catalog::paramOr(clip->params, spec->params, param.id);
        form->addRow(fieldLabel(param.label),
                     editorFor(param, value, [this, id, param](const QVariant &v) {
                         m_state->setClipParam(id, param.id, v);
                     }));
    }
}

void InspectorPanel::addSceneSection(QVBoxLayout *layout)
{
    const Document &document = m_state->document();
    QFormLayout *form = addGroup(layout, tr("Scene"));

    QDoubleSpinBox *duration = makeDoubleBox();
    duration->setRange(1, 3600);
    duration->setSingleStep(1);
    duration->setDecimals(2);
    duration->setSuffix(QStringLiteral(" s"));
    duration->setValue(document.timeline.duration);
    connect(duration, &QDoubleSpinBox::valueChanged, this,
            [this](double v) { m_state->setTimelineDuration(v); });
    form->addRow(fieldLabel(tr("Length")), duration);

    auto *resolution = new QLabel(QStringLiteral("%1 × %2").arg(document.render.width)
                                      .arg(document.render.height));
    resolution->setProperty("role", "subtitle");
    form->addRow(fieldLabel(tr("Size")), resolution);

    auto *fps = new QLabel(tr("%1 fps").arg(document.render.fps));
    fps->setProperty("role", "subtitle");
    form->addRow(fieldLabel(tr("Rate")), fps);

    auto *objects = new QLabel(QString::number(document.objects.size()));
    objects->setProperty("role", "subtitle");
    form->addRow(fieldLabel(tr("Objects")), objects);

    auto *clips = new QLabel(QString::number(document.timeline.clips.size()));
    clips->setProperty("role", "subtitle");
    form->addRow(fieldLabel(tr("Animations")), clips);
}

void InspectorPanel::addEmptyState(QVBoxLayout *layout, const QString &message)
{
    auto *hint = new QLabel(message);
    hint->setProperty("role", "subtitle");
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignHCenter);
    layout->addWidget(hint);
}

void InspectorPanel::fillPage(QScrollArea *page, int which)
{
    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    const ObjectId object = m_state->selectedObject();
    const ClipId clip = m_state->selectedClip();

    switch (which) {
    case 0:
        if (object != kInvalidObjectId)
            addObjectSection(layout, object);
        else
            addEmptyState(layout, tr("Select an object on the canvas or in the scene list."));
        break;
    case 1:
        if (clip != kInvalidClipId)
            addClipSection(layout, clip);
        else
            addEmptyState(layout, tr("Select a clip on the timeline."));
        break;
    default:
        addSceneSection(layout);
        break;
    }

    layout->addStretch(1);

    // Replaced wholesale rather than unpicked. Draining a layout by hand is
    // where the bugs live — a QLayoutItem owns any layout it holds, so freeing
    // both is a double free — and a fresh widget cannot leave a stale editor
    // pointing at an object that has since been deleted.
    QWidget *previous = page->takeWidget();
    page->setWidget(body);
    if (previous)
        previous->deleteLater();
}

void InspectorPanel::rebuild()
{
    fillPage(m_objectPage, 0);
    fillPage(m_clipPage, 1);
    fillPage(m_scenePage, 2);

    // Follow the selection to the tab that has something to say about it,
    // without overriding a tab the user picked deliberately.
    m_syncing = true;
    if (m_state->selectedClip() != kInvalidClipId)
        m_tabs->setCurrentIndex(1);
    else if (m_state->selectedObject() != kInvalidObjectId)
        m_tabs->setCurrentIndex(0);
    else
        m_tabs->setCurrentIndex(2);
    m_syncing = false;
}

} // namespace mn::ui
