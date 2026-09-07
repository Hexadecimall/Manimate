#pragma once

#include <QList>
#include <QMainWindow>
#include <QPixmap>

class QMenu;
class QVBoxLayout;

namespace mn::ui {

class TitleBar;

/// Base window for the application: frameless on every platform, with rounded
/// corners, a soft drop shadow and a title bar the application draws itself.
///
/// Being frameless means the window has to provide what the system frame
/// normally would. The shadow and the rounded shape are painted here; the
/// margin they occupy doubles as the band that starts an edge resize, since it
/// is the one part of the window no child widget covers.
///
/// Menus are the one thing that changes by platform. On macOS a menu belongs in
/// the screen's menu bar, so setMenus() puts them there; on Windows and Linux
/// there is no such bar, so the same menus appear as buttons inside the title
/// bar. Callers build one set of menus and get the right result either way.
class AppWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AppWindow(QWidget *parent = nullptr);

    TitleBar *titleBar() const { return m_titleBar; }

    /// Fill the area below the title bar.
    void setContent(QWidget *content);

    /// Install the window's menus wherever this platform expects them.
    void setMenus(const QList<QMenu *> &menus);

    void setWindowTitle(const QString &title);

    /// True when menus go to a system-wide menu bar rather than the title bar.
    static bool usesNativeMenuBar();

    /// Margin round the window holding the shadow, and the band that starts a
    /// resize. Collapses to nothing while maximised.
    static constexpr int kShadowMargin = 10;

    /// Corner radius of the window itself.
    static constexpr int kCornerRadius = 10;

    /// Radius a widget touching the window's edge should use so its own
    /// corner follows the window's exactly. The shell draws no border, so
    /// there is nothing to inset for and the two radii are the same.
    static constexpr int kInnerCornerRadius = kCornerRadius;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyMaximisedState();

    /// Redraw the cached shadow. Done on resize only: a live graphics effect
    /// would re-blur the whole window on every repaint anywhere inside it.
    void rebuildShadow();

    QPixmap m_shadow;
    Qt::Edges edgesAt(const QPoint &position) const;
    static Qt::CursorShape cursorForEdges(Qt::Edges edges);

    /// The rounded panel everything is drawn inside.
    QWidget *m_shell = nullptr;
    QVBoxLayout *m_outerLayout = nullptr;
    QVBoxLayout *m_shellLayout = nullptr;
    TitleBar *m_titleBar = nullptr;
    QWidget *m_content = nullptr;
};

} // namespace mn::ui
