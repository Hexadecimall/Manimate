#pragma once

#include <QStyledItemDelegate>

namespace mn::ui {

/// Paints each recent project as a card: name, folder, relative timestamp, and
/// a coloured initial. Cards are drawn rather than composed from widgets so a
/// long project list stays cheap to scroll.
class ProjectCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ProjectCardDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /// Relative description of `when`, e.g. "12 minutes ago".
    static QString describeTime(const QDateTime &when);
};

} // namespace mn::ui
