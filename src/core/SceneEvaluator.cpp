#include "SceneEvaluator.h"

#include "Catalog.h"
#include "Document.h"
#include "RateFunctions.h"

#include <QtMath>
#include <algorithm>

namespace mn::evaluator {
namespace {

QColor blend(const QColor &from, const QColor &to, double t)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                            from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF() + (to.blueF() - from.blueF()) * t,
                            from.alphaF() + (to.alphaF() - from.alphaF()) * t);
}

/// How far through `clip` the moment `time` is, before easing. Below zero means
/// the clip has not begun; above one, that it has finished.
double rawProgress(const Clip &clip, double time)
{
    if (clip.duration <= 0.0)
        return time >= clip.start ? 1.0 : 0.0;
    return (time - clip.start) / clip.duration;
}

} // namespace

ObjectState evaluateObject(const Document &document, ObjectId id, double time)
{
    ObjectState state;

    const SceneObject *object = document.findObject(id);
    if (!object)
        return state;

    state.id = object->id;
    state.type = object->type;
    state.name = object->name;
    state.zOrder = object->zOrder;
    state.visible = object->visible;

    const catalog::MobjectSpec *spec = catalog::findMobject(object->type);
    if (spec) {
        state.params = catalog::defaultParams(*spec);
        for (auto it = object->params.constBegin(); it != object->params.constEnd(); ++it)
            state.params.insert(it.key(), it.value());
    } else {
        state.params = object->params;
    }

    if (!state.visible)
        return state;

    // Clips are applied in the order they begin, so later animations build on
    // what earlier ones left behind.
    QVector<const Clip *> clips;
    for (const Clip &clip : document.timeline.clips) {
        if (clip.objectId == id)
            clips.append(&clip);
    }
    std::sort(clips.begin(), clips.end(), [](const Clip *a, const Clip *b) {
        return a->start < b->start;
    });

    bool hasEntrance = false;
    bool entered = false;

    for (const Clip *clip : std::as_const(clips)) {
        const catalog::AnimationSpec *animation = catalog::findAnimation(clip->type);
        if (!animation)
            continue;

        if (animation->isEntrance) {
            hasEntrance = true;
            if (time >= clip->start)
                entered = true;
        }

        const double raw = rawProgress(*clip, time);
        if (raw <= 0.0)
            continue;  // not started; nothing to apply

        const double p = rate::apply(clip->rateFunc, std::clamp(raw, 0.0, 1.0));
        const bool finished = raw >= 1.0;

        const QVariantMap &params = clip->params;
        const QVector<catalog::ParamSpec> &paramSpecs = animation->params;

        switch (animation->effect) {
        case catalog::Effect::Draw:
            if (animation->isExit) {
                state.drawProgress = 1.0 - p;
                if (finished)
                    state.visible = false;
            } else {
                state.drawProgress = p;
            }
            break;

        case catalog::Effect::FadeIn: {
            state.opacity = p;
            const QPointF shift =
                catalog::paramOr(params, paramSpecs, QStringLiteral("shift")).toPointF();
            // Starts displaced by -shift and arrives in place.
            state.offset -= shift * (1.0 - p);
            break;
        }

        case catalog::Effect::FadeOut: {
            state.opacity = 1.0 - p;
            const QPointF shift =
                catalog::paramOr(params, paramSpecs, QStringLiteral("shift")).toPointF();
            state.offset += shift * p;
            if (finished)
                state.visible = false;
            break;
        }

        case catalog::Effect::Grow:
            if (animation->isExit) {
                state.scale *= (1.0 - p);
                if (finished)
                    state.visible = false;
            } else {
                state.scale *= p;
            }
            break;

        case catalog::Effect::Shift: {
            const QPointF by = catalog::paramOr(params, paramSpecs, QStringLiteral("by")).toPointF();
            state.offset += by * p;
            break;
        }

        case catalog::Effect::Rotate: {
            const double angle =
                catalog::paramOr(params, paramSpecs, QStringLiteral("angle")).toDouble();
            state.rotationDegrees += angle * p;
            break;
        }

        case catalog::Effect::Scale: {
            const double factor =
                catalog::paramOr(params, paramSpecs, QStringLiteral("factor")).toDouble();
            state.scale *= 1.0 + (factor - 1.0) * p;
            break;
        }

        case catalog::Effect::Recolor: {
            const QColor to = catalog::paramOr(params, paramSpecs, QStringLiteral("to")).value<QColor>();
            const QColor from = state.colorOverride.isValid()
                                    ? state.colorOverride
                                    : state.params.value(QStringLiteral("color")).value<QColor>();
            if (to.isValid() && from.isValid())
                state.colorOverride = blend(from, to, p);
            break;
        }

        case catalog::Effect::MoveTo: {
            const QPointF to = catalog::paramOr(params, paramSpecs, QStringLiteral("to")).toPointF();
            const QPointF from = state.params.value(QStringLiteral("position")).toPointF();
            state.offset += (to - from) * p;
            break;
        }

        case catalog::Effect::Fade: {
            const double to = catalog::paramOr(params, paramSpecs, QStringLiteral("to")).toDouble();
            state.opacity = 1.0 + (to - 1.0) * p;
            break;
        }

        case catalog::Effect::Emphasise: {
            // Emphasis returns the object to where it started, so it swells and
            // settles rather than leaving anything behind.
            const double swell = std::sin(p * M_PI);
            const double factor =
                catalog::paramOr(params, paramSpecs, QStringLiteral("scale_factor")).toDouble();
            state.scale *= 1.0 + (factor > 0 ? factor - 1.0 : 0.15) * swell;
            break;
        }

        case catalog::Effect::Code:
        case catalog::Effect::Wait:
            // Neither shows anything on the canvas: a Python block runs at
            // render time, and the canvas cannot know what it will do.
            break;
        }
    }

    // An object with an entrance animation does not exist before it begins.
    if (hasEntrance && !entered)
        state.visible = false;

    // A child moves with its group. Walked upwards rather than downwards so a
    // single object can be evaluated on its own, without building the tree.
    ObjectId ancestor = object->parentId;
    int guard = 0;
    while (ancestor != kInvalidObjectId && guard++ < 64) {
        const SceneObject *parent = document.findObject(ancestor);
        if (!parent)
            break;

        const ObjectState parentState = evaluateObject(document, ancestor, time);
        if (!parentState.visible)
            state.visible = false;

        state.opacity *= parentState.opacity;
        state.scale *= parentState.scale;
        state.rotationDegrees += parentState.rotationDegrees;

        // The parent's own position is where the group sits, so a child is
        // carried by it as well as by whatever animated the group.
        const QPointF parentOrigin =
            parentState.params.value(QStringLiteral("position")).toPointF();
        state.offset += parentState.offset + parentOrigin;

        ancestor = parent->parentId;
    }

    return state;
}

QVector<ObjectState> evaluate(const Document &document, double time)
{
    QVector<ObjectState> result;
    result.reserve(document.objects.size());

    for (const SceneObject &object : document.objects) {
        ObjectState state = evaluateObject(document, object.id, time);
        if (state.visible && state.opacity > 0.0 && state.scale > 0.0)
            result.append(std::move(state));
    }

    std::stable_sort(result.begin(), result.end(), [](const ObjectState &a, const ObjectState &b) {
        return a.zOrder < b.zOrder;
    });
    return result;
}

} // namespace mn::evaluator
