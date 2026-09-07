#pragma once

#include "Document.h"
#include "Project.h"

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace mn::ui {

/// Runs Manim over the project's script, out of the way of the interface.
///
/// Rendering takes seconds to minutes, so it happens in a child process and
/// reports back as it goes: the window stays live, and the render can be
/// abandoned without killing anything else.
class RenderJob : public QObject
{
    Q_OBJECT

public:
    explicit RenderJob(QObject *parent = nullptr);
    ~RenderJob() override;

    bool isRunning() const;

    /// Start a render. `scriptPath` is the file to render and `sceneClass` the
    /// Scene subclass inside it.
    void start(const ProjectLayout &layout, const QString &scriptPath, const QString &sceneClass,
               const RenderSettings &settings);

    void cancel();

    /// The interpreter that will be used, and whether Manim is importable.
    static QString pythonExecutable();
    static bool manimAvailable(QString *versionOut = nullptr);

    /// Whether a LaTeX binary is on the path. Manim typesets formulae through
    /// it, and it is the one dependency that cannot be shipped with the app.
    static bool latexAvailable();

    /// Names of objects in `document` that will need LaTeX to render, so the
    /// user is told which ones rather than shown a Python traceback.
    static QStringList objectsNeedingLatex(const Document &document);

Q_SIGNALS:
    void started(const QString &commandLine);
    void output(const QString &line);
    void finished(bool success, const QString &message);

private:
    void readOutput();

    QProcess *m_process = nullptr;
    ProjectLayout m_layout;
    QString m_scriptPath;
    bool m_cancelled = false;
};

} // namespace mn::ui
