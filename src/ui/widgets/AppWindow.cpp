#include "AppWindow.h"

#include "Theme.h"
#include "TitleBar.h"

#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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
    const theme::Palette &p = theme::palette();

    setWindowFlag(Qt::FramelessWindowHint, true);
    // A frameless window is square and shadowless by default. Painting the
    // shape here needs the area outside it to be genuinely transparent.
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);

    auto *central = new QWidget(this);
    central->setAttribute(Qt::WA_TranslucentBackground);
    central->setMouseTracking(true);

    m_outerLayout = new QVBoxLayout(central);
    m_outerLayout->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);
    m_outerLayout->setSpacing(0);

    m_shell = new QWidget(central);
    m_shell->setObjectName(QStringLiteral("shell"));
    m_shell->setAttribute(Qt::WA_StyledBackground, true);
    m_shell->setStyleSheet(QStringLiteral("QWidget#shell {"
                                          "  background: %1;"
                                          "  border: 1px solid %2;"
                                          "  border-radius: %3px;"
                                          "}")
                               .arg(p.window.name(), p.border.name())
                               .arg(kCornerRadius));

    m_shellLayout = new QVBoxLayout(m_shell);
    m_shellLayout->setContentsMargins(0, 0, 0, 0);
    m_shellLayout->setSpacing(0);

    m_titleBar = new TitleBar(m_shell);
    m_shellLayout->addWidget(m_titleBar);

    m_outerLayout->addWidget(m_shell);
    setCentralWidget(central);

    installEventFilter(this);
}

void AppWindow::setContent(QWidget *content)
{
    if (m_content) {
        m_shellLayout->removeWidget(m_content);
        m_content->deleteLater();
    }
    m_content = content;
    if (m_content)
        m_shellLayout->addWidget(m_content, 1);
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

void AppWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        applyMaximisedState();
}

void AppWindow::applyMaximisedState()
{
    const theme::Palette &p = theme::palette();
    const bool filling = isMaximized() || isFullScreen();

    // A window filling the screen has no corners to round and nowhere to cast
    // a shadow, so it should meet the screen edges exactly.
    const int margin = filling ? 0 : kShadowMargin;
    m_outerLayout->setContentsMargins(margin, margin, margin, margin);

    m_shell->setStyleSheet(QStringLiteral("QWidget#shell {"
                                          "  background: %1;"
                                          "  border: %2;"
                                          "  border-radius: %3px;"
                                          "}")
                               .arg(p.window.name(),
                                    filling ? QStringLiteral("none")
                                            : QStringLiteral("1px solid %1").arg(p.border.name()))
                               .arg(filling ? 0 : kCornerRadius));
    update();
}

void AppWindow::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);

    if (isMaximized() || isFullScreen())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);

    // A soft shadow, drawn as concentric rounded outlines fading outwards.
    // Cheaper than a blur and indistinguishable at this size.
    const QRectF panel = QRectF(m_shell->geometry());
    for (int i = kShadowMargin; i > 0; --i) {
        const qreal t = qreal(i) / kShadowMargin;
        QColor shade(0, 0, 0);
        shade.setAlphaF(0.16 * (1.0 - t) * (1.0 - t));
        painter.setPen(QPen(shade, 1.0));
        painter.drawRoundedRect(panel.adjusted(-i, -i + 1, i, i + 1),
                                kCornerRadius + i, kCornerRadius + i);
    }
}

Qt::Edges AppWindow::edgesAt(const QPoint &position) const
{
    // The grab band is the shadow margin: the only part of the window that no
    // child widget sits on top of.
    const int band = kShadowMargin;

    Qt::Edges edges;
    if (position.x() <= band)
        edges |= Qt::LeftEdge;
    if (position.x() >= width() - band)
        edges |= Qt::RightEdge;
    if (position.y() <= band)
        edges |= Qt::TopEdge;
    if (position.y() >= height() - band)
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
