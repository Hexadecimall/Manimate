#include "ProjectCardDelegate.h"

#include "RecentProjectsModel.h"
#include "Theme.h"

#include <QDateTime>
#include <QPainter>
#include <QPainterPath>

namespace mn::ui {
namespace {

constexpr int kCardHeight = 74;
constexpr int kCardSpacing = 8;
constexpr int kPadding = 14;
constexpr int kBadgeSize = 44;

/// Give each project a stable accent drawn from the theme's highlight colours,
/// so the list is scannable by colour as well as by name.
QColor badgeColor(const QString &seed)
{
    const theme::Palette &p = theme::palette();
    const QVector<QColor> colors = {p.accent, p.success, p.violet, p.warning, p.teal, p.danger};
    if (seed.isEmpty())
        return colors.first();

    uint hash = 2166136261u;
    for (const QChar c : seed) {
        hash ^= uint(c.unicode());
        hash *= 16777619u;
    }
    return colors.at(int(hash % uint(colors.size())));
}

QString initials(const QString &name)
{
    const QStringList words = name.split(QRegularExpression(QStringLiteral("[\\s_-]+")), Qt::SkipEmptyParts);
    if (words.isEmpty())
        return QStringLiteral("?");
    if (words.size() == 1)
        return words.first().left(1).toUpper();
    return (words.first().left(1) + words.at(1).left(1)).toUpper();
}

} // namespace

ProjectCardDelegate::ProjectCardDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QString ProjectCardDelegate::describeTime(const QDateTime &when)
{
    if (!when.isValid())
        return {};

    const qint64 seconds = when.secsTo(QDateTime::currentDateTime());
    if (seconds < 60)
        return QObject::tr("just now");
    if (seconds < 3600) {
        const int minutes = int(seconds / 60);
        return minutes == 1 ? QObject::tr("1 minute ago") : QObject::tr("%1 minutes ago").arg(minutes);
    }
    if (seconds < 86400) {
        const int hours = int(seconds / 3600);
        return hours == 1 ? QObject::tr("1 hour ago") : QObject::tr("%1 hours ago").arg(hours);
    }
    if (seconds < 86400 * 7) {
        const int days = int(seconds / 86400);
        return days == 1 ? QObject::tr("yesterday") : QObject::tr("%1 days ago").arg(days);
    }
    return when.date().toString(QStringLiteral("d MMM yyyy"));
}

QSize ProjectCardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return {option.rect.width(), kCardHeight + kCardSpacing};
}

void ProjectCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const theme::Palette &p = theme::palette();

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool missing = index.data(RecentProjectsModel::MissingRole).toBool();

    const QRectF card = QRectF(option.rect.adjusted(0, 0, 0, -kCardSpacing));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QColor background = p.surface;
    if (selected)
        background = theme::mix(p.surfaceRaised, p.accent, 0.16);
    else if (hovered)
        background = p.surfaceHover;

    QPainterPath shape;
    shape.addRoundedRect(card, theme::kRadius + 2, theme::kRadius + 2);
    painter->fillPath(shape, background);

    painter->setPen(QPen(selected ? p.accent : p.border, 1.0));
    painter->drawPath(shape.translated(0.5, 0.5));

    const QString name = index.data(RecentProjectsModel::NameRole).toString();
    const QColor accent = missing ? p.textFaint : badgeColor(name);

    // Coloured initial badge.
    const QRectF badge(card.left() + kPadding, card.top() + (card.height() - kBadgeSize) / 2.0,
                       kBadgeSize, kBadgeSize);
    QPainterPath badgePath;
    badgePath.addRoundedRect(badge, theme::kRadius, theme::kRadius);
    painter->fillPath(badgePath, theme::mix(p.window, accent, 0.22));
    painter->setPen(QPen(theme::mix(p.border, accent, 0.5), 1.0));
    painter->drawPath(badgePath);

    QFont badgeFont = theme::font(1, QFont::DemiBold);
    badgeFont.setPixelSize(16);
    painter->setFont(badgeFont);
    painter->setPen(accent);
    painter->drawText(badge, Qt::AlignCenter, initials(name));

    const qreal textLeft = badge.right() + kPadding;
    const QString timestamp = describeTime(index.data(RecentProjectsModel::LastOpenedRole).toDateTime());

    QFont timeFont = theme::font(1);
    timeFont.setPixelSize(11);
    const QFontMetrics timeMetrics(timeFont);
    const int timeWidth = timestamp.isEmpty() ? 0 : timeMetrics.horizontalAdvance(timestamp) + kPadding;
    const qreal textRight = card.right() - kPadding - timeWidth;

    QFont nameFont = theme::font(1, QFont::DemiBold);
    nameFont.setPixelSize(14);
    painter->setFont(nameFont);
    painter->setPen(missing ? p.textMuted : p.text);
    const QFontMetrics nameMetrics(nameFont);
    const QRectF nameRect(textLeft, card.top() + 15, textRight - textLeft, nameMetrics.height());
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(name, Qt::ElideMiddle, int(nameRect.width())));

    QFont pathFont = theme::font(1);
    pathFont.setPixelSize(11);
    painter->setFont(pathFont);
    const QFontMetrics pathMetrics(pathFont);

    QString secondary = index.data(RecentProjectsModel::DescriptionRole).toString().simplified();
    if (secondary.isEmpty())
        secondary = index.data(RecentProjectsModel::RootDirRole).toString();

    painter->setPen(missing ? p.danger : p.textFaint);
    const QRectF pathRect(textLeft, nameRect.bottom() + 2, textRight - textLeft, pathMetrics.height());
    painter->drawText(pathRect, Qt::AlignLeft | Qt::AlignVCenter,
                      pathMetrics.elidedText(missing ? QObject::tr("Missing — %1").arg(secondary) : secondary,
                                             Qt::ElideMiddle, int(pathRect.width())));

    if (!timestamp.isEmpty()) {
        painter->setFont(timeFont);
        painter->setPen(p.textFaint);
        const QRectF timeRect(card.right() - kPadding - timeWidth, card.top(), timeWidth, card.height());
        painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, timestamp);
    }

    painter->restore();
}

} // namespace mn::ui
