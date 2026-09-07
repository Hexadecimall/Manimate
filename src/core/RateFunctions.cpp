#include "RateFunctions.h"

#include <QtMath>
#include <algorithm>

namespace mn::rate {
namespace {

double clamp01(double t)
{
    return std::clamp(t, 0.0, 1.0);
}

/// Manim's `smooth`: a quintic with zero first and second derivatives at both
/// ends, which is why its animations start and stop without a visible jolt.
double smooth(double t)
{
    t = clamp01(t);
    const double s = 1.0 - t;
    return (t * t * t) * (10.0 * s * s + 5.0 * s * t + t * t);
}

} // namespace

double apply(const QString &name, double t)
{
    t = clamp01(t);

    if (name.isEmpty() || name == QLatin1String("linear"))
        return t;
    if (name == QLatin1String("smooth"))
        return smooth(t);
    if (name == QLatin1String("rush_into"))
        return 2.0 * smooth(t / 2.0);
    if (name == QLatin1String("rush_from"))
        return 2.0 * smooth(t / 2.0 + 0.5) - 1.0;
    if (name == QLatin1String("slow_into"))
        return std::sqrt(1.0 - (1.0 - t) * (1.0 - t));
    if (name == QLatin1String("double_smooth"))
        return t < 0.5 ? 0.5 * smooth(2.0 * t) : 0.5 * (1.0 + smooth(2.0 * t - 1.0));
    if (name == QLatin1String("there_and_back"))
        return t < 0.5 ? smooth(2.0 * t) : smooth(2.0 - 2.0 * t);
    if (name == QLatin1String("there_and_back_with_pause")) {
        if (t < 1.0 / 3.0)
            return smooth(3.0 * t);
        if (t < 2.0 / 3.0)
            return 1.0;
        return smooth(3.0 - 3.0 * t);
    }
    if (name == QLatin1String("running_start"))
        return t * t * (3.6 * t - 2.6);  // overshoots backwards before setting off
    if (name == QLatin1String("ease_in_sine"))
        return 1.0 - std::cos((t * M_PI) / 2.0);
    if (name == QLatin1String("ease_out_sine"))
        return std::sin((t * M_PI) / 2.0);
    if (name == QLatin1String("ease_in_out_sine"))
        return -(std::cos(M_PI * t) - 1.0) / 2.0;
    if (name == QLatin1String("ease_in_quad"))
        return t * t;
    if (name == QLatin1String("ease_out_quad"))
        return 1.0 - (1.0 - t) * (1.0 - t);
    if (name == QLatin1String("ease_in_out_quad"))
        return t < 0.5 ? 2.0 * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 2.0) / 2.0;
    if (name == QLatin1String("ease_in_cubic"))
        return t * t * t;
    if (name == QLatin1String("ease_out_cubic"))
        return 1.0 - std::pow(1.0 - t, 3.0);
    if (name == QLatin1String("ease_in_out_cubic"))
        return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
    if (name == QLatin1String("ease_in_expo"))
        return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0);
    if (name == QLatin1String("ease_out_expo"))
        return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t);
    if (name == QLatin1String("ease_out_back")) {
        constexpr double c1 = 1.70158;
        constexpr double c3 = c1 + 1.0;
        return 1.0 + c3 * std::pow(t - 1.0, 3.0) + c1 * std::pow(t - 1.0, 2.0);
    }
    if (name == QLatin1String("ease_out_bounce")) {
        constexpr double n1 = 7.5625;
        constexpr double d1 = 2.75;
        if (t < 1.0 / d1)
            return n1 * t * t;
        if (t < 2.0 / d1) {
            t -= 1.5 / d1;
            return n1 * t * t + 0.75;
        }
        if (t < 2.5 / d1) {
            t -= 2.25 / d1;
            return n1 * t * t + 0.9375;
        }
        t -= 2.625 / d1;
        return n1 * t * t + 0.984375;
    }

    return t;
}

const QStringList &names()
{
    static const QStringList result = {
        QStringLiteral("smooth"),
        QStringLiteral("linear"),
        QStringLiteral("rush_into"),
        QStringLiteral("rush_from"),
        QStringLiteral("slow_into"),
        QStringLiteral("double_smooth"),
        QStringLiteral("there_and_back"),
        QStringLiteral("there_and_back_with_pause"),
        QStringLiteral("running_start"),
        QStringLiteral("ease_in_sine"),
        QStringLiteral("ease_out_sine"),
        QStringLiteral("ease_in_out_sine"),
        QStringLiteral("ease_in_quad"),
        QStringLiteral("ease_out_quad"),
        QStringLiteral("ease_in_out_quad"),
        QStringLiteral("ease_in_cubic"),
        QStringLiteral("ease_out_cubic"),
        QStringLiteral("ease_in_out_cubic"),
        QStringLiteral("ease_in_expo"),
        QStringLiteral("ease_out_expo"),
        QStringLiteral("ease_out_back"),
        QStringLiteral("ease_out_bounce"),
    };
    return result;
}

} // namespace mn::rate
