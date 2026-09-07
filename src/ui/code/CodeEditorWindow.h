#pragma once

#include "AppWindow.h"

class QLabel;

namespace mn::ui {

class CodeEditor;

/// A window holding one Python file.
///
/// Deliberately small: open, edit, save. The editor itself is the interesting
/// part, and it is a widget, so when the project editor arrives this window can
/// go away without taking anything with it.
class CodeEditorWindow : public AppWindow
{
    Q_OBJECT

public:
    explicit CodeEditorWindow(QWidget *parent = nullptr);

    /// Load `filePath` into the editor. Returns false and reports why not.
    bool open(const QString &filePath);

    /// Save to the current path, asking for one if there is none yet.
    bool save();
    bool saveAs();

    CodeEditor *editor() const { return m_editor; }

    QString filePath() const { return m_filePath; }
    bool isModified() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildMenus();
    void updateTitle();
    void updatePosition(int line, int column);
    bool writeTo(const QString &filePath);

    /// Offer to save before discarding. False means the caller should stop.
    bool confirmDiscard();

    CodeEditor *m_editor = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
    QString m_filePath;
};

} // namespace mn::ui
