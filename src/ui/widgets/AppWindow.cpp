#include "AppWindow.h"

#include "Theme.h"
#include "TitleBar.h"

#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace mn::ui {

bool AppWindow::usesNativeMenuBar()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

AppWindow::AppWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);

    auto *central = new QWidget(this);
    m_layout = new QVBoxLayout(central);
    // The margin is the band left uncovered by children, so the window itself
    // still receives the mouse events that start an edge resize.
    m_layout->setContentsMargins(kResizeMargin, kResizeMargin, kResizeMargin, kResizeMargin);
    m_layout->setSpacing(0);

    m_titleBar = new TitleBar(central);
    m_layout->addWidget(m_titleBar);
    setCentralWidget(central);

    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    central->setMouseTracking(true);
    installEventFilter(this);
}

void AppWindow::setContent(QWidget *content)
{
    if (m_content) {
        m_layout->removeWidget(m_content);
        m_content->deleteLater();
    }
    m_content = content;
    if (m_content)
        m_layout->addWidget(m_content, 1);
}

void AppWindow::setMenus(const QList<QMenu *> &menus)
{
    if (usesNativeMenuBar()) {
        QMenuBar *bar = menuBar();
        bar->clear();
        for (QMenu *menu : menus)
            bar->addMenu(menu);
        return;
    }

    for (QMenu *menu : menus) {
        auto *button = new QToolButton(m_titleBar);
        button->setProperty("role", "menu");
        button->setText(menu->title());
        button->setMenu(menu);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::ArrowCursor);
        m_titleBar->addActionWidget(button);
    }
}

void AppWindow::setWindowTitle(const QString &title)
{
    QMainWindow::setWindowTitle(title);
    m_titleBar->setTitle(title);
}

void AppWindow::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);

    const theme::Palette &p = theme::palette();
    QPainter painter(this);
    painter.fillRect(rect(), p.window);
    painter.setPen(QPen(p.border, 1.0));
    painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
}

Qt::Edges AppWindow::edgesAt(const QPoint &position) const
{
    Qt::Edges edges;
    if (position.x() <= kResizeMargin)
        edges |= Qt::LeftEdge;
    if (position.x() >= width() - kResizeMargin)
        edges |= Qt::RightEdge;
    if (position.y() <= kResizeMargin)
        edges |= Qt::TopEdge;
    if (position.y() >= height() - kResizeMargin)
        edges |= Qt::BottomEdge;
    return edges;
}

Qt::CursorShape AppWindow::cursorForEdges(Qt::Edges edges)
{
    if ((edges & Qt::TopEdge && edges & Qt::LeftEdge) || (edges & Qt::BottomEdge && edges & Qt::RightEdge))
        return Qt::SizeFDiagCursor;
    if ((edges & Qt::TopEdge && edges & Qt::RightEdge) || (edges & Qt::BottomEdge && edges & Qt::LeftEdge))
        return Qt::SizeBDiagCursor;
    if (edges & (Qt::LeftEdge | Qt::RightEdge))
        return Qt::SizeHorCursor;
    if (edges & (Qt::TopEdge | Qt::BottomEdge))
        return Qt::SizeVerCursor;
    return Qt::ArrowCursor;
}

bool AppWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != this || isMaximized() || isFullScreen())
        return QMainWindow::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseMove: {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        setCursor(cursorForEdges(edgesAt(mouse->position().toPoint())));
        break;
    }
    case QEvent::MouseButtonPress: {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            break;
        const Qt::Edges edges = edgesAt(mouse->position().toPoint());
        if (edges && windowHandle() && windowHandle()->startSystemResize(edges))
            return true;
        break;
    }
    case QEvent::Leave:
        unsetCursor();
        break;
    default:
        break;
    }

    return QMainWindow::eventFilter(watched, event);
}

} // namespace mn::ui
