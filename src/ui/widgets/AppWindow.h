#pragma once

#include <QList>
#include <QMainWindow>

class QMenu;
class QVBoxLayout;

namespace mn::ui {

class TitleBar;

/// Base window for the application: frameless on every platform, with a title
/// bar the application draws itself so its colours and controls match the rest
/// of the interface.
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

    /// Width of the invisible band along each edge that starts a resize.
    static constexpr int kResizeMargin = 6;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Qt::Edges edgesAt(const QPoint &position) const;
    static Qt::CursorShape cursorForEdges(Qt::Edges edges);

    TitleBar *m_titleBar = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QWidget *m_content = nullptr;
};

} // namespace mn::ui
