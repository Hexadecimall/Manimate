#include "InspectorPanel.h"

#include "Document.h"
#include "EditorState.h"
#include "RateFunctions.h"
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

QLabel *heading(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("role", "section");
    return label;
}

QFormLayout *makeForm()
{
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(7);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
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

InspectorPanel::InspectorPanel(EditorState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    const theme::Palette &p = theme::palette();
    setStyleSheet(QStringLiteral("QWidget { background: %1; }").arg(p.surface.name()));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(m_scroll);

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
        auto *box = new QDoubleSpinBox;
        box->setRange(spec.minimum, spec.maximum);
        box->setSingleStep(spec.step);
        box->setDecimals(3);
        box->setValue(value.toDouble());
        box->setKeyboardTracking(false);
        connect(box, &QDoubleSpinBox::valueChanged, this,
                [onChanged](double v) { onChanged(v); });
        return box;
    }
    case ParamType::Integer: {
        auto *box = new QSpinBox;
        box->setRange(int(spec.minimum), int(spec.maximum));
        box->setValue(value.toInt());
        box->setKeyboardTracking(false);
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

        auto *x = new QDoubleSpinBox;
        auto *y = new QDoubleSpinBox;
        for (QDoubleSpinBox *box : {x, y}) {
            box->setRange(-1000, 1000);
            box->setSingleStep(0.1);
            box->setDecimals(3);
            box->setKeyboardTracking(false);
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
        connect(edit, &QLineEdit::textEdited, this,
                [onChanged](const QString &v) { onChanged(v); });
        return edit;
    }
    case ParamType::Choice: {
        auto *box = new QComboBox;
        box->addItems(spec.choices);
        box->setCurrentText(value.toString());
        connect(box, &QComboBox::currentTextChanged, this,
                [onChanged](const QString &v) { onChanged(v); });
        return box;
    }
    }
    return new QWidget;
}

void InspectorPanel::addObjectSection(QVBoxLayout *layout, ObjectId id)
{
    const SceneObject *object = m_state->document().findObject(id);
    if (!object)
        return;

    const catalog::MobjectSpec *spec = catalog::findMobject(object->type);
    layout->addWidget(heading(spec ? spec->displayName.toUpper() : tr("OBJECT")));

    auto *form = makeForm();

    auto *nameEdit = new QLineEdit(object->name);
    connect(nameEdit, &QLineEdit::editingFinished, this, [this, id, nameEdit] {
        m_state->setObjectName(id, nameEdit->text());
    });
    form->addRow(tr("Name"), nameEdit);

    if (spec) {
        for (const catalog::ParamSpec &param : spec->params) {
            const QVariant value =
                catalog::paramOr(object->params, spec->params, param.id);
            form->addRow(param.label, editorFor(param, value, [this, id, param](const QVariant &v) {
                             m_state->setObjectParam(id, param.id, v);
                         }));
        }
    }

    layout->addLayout(form);
}

void InspectorPanel::addClipSection(QVBoxLayout *layout, ClipId id)
{
    const Clip *clip = m_state->document().findClip(id);
    if (!clip)
        return;

    const catalog::AnimationSpec *spec = catalog::findAnimation(clip->type);
    layout->addWidget(heading(spec ? spec->displayName.toUpper() : tr("ANIMATION")));

    auto *form = makeForm();

    auto *start = new QDoubleSpinBox;
    start->setRange(0, 3600);
    start->setSingleStep(0.1);
    start->setDecimals(2);
    start->setSuffix(QStringLiteral(" s"));
    start->setValue(clip->start);
    start->setKeyboardTracking(false);

    auto *duration = new QDoubleSpinBox;
    duration->setRange(0.05, 3600);
    duration->setSingleStep(0.1);
    duration->setDecimals(2);
    duration->setSuffix(QStringLiteral(" s"));
    duration->setValue(clip->duration);
    duration->setKeyboardTracking(false);

    auto applyTiming = [this, id, start, duration] {
        const Clip *current = m_state->document().findClip(id);
        if (!current)
            return;
        m_state->beginEdit();
        m_state->setClipTiming(id, start->value(), duration->value(), current->track);
    };
    connect(start, &QDoubleSpinBox::valueChanged, this, applyTiming);
    connect(duration, &QDoubleSpinBox::valueChanged, this, applyTiming);

    form->addRow(tr("Start"), start);
    form->addRow(tr("Duration"), duration);

    auto *rateBox = new QComboBox;
    rateBox->addItems(rate::names());
    rateBox->setCurrentText(clip->rateFunc);
    connect(rateBox, &QComboBox::currentTextChanged, this,
            [this, id](const QString &name) { m_state->setClipRateFunction(id, name); });
    form->addRow(tr("Easing"), rateBox);

    if (spec) {
        for (const catalog::ParamSpec &param : spec->params) {
            const QVariant value = catalog::paramOr(clip->params, spec->params, param.id);
            form->addRow(param.label, editorFor(param, value, [this, id, param](const QVariant &v) {
                             m_state->setClipParam(id, param.id, v);
                         }));
        }
    }

    layout->addLayout(form);
}

void InspectorPanel::addSceneSection(QVBoxLayout *layout)
{
    layout->addWidget(heading(tr("SCENE")));

    auto *form = makeForm();
    const Document &document = m_state->document();

    auto *duration = new QDoubleSpinBox;
    duration->setRange(1, 3600);
    duration->setSingleStep(1);
    duration->setDecimals(2);
    duration->setSuffix(QStringLiteral(" s"));
    duration->setValue(document.timeline.duration);
    duration->setKeyboardTracking(false);
    connect(duration, &QDoubleSpinBox::valueChanged, this,
            [this](double v) { m_state->setTimelineDuration(v); });
    form->addRow(tr("Length"), duration);

    auto *resolution = new QLabel(QStringLiteral("%1 × %2 · %3 fps")
                                     .arg(document.render.width)
                                     .arg(document.render.height)
                                     .arg(document.render.fps));
    resolution->setProperty("role", "subtitle");
    form->addRow(tr("Video"), resolution);

    auto *objects = new QLabel(QString::number(document.objects.size()));
    objects->setProperty("role", "subtitle");
    form->addRow(tr("Objects"), objects);

    auto *clips = new QLabel(QString::number(document.timeline.clips.size()));
    clips->setProperty("role", "subtitle");
    form->addRow(tr("Animations"), clips);

    layout->addLayout(form);
}

void InspectorPanel::rebuild()
{
    // Replaced wholesale rather than unpicked. Draining a layout by hand is
    // where the bugs live — a QLayoutItem owns any layout it holds, so freeing
    // both is a double free — and a fresh widget cannot leave a stale editor
    // pointing at an object that has since been deleted.
    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(14);

    const ObjectId object = m_state->selectedObject();
    const ClipId clip = m_state->selectedClip();

    if (clip != kInvalidClipId)
        addClipSection(layout, clip);

    if (object != kInvalidObjectId)
        addObjectSection(layout, object);

    if (object == kInvalidObjectId && clip == kInvalidClipId)
        addSceneSection(layout);

    layout->addStretch(1);

    QWidget *previous = m_scroll->takeWidget();
    m_scroll->setWidget(body);
    if (previous)
        previous->deleteLater();
}

} // namespace mn::ui
