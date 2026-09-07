#pragma once

#include <QStringList>
#include <QWidget>

class QStackedWidget;

namespace mn::ui {

/// A row of tabs across the top of a panel, with the panel's pages under it.
///
/// Editing applications put several things in one column and switch between
/// them, rather than stacking them all and letting each have a sliver of the
/// height. This is that control: a compact strip of tabs and the stack they
/// drive, in one widget so a panel is one line of setup.
class SegmentedTabs : public QWidget
{
    Q_OBJECT

public:
    explicit SegmentedTabs(QWidget *parent = nullptr);

    /// Add a page under a tab of its own. Returns the page's index.
    int addPage(const QString &title, QWidget *page);

    int currentIndex() const;
    void setCurrentIndex(int index);

    /// Height of the tab strip, so neighbouring panels can line up with it.
    static constexpr int kStripHeight = 30;

Q_SIGNALS:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    /// Where the tab at `index` sits in the strip.
    QRectF tabRect(int index) const;
    int tabAt(const QPointF &point) const;

    QStringList m_titles;
    QStackedWidget *m_stack = nullptr;
    int m_hovered = -1;
};

} // namespace mn::ui
