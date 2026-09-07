#pragma once

#include <QAbstractButton>

namespace mn::ui {

/// A window control drawn by hand so it matches the application's own theme
/// rather than the system frame, which a frameless window does not have.
///
/// Two appearances: the macOS traffic light (a coloured disc that only reveals
/// its glyph on hover) and the glyph button used everywhere else.
class WindowButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Kind { Close, Minimize, Maximize };
    enum class Appearance { Traffic, Glyph };

    /// The appearance that matches the host platform's conventions.
    static Appearance nativeAppearance();

    WindowButton(Kind kind, Appearance appearance, QWidget *parent = nullptr);

    /// Traffic lights light up together when any of them is hovered, so the
    /// title bar tells the group when the pointer is over it.
    void setGroupHovered(bool hovered);

    /// Swaps the maximise glyph for the restore glyph.
    void setRestoreState(bool restore);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void paintTraffic(QPainter &painter);
    void paintGlyph(QPainter &painter);

    Kind m_kind;
    Appearance m_appearance;
    bool m_groupHovered = false;
    bool m_restore = false;
};

} // namespace mn::ui
