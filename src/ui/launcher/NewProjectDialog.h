#pragma once

#include "Project.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace mn::ui {

/// Collects everything needed to scaffold a project, and creates it on accept.
///
/// The dialog shows the exact folder it is about to create before you commit,
/// because creating a project makes a whole directory tree on disk rather than
/// a single file.
class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    /// `importSource`, when given, is a .py file the new project starts from.
    explicit NewProjectDialog(QWidget *parent = nullptr, const QString &importSource = {});

    /// Ask for a .py file and return its path, or an empty string if the user
    /// backed out. The caller passes the result to the constructor.
    static QString askForPythonFile(QWidget *parent);

    /// Valid only after the dialog was accepted.
    ProjectLayout layout() const { return m_layout; }

    /// Directory new projects default into. Remembered between runs.
    static QString defaultLocation();
    static void setDefaultLocation(const QString &path);

private:
    void browseForLocation();
    void refreshPreview();
    void createProject();

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_locationEdit = nullptr;
    QLineEdit *m_descriptionEdit = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QLabel *m_previewLabel = nullptr;
    QString m_importSource;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_createButton = nullptr;

    ProjectLayout m_layout;
};

} // namespace mn::ui
