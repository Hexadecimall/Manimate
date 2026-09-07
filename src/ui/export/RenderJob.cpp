#include "RenderJob.h"

#include "Catalog.h"

#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QStandardPaths>

namespace mn::ui {
namespace {

/// Manim writes into media/videos/<module>/<height>p<fps>/<Scene>.mp4; the
/// project wants the file in output/ under its own name.
QString renderedFile(const QString &mediaDir, const QString &sceneClass)
{
    QDirIterator it(mediaDir, {QStringLiteral("%1.mp4").arg(sceneClass)}, QDir::Files,
                    QDirIterator::Subdirectories);
    QString newest;
    QDateTime newestTime;
    while (it.hasNext()) {
        const QString path = it.next();
        const QDateTime modified = QFileInfo(path).lastModified();
        if (newest.isEmpty() || modified > newestTime) {
            newest = path;
            newestTime = modified;
        }
    }
    return newest;
}

} // namespace

RenderJob::RenderJob(QObject *parent)
    : QObject(parent)
{
}

RenderJob::~RenderJob()
{
    cancel();
}

QString RenderJob::pythonExecutable()
{
    for (const QString &candidate : {QStringLiteral("python3"), QStringLiteral("python")}) {
        const QString found = QStandardPaths::findExecutable(candidate);
        if (!found.isEmpty())
            return found;
    }
    return {};
}

bool RenderJob::manimAvailable(QString *versionOut)
{
    const QString python = pythonExecutable();
    if (python.isEmpty())
        return false;

    QProcess probe;
    probe.start(python, {QStringLiteral("-c"),
                         QStringLiteral("import manim; print(manim.__version__)")});
    if (!probe.waitForFinished(8000))
        return false;
    if (probe.exitCode() != 0)
        return false;

    if (versionOut)
        *versionOut = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    return true;
}

bool RenderJob::latexAvailable()
{
    for (const QString &binary : {QStringLiteral("latex"), QStringLiteral("xelatex"),
                                  QStringLiteral("pdflatex")}) {
        if (!QStandardPaths::findExecutable(binary).isEmpty())
            return true;
    }
    return false;
}

QStringList RenderJob::objectsNeedingLatex(const Document &document)
{
    const QStringList flagged = catalog::latexDependentIds();

    QStringList names;
    for (const SceneObject &object : document.objects) {
        if (object.visible && flagged.contains(object.type))
            names.append(object.name);
    }
    names.removeDuplicates();
    return names;
}

bool RenderJob::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void RenderJob::start(const ProjectLayout &layout, const QString &scriptPath,
                      const QString &sceneClass, const RenderSettings &settings)
{
    if (isRunning())
        return;

    m_layout = layout;
    m_scriptPath = scriptPath;
    m_cancelled = false;

    const QString python = pythonExecutable();
    if (python.isEmpty()) {
        Q_EMIT finished(false, tr("No Python interpreter was found on this system."));
        return;
    }

    const QStringList arguments = {
        QStringLiteral("-m"),
        QStringLiteral("manim"),
        QStringLiteral("render"),
        QStringLiteral("--quality"),
        settings.quality,
        QStringLiteral("--frame_rate"),
        QString::number(settings.fps),
        QStringLiteral("--resolution"),
        QStringLiteral("%1,%2").arg(settings.width).arg(settings.height),
        QStringLiteral("--media_dir"),
        m_layout.intermediateDir,
        scriptPath,
        sceneClass,
    };

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_layout.root);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyRead, this, &RenderJob::readOutput);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            Q_EMIT finished(false, tr("Could not start Python."));
    });
    connect(m_process, &QProcess::finished, this,
            [this, sceneClass](int exitCode, QProcess::ExitStatus status) {
                readOutput();

                if (m_cancelled) {
                    Q_EMIT finished(false, tr("Render cancelled."));
                    return;
                }
                if (status != QProcess::NormalExit || exitCode != 0) {
                    Q_EMIT finished(false, tr("Manim exited with code %1.").arg(exitCode));
                    return;
                }

                // Move the result into the project's own output folder.
                const QString produced = renderedFile(m_layout.intermediateDir, sceneClass);
                if (produced.isEmpty()) {
                    Q_EMIT finished(false, tr("Manim finished but produced no video."));
                    return;
                }

                QDir().mkpath(m_layout.outputDir);
                const QString target =
                    QDir(m_layout.outputDir)
                        .filePath(QFileInfo(m_layout.projectFile).completeBaseName()
                                  + QStringLiteral(".mp4"));
                QFile::remove(target);
                if (!QFile::copy(produced, target)) {
                    Q_EMIT finished(true, tr("Rendered to %1").arg(QDir::toNativeSeparators(produced)));
                    return;
                }
                Q_EMIT finished(true, tr("Rendered to %1").arg(QDir::toNativeSeparators(target)));
            });

    Q_EMIT started(python + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
    m_process->start(python, arguments);
}

void RenderJob::cancel()
{
    if (!isRunning())
        return;
    m_cancelled = true;
    m_process->terminate();
    if (!m_process->waitForFinished(3000))
        m_process->kill();
}

void RenderJob::readOutput()
{
    if (!m_process)
        return;
    const QString text = QString::fromUtf8(m_process->readAll());
    for (const QString &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        Q_EMIT output(line.trimmed());
}

} // namespace mn::ui
