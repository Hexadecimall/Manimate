#pragma once

#include <QAbstractButton>

namespace mn::ui {

/// A window control drawn by the application in its own visual language.
///
/// Not an imitation of the system's controls: a quiet glyph on no background
/// until the pointer arrives, then a rounded panel behind it — red for close,
/// neutral for the other two. That keeps the title bar calm at rest, where
/// three coloured discs would be the loudest thing on the screen, and makes
/// what each button does legible at a glance rather than by memorised colour.
class WindowButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Kind { Close, Minimize, Maximize };

    WindowButton(Kind kind, QWidget *parent = nullptr);

    /// The whole cluster reacts together, so the controls read as one group.
    void setGroupHovered(bool hovered);

    /// Swaps the maximise glyph for the restore glyph.
    void setRestoreState(bool restore);

    /// True when the platform puts the controls at the left of the title bar.
    static bool controlsBelongOnTheLeft();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Kind m_kind;
    bool m_groupHovered = false;
    bool m_restore = false;
};

} // namespace mn::ui
