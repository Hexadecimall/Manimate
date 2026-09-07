#pragma once

#include <QWidget>

namespace mn::ui {

/// The application's logo: Manim's three primitives beside the wordmark.
///
/// Drawn rather than loaded from an image so it stays crisp at any scale and
/// on any display density, and so it can pick its colours from the theme.
class Wordmark : public QWidget
{
    Q_OBJECT

public:
    explicit Wordmark(QWidget *parent = nullptr);

    /// Height of the glyph row; the text scales with it.
    void setGlyphSize(int pixels);
    void setSubtitle(const QString &subtitle);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_glyphSize = 34;
    QString m_subtitle;
};

} // namespace mn::ui
