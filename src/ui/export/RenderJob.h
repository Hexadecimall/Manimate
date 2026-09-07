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

    /// The interpreter that will be used.
    ///
    /// The application ships its own Python with Manim already installed, so a
    /// render works on a machine that has neither. That copy is preferred; a
    /// system interpreter is the fallback, and a path set in settings beats
    /// both, for anyone who wants their own Manim version.
    static QString pythonExecutable();

    /// Where the bundled interpreter lives inside the installed application,
    /// or an empty string if this build has none.
    static QString bundledPython();

    /// True when the interpreter in use is the one shipped with the app.
    static bool usingBundledPython();

    /// The environment a render runs in. The bundled interpreter's own folder
    /// goes on PATH so Manim finds the ffmpeg shipped beside it.
    static QProcessEnvironment renderEnvironment();
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
