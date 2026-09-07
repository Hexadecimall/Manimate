#include "Theme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>

namespace mn::theme {
namespace {

const Palette g_palette;

QString hex(const QColor &color)
{
    return color.name(QColor::HexRgb);
}

QString rgba(const QColor &color, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(alpha);
}

} // namespace

const Palette &palette()
{
    return g_palette;
}

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QFont font(int pointSize, QFont::Weight weight)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    f.setPointSize(pointSize);
    f.setWeight(weight);
    return f;
}

QString styleSheet()
{
    const Palette &p = palette();

    return QStringLiteral(R"(
QWidget {
    background: transparent;
    color: %(text)s;
}

/* Deliberately not QMainWindow: the main window is translucent so it can paint
   its own rounded shape and shadow, and an opaque background here would fill
   the corners back in. */
QDialog {
    background: %(window)s;
}

QToolTip {
    background: %(surfaceRaised)s;
    color: %(text)s;
    border: 1px solid %(border)s;
    border-radius: %(radiusSmall)dpx;
    padding: 5px 8px;
}

QLabel[role="title"] {
    font-size: 22px;
    font-weight: 600;
}

QLabel[role="subtitle"] {
    color: %(textMuted)s;
    font-size: 13px;
}

QLabel[role="section"] {
    color: %(textFaint)s;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1.4px;
}

QLabel[role="error"] {
    color: %(danger)s;
    font-size: 12px;
}

QPushButton {
    background: %(surfaceRaised)s;
    color: %(text)s;
    border: 1px solid %(border)s;
    border-radius: %(radius)dpx;
    padding: 8px 16px;
    font-size: 13px;
    min-height: 18px;
}
QPushButton:hover  { background: %(surfaceHover)s; border-color: %(borderStrong)s; }
QPushButton:pressed { background: %(surfaceActive)s; }
QPushButton:disabled { color: %(textFaint)s; border-color: %(surface)s; }

QPushButton[role="primary"] {
    background: %(accent)s;
    color: %(onAccent)s;
    border: none;
    font-weight: 600;
}
QPushButton[role="primary"]:hover   { background: %(accentHover)s; }
QPushButton[role="primary"]:pressed { background: %(accentPressed)s; }
QPushButton[role="primary"]:disabled { background: %(surfaceHover)s; color: %(textFaint)s; }

QPushButton[role="quiet"] {
    background: transparent;
    border: none;
    color: %(textMuted)s;
    padding: 6px 10px;
    text-align: left;
}
QPushButton[role="quiet"]:hover   { background: %(surfaceHover)s; color: %(text)s; }
QPushButton[role="quiet"]:pressed { background: %(surfaceActive)s; }

QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: %(surface)s;
    border: 1px solid %(border)s;
    border-radius: %(radiusSmall)dpx;
    padding: 4px 8px;
    selection-background-color: %(accentSel)s;
    selection-color: %(text)s;
    font-size: 13px;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: %(accent)s;
}
QLineEdit:disabled, QComboBox:disabled { color: %(textFaint)s; }
QLineEdit[state="invalid"] { border-color: %(danger)s; }

QComboBox::drop-down { border: none; width: 18px; }

/* The stepper arrows waste width in a narrow inspector; the field is still
   scrollable and typeable without them. */
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    width: 0px;
    border: none;
}
QComboBox QAbstractItemView {
    background: %(surfaceRaised)s;
    border: 1px solid %(border)s;
    border-radius: %(radius)dpx;
    padding: 4px;
    outline: none;
    selection-background-color: %(surfaceHover)s;
}

QListView, QTreeView, QTableView {
    background: transparent;
    border: none;
    outline: none;
}

QTreeView::item, QListView::item {
    border-radius: %(radiusSmall)dpx;
    padding: 3px 4px;
    margin: 1px 4px 1px 0px;
    color: %(text)s;
}
QTreeView::item:hover, QListView::item:hover {
    background: %(surfaceHover)s;
}
QTreeView::item:selected, QListView::item:selected {
    background: %(accentSoft)s;
    color: %(text)s;
}
QTreeView::item:disabled, QListView::item:disabled {
    color: %(textFaint)s;
}
QTreeView::branch { background: transparent; }

QListView::indicator, QTreeView::indicator, QCheckBox::indicator {
    width: 13px;
    height: 13px;
    border: 1px solid %(borderStrong)s;
    border-radius: 4px;
    background: %(surface)s;
}
QListView::indicator:hover, QTreeView::indicator:hover, QCheckBox::indicator:hover {
    border-color: %(accent)s;
}
QListView::indicator:checked, QTreeView::indicator:checked, QCheckBox::indicator:checked {
    background: %(accent)s;
    border-color: %(accent)s;
}

/* A panel's title strip: the same height everywhere, so the columns line up. */
QLabel[role="panelTitle"] {
    color: %(textMuted)s;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1.2px;
    padding: 0px;
}

QWidget[role="panelHeader"] {
    background: %(surfaceRaised)s;
    border-bottom: 1px solid %(border)s;
}

QWidget[role="group"] {
    background: %(surfaceRaised)s;
    border: 1px solid %(border)s;
    border-radius: %(radius)dpx;
}

QLabel[role="groupTitle"] {
    color: %(accent)s;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 1.1px;
}

QLabel[role="field"] {
    color: %(textMuted)s;
    font-size: 12px;
}

QSplitter::handle { background: %(border)s; }
QSplitter::handle:hover { background: %(accent)s; }

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: %(scroll)s;
    border-radius: 4px;
    min-height: 32px;
}
QScrollBar::handle:vertical:hover { background: %(scrollHover)s; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: %(scroll)s;
    border-radius: 4px;
    min-width: 32px;
}
QScrollBar::handle:horizontal:hover { background: %(scrollHover)s; }

QSlider::groove:horizontal {
    height: 3px;
    background: %(borderStrong)s;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: %(accent)s;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: %(text)s;
    width: 11px;
    height: 11px;
    margin: -4px 0;
    border-radius: 6px;
}
QSlider::handle:horizontal:hover { background: %(accent)s; }

QMenu {
    background: %(surfaceRaised)s;
    border: 1px solid %(border)s;
    border-radius: %(radius)dpx;
    padding: 5px;
}
QMenu::item { padding: 6px 22px 6px 12px; border-radius: %(radiusSmall)dpx; }
QMenu::item:selected { background: %(surfaceHover)s; }
QMenu::separator { height: 1px; background: %(border)s; margin: 5px 8px; }

QToolButton[role="menu"] {
    background: transparent;
    border: none;
    border-radius: %(radiusSmall)dpx;
    color: %(textMuted)s;
    padding: 5px 10px;
    font-size: 12px;
}
QToolButton[role="menu"]:hover { background: %(surfaceHover)s; color: %(text)s; }
QToolButton[role="menu"]:pressed,
QToolButton[role="menu"]:checked { background: %(surfaceActive)s; color: %(text)s; }
QToolButton[role="menu"]::menu-indicator { image: none; width: 0; }

QFrame[role="separator"] { background: %(border)s; border: none; }
)")
        .replace(QStringLiteral("%(window)s"), hex(p.window))
        .replace(QStringLiteral("%(surfaceRaised)s"), hex(p.surfaceRaised))
        .replace(QStringLiteral("%(surfaceHover)s"), hex(p.surfaceHover))
        .replace(QStringLiteral("%(surfaceActive)s"), hex(p.surfaceActive))
        .replace(QStringLiteral("%(surface)s"), hex(p.surface))
        .replace(QStringLiteral("%(borderStrong)s"), hex(p.borderStrong))
        .replace(QStringLiteral("%(border)s"), hex(p.border))
        .replace(QStringLiteral("%(textMuted)s"), hex(p.textMuted))
        .replace(QStringLiteral("%(textFaint)s"), hex(p.textFaint))
        .replace(QStringLiteral("%(text)s"), hex(p.text))
        .replace(QStringLiteral("%(accentHover)s"), hex(p.accentHover))
        .replace(QStringLiteral("%(accentPressed)s"), hex(p.accentPressed))
        .replace(QStringLiteral("%(accentSoft)s"), rgba(p.accent, 64))
        .replace(QStringLiteral("%(accentSel)s"), rgba(p.accent, 90))
        .replace(QStringLiteral("%(accent)s"), hex(p.accent))
        .replace(QStringLiteral("%(onAccent)s"), hex(p.onAccent))
        .replace(QStringLiteral("%(danger)s"), hex(p.danger))
        .replace(QStringLiteral("%(scrollHover)s"), hex(p.borderStrong))
        .replace(QStringLiteral("%(scroll)s"), hex(p.border))
        .replace(QStringLiteral("%(radiusSmall)d"), QString::number(kRadiusSmall))
        .replace(QStringLiteral("%(radius)d"), QString::number(kRadius));
}

void apply(QApplication &application)
{
    const Palette &p = palette();

    QPalette qtPalette;
    qtPalette.setColor(QPalette::Window, p.window);
    qtPalette.setColor(QPalette::WindowText, p.text);
    qtPalette.setColor(QPalette::Base, p.surface);
    qtPalette.setColor(QPalette::AlternateBase, p.surfaceRaised);
    qtPalette.setColor(QPalette::Text, p.text);
    qtPalette.setColor(QPalette::PlaceholderText, p.textFaint);
    qtPalette.setColor(QPalette::Button, p.surfaceRaised);
    qtPalette.setColor(QPalette::ButtonText, p.text);
    qtPalette.setColor(QPalette::Highlight, p.accent);
    qtPalette.setColor(QPalette::HighlightedText, p.onAccent);
    qtPalette.setColor(QPalette::ToolTipBase, p.surfaceRaised);
    qtPalette.setColor(QPalette::ToolTipText, p.text);
    qtPalette.setColor(QPalette::Link, p.accent);
    qtPalette.setColor(QPalette::Disabled, QPalette::Text, p.textFaint);
    qtPalette.setColor(QPalette::Disabled, QPalette::ButtonText, p.textFaint);
    qtPalette.setColor(QPalette::Disabled, QPalette::WindowText, p.textFaint);

    application.setPalette(qtPalette);
    application.setFont(font(13));
    application.setStyleSheet(styleSheet());
}

} // namespace mn::theme
