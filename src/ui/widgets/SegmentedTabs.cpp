#include "SegmentedTabs.h"

#include "Theme.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

// Tabs run from the panel's edge: a gap before the first one reads as a
// misalignment rather than as breathing room.
constexpr int kSidePadding = 0;
constexpr int kTabPadding = 15;

QFont tabFont()
{
    QFont font = theme::font(1, QFont::DemiBold);
    font.setPixelSize(11);
    font.setLetterSpacing(QFont::PercentageSpacing, 104.0);
    return font;
}

} // namespace

SegmentedTabs::SegmentedTabs(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // The strip is painted, not laid out, so the pages start below it.
    layout->addSpacing(kStripHeight);

    m_stack = new QStackedWidget;
    layout->addWidget(m_stack, 1);

    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        update();
        Q_EMIT currentIndexChanged(index);
    });
}

int SegmentedTabs::addPage(const QString &title, QWidget *page)
{
    m_titles.append(title);
    const int index = m_stack->addWidget(page);
    update();
    return index;
}

int SegmentedTabs::currentIndex() const
{
    return m_stack->currentIndex();
}

void SegmentedTabs::setCurrentIndex(int index)
{
    m_stack->setCurrentIndex(index);
}

QRectF SegmentedTabs::tabRect(int index) const
{
    const QFontMetricsF metrics(tabFont());

    qreal x = kSidePadding;
    for (int i = 0; i < index; ++i)
        x += metrics.horizontalAdvance(m_titles.at(i)) + kTabPadding * 2;

    const qreal width = metrics.horizontalAdvance(m_titles.at(index)) + kTabPadding * 2;
    return QRectF(x, 0, width, kStripHeight);
}

int SegmentedTabs::tabAt(const QPointF &point) const
{
    if (point.y() > kStripHeight)
        return -1;
    for (int i = 0; i < m_titles.size(); ++i) {
        if (tabRect(i).contains(point))
            return i;
    }
    return -1;
}

void SegmentedTabs::paintEvent(QPaintEvent *)
{
    const theme::Palette &p = theme::palette();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF strip(0, 0, width(), kStripHeight);
    painter.fillRect(strip, p.surfaceRaised);

    painter.setFont(tabFont());

    for (int i = 0; i < m_titles.size(); ++i) {
        const QRectF box = tabRect(i);
        const bool active = i == m_stack->currentIndex();

        if (active)
            painter.fillRect(box, p.surface);
        else if (i == m_hovered)
            painter.fillRect(box, p.surfaceHover);

        painter.setPen(active ? p.text : p.textMuted);
        painter.drawText(box, Qt::AlignCenter, m_titles.at(i));

        // The active tab is marked by a short bar in the accent: the accent's
        // one job in the chrome, so it still means something.
        if (active) {
            const qreal inset = 10.0;
            painter.fillRect(QRectF(box.left() + inset, box.bottom() - 2,
                                    box.width() - inset * 2, 2),
                             p.accent);
        }
    }

    painter.setPen(QPen(p.border, 1.0));
    painter.drawLine(QPointF(0, kStripHeight - 0.5), QPointF(width(), kStripHeight - 0.5));
}

void SegmentedTabs::mousePressEvent(QMouseEvent *event)
{
    const int index = tabAt(event->position());
    if (index >= 0) {
        setCurrentIndex(index);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void SegmentedTabs::mouseMoveEvent(QMouseEvent *event)
{
    const int index = tabAt(event->position());
    if (index != m_hovered) {
        m_hovered = index;
        setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void SegmentedTabs::leaveEvent(QEvent *event)
{
    if (m_hovered != -1) {
        m_hovered = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

} // namespace mn::ui
