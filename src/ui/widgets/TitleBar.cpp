#include "TitleBar.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

namespace mn::ui {
namespace {

constexpr int kEdgeMargin = 14;

} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(kHeight);
    setAttribute(Qt::WA_Hover);

    const bool controlsOnLeft = WindowButton::controlsBelongOnTheLeft();

    m_controls = new QWidget(this);
    m_controls->setAttribute(Qt::WA_Hover);
    m_controls->installEventFilter(this);

    m_close = new WindowButton(WindowButton::Kind::Close, m_controls);
    m_minimize = new WindowButton(WindowButton::Kind::Minimize, m_controls);
    m_maximize = new WindowButton(WindowButton::Kind::Maximize, m_controls);

    auto *controlsLayout = new QHBoxLayout(m_controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(2);
    if (controlsOnLeft) {
        controlsLayout->addWidget(m_close);
        controlsLayout->addWidget(m_minimize);
        controlsLayout->addWidget(m_maximize);
    } else {
        controlsLayout->addWidget(m_minimize);
        controlsLayout->addWidget(m_maximize);
        controlsLayout->addWidget(m_close);
    }

    m_titleLabel = new QLabel(this);
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont titleFont = theme::font(1, QFont::DemiBold);
    titleFont.setPixelSize(12);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(theme::palette().textMuted.name()));

    m_actionLayout = new QHBoxLayout;
    m_actionLayout->setContentsMargins(0, 0, 0, 0);
    m_actionLayout->setSpacing(2);

    m_trailingLayout = new QHBoxLayout;
    m_trailingLayout->setContentsMargins(0, 0, 0, 0);
    m_trailingLayout->setSpacing(8);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(kEdgeMargin, 0, controlsOnLeft ? kEdgeMargin : 0, 0);
    layout->setSpacing(0);

    if (controlsOnLeft) {
        layout->addWidget(m_controls);
        layout->addSpacing(18);
        layout->addWidget(m_titleLabel);
        layout->addSpacing(14);
        layout->addLayout(m_actionLayout);
        layout->addStretch(1);
        layout->addLayout(m_trailingLayout);
    } else {
        layout->addWidget(m_titleLabel);
        layout->addSpacing(14);
        layout->addLayout(m_actionLayout);
        layout->addStretch(1);
        layout->addLayout(m_trailingLayout);
        layout->addSpacing(12);
        layout->addWidget(m_controls);
    }

    connect(m_close, &QAbstractButton::clicked, this, [this] { window()->close(); });
    connect(m_minimize, &QAbstractButton::clicked, this, [this] { window()->showMinimized(); });
    connect(m_maximize, &QAbstractButton::clicked, this, &TitleBar::toggleMaximised);
}

void TitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void TitleBar::addActionWidget(QWidget *widget)
{
    m_actionLayout->addWidget(widget);
}

void TitleBar::addTrailingWidget(QWidget *widget)
{
    m_trailingLayout->addWidget(widget);
}

void TitleBar::setGroupHovered(bool hovered)
{
    m_close->setGroupHovered(hovered);
    m_minimize->setGroupHovered(hovered);
    m_maximize->setGroupHovered(hovered);
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_controls) {
        if (event->type() == QEvent::Enter)
            setGroupHovered(true);
        else if (event->type() == QEvent::Leave)
            setGroupHovered(false);
    }
    return QWidget::eventFilter(watched, event);
}

bool TitleBar::event(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange || event->type() == QEvent::Show)
        m_maximize->setRestoreState(window()->isMaximized());
    return QWidget::event(event);
}

void TitleBar::toggleMaximised()
{
    QWidget *host = window();
    if (host->isMaximized())
        host->showNormal();
    else
        host->showMaximized();
    m_maximize->setRestoreState(host->isMaximized());
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Hand the drag to the window system so snapping and multi-monitor
    // behaviour stay native instead of being reimplemented here.
    if (QWindow *handle = window()->windowHandle()) {
        if (handle->startSystemMove()) {
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        toggleMaximised();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TitleBar::paintEvent(QPaintEvent *)
{
    const theme::Palette &p = theme::palette();

    // Left transparent so the window's rounded top corners show through.
    QPainter painter(this);
    painter.setPen(QPen(p.border, 1.0));
    painter.drawLine(QPointF(1, height() - 0.5), QPointF(width() - 1, height() - 0.5));
}

} // namespace mn::ui
