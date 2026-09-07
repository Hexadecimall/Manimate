#pragma once

#include "WindowButton.h"

#include <QWidget>

class QHBoxLayout;
class QLabel;

namespace mn::ui {

/// The window's own title bar, drawn by the application on every platform, so
/// its colours and its window controls match the rest of the interface instead
/// of the system frame.
///
/// Control placement still follows the host: discs on the left as on macOS,
/// glyph buttons on the right as on Windows and Linux. Dragging the empty area
/// moves the window and double-clicking it zooms, both delegated to the
/// compositor so the behaviour stays native.
class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);

    /// Add a widget to the bar's action area, left to right after the title.
    void addActionWidget(QWidget *widget);

    /// Add a widget hard against the far end of the bar.
    void addTrailingWidget(QWidget *widget);

    static constexpr int kHeight = 44;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void toggleMaximised();
    void setGroupHovered(bool hovered);

    /// The three window controls, grouped so hovering any of them lights all
    /// three, and hovering the rest of the bar does not.
    QWidget *m_controls = nullptr;
    QLabel *m_titleLabel = nullptr;
    QHBoxLayout *m_actionLayout = nullptr;
    QHBoxLayout *m_trailingLayout = nullptr;
    WindowButton *m_close = nullptr;
    WindowButton *m_minimize = nullptr;
    WindowButton *m_maximize = nullptr;
};

} // namespace mn::ui
