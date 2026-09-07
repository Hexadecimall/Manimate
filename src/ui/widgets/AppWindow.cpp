#include "AppWindow.h"

#include "MacWindow.h"
#include "Theme.h"
#include "TitleBar.h"

#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
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
    // No border. A border plus the inset needed to keep children off it left a
    // ring of the shell's own background showing between the two, which is a
    // second edge by another name. The shadow separates the window from what
    // is behind it; the panels' own colours do the rest.
    m_shell->setStyleSheet(QStringLiteral("QWidget#shell {"
                                          "  background: %1;"
                                          "  border: none;"
                                          "  border-radius: %2px;"
                                          "}")
                               .arg(p.window.name())
                               .arg(kCornerRadius));

    m_shellLayout = new QVBoxLayout(m_shell);
    // Children run to the edge: whatever sits in a corner rounds it itself,
    // at the window's own radius.
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

    rebuildShadow();

    // A window filling the screen has no corners to round and nowhere to cast
    // a shadow, so it should meet the screen edges exactly.
    const int margin = filling ? 0 : kShadowMargin;
    m_outerLayout->setContentsMargins(margin, margin, margin, margin);

    m_shell->setStyleSheet(QStringLiteral("QWidget#shell {"
                                          "  background: %1;"
                                          "  border: none;"
                                          "  border-radius: %2px;"
                                          "}")
                               .arg(p.window.name())
                               .arg(filling ? 0 : kCornerRadius));
    update();
}

void AppWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // Done once the window exists natively, and only then.
    if (!m_nativeShadowDisabled) {
        m_nativeShadowDisabled = true;
        mac::disableNativeShadow(this);
    }
    rebuildShadow();
}

void AppWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    rebuildShadow();
}

/// Blur one channel-agnostic pass over the image, horizontally then
/// vertically. Three passes of a box blur approximate a Gaussian closely
/// enough that nothing about the result looks boxy.
static void boxBlur(QImage &image, int radius)
{
    if (radius < 1)
        return;

    const int width = image.width();
    const int height = image.height();
    const int span = radius * 2 + 1;

    QImage scratch = image;

    for (int pass = 0; pass < 3; ++pass) {
        // Horizontal.
        for (int y = 0; y < height; ++y) {
            const QRgb *in = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            QRgb *out = reinterpret_cast<QRgb *>(scratch.scanLine(y));
            for (int x = 0; x < width; ++x) {
                int a = 0;
                for (int k = -radius; k <= radius; ++k)
                    a += qAlpha(in[std::clamp(x + k, 0, width - 1)]);
                out[x] = qRgba(0, 0, 0, a / span);
            }
        }
        // Vertical.
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                int a = 0;
                for (int k = -radius; k <= radius; ++k) {
                    const QRgb *line = reinterpret_cast<const QRgb *>(
                        scratch.constScanLine(std::clamp(y + k, 0, height - 1)));
                    a += qAlpha(line[x]);
                }
                reinterpret_cast<QRgb *>(image.scanLine(y))[x] = qRgba(0, 0, 0, a / span);
            }
        }
    }
}

void AppWindow::rebuildShadow()
{
    if (isMaximized() || isFullScreen() || m_shell->geometry().isEmpty() || size().isEmpty()) {
        m_shadow = {};
        return;
    }

    QImage image(size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 190));
        // Cast from slightly above, as a window's shadow is.
        painter.drawRoundedRect(QRectF(m_shell->geometry()).adjusted(1, 3, -1, 3),
                                kCornerRadius, kCornerRadius);
    }

    boxBlur(image, kShadowMargin / 3);
    m_shadow = QPixmap::fromImage(image);
}

void AppWindow::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);

    if (m_shadow.isNull())
        return;

    // Just a blit: the shadow only changes when the window is resized.
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_shadow);
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
