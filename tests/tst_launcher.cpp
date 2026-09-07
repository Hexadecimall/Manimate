#include "LauncherWindow.h"
#include "ProjectCardDelegate.h"
#include "Project.h"
#include "RecentProjects.h"
#include "RecentProjectsModel.h"
#include "AppWindow.h"
#include "CodeEditor.h"
#include "CodeGenerator.h"
#include "EditorState.h"
#include "SceneEvaluator.h"
#include "ProjectWindow.h"
#include "Theme.h"
#include "TitleBar.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QMenuBar>
#include <QToolButton>
#include <QDateTime>
#include <QDir>
#include <QLineEdit>
#include <QTest>
#include <QTreeWidget>
#include <QListView>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace mn;
using namespace mn::ui;

class LauncherTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void modelExposesEntriesNewestFirst();
    void modelFlagsMissingProjects();
    void filterMatchesNameDescriptionAndPath();
    void removingAnEntryDropsItFromSettings();
    void relativeTimesReadNaturally();
    void windowShowsSeededProjects();
    void searchNarrowsTheVisibleList();

    void menusGoWherePlatformExpects();

    void projectWindowOpensTheProjectsScript();
    void projectWindowSavesBackToTheScript();

    void snapshot();
    void projectSnapshot();
    void repaintIsFastEnoughForPlayback();
    void typingInATextFieldKeepsFocus();

    void generatesRunnablePython();
    void solverExpressesOverlapExactly();
    void addingAnObjectSelectsIt();
    void animationsChangeWhatTheCanvasWouldDraw();
    void undoRestoresWhatWasThere();

private:
    void seed(const QString &name, const QString &description, int minutesAgo);

    QTemporaryDir m_dir;
};

void LauncherTest::initTestCase()
{
    // Keep the test's recent-project list out of the real application's.
    QCoreApplication::setOrganizationName(QStringLiteral("Manimation"));
    QCoreApplication::setApplicationName(QStringLiteral("ManimationLauncherTest"));
    QVERIFY(m_dir.isValid());
}

void LauncherTest::init()
{
    RecentProjects::save({});
}

void LauncherTest::seed(const QString &name, const QString &description, int minutesAgo)
{
    ProjectLayout layout;
    QString error;
    if (!QDir(m_dir.filePath(name)).exists())
        QVERIFY2(project::create(m_dir.path(), name, &layout, &error), qPrintable(error));
    else
        layout = *project::resolve(m_dir.filePath(name));

    QVector<RecentProject> entries = RecentProjects::load();
    RecentProject entry;
    entry.projectFile = layout.projectFile;
    entry.name = name;
    entry.description = description;
    entry.lastOpened = QDateTime::currentDateTime().addSecs(-60 * minutesAgo);
    entries.append(entry);
    RecentProjects::save(entries);
}

void LauncherTest::modelExposesEntriesNewestFirst()
{
    seed(QStringLiteral("Alpha"), QStringLiteral("first"), 5);
    seed(QStringLiteral("Beta"), QStringLiteral("second"), 90);

    RecentProjectsModel model;
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(0, 0).data(RecentProjectsModel::NameRole).toString(), QStringLiteral("Alpha"));
    QCOMPARE(model.index(1, 0).data(RecentProjectsModel::DescriptionRole).toString(), QStringLiteral("second"));
    QVERIFY(!model.index(0, 0).data(RecentProjectsModel::MissingRole).toBool());
}

void LauncherTest::modelFlagsMissingProjects()
{
    QVector<RecentProject> entries;
    RecentProject ghost;
    ghost.projectFile = m_dir.filePath(QStringLiteral("Gone/Gone.manproj"));
    ghost.name = QStringLiteral("Gone");
    ghost.lastOpened = QDateTime::currentDateTime();
    entries.append(ghost);
    RecentProjects::save(entries);

    RecentProjectsModel model;
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.index(0, 0).data(RecentProjectsModel::MissingRole).toBool());

    model.reload(/*pruneMissing=*/true);
    QCOMPARE(model.rowCount(), 0);
}

void LauncherTest::filterMatchesNameDescriptionAndPath()
{
    seed(QStringLiteral("Fourier"), QStringLiteral("rotating vectors"), 1);
    seed(QStringLiteral("Vectors"), QStringLiteral("chapter one"), 2);

    RecentProjectsModel model;
    RecentProjectsFilter filter;
    filter.setSourceModel(&model);

    filter.setFilterFixedString(QStringLiteral("fourier"));
    QCOMPARE(filter.rowCount(), 1);

    filter.setFilterFixedString(QStringLiteral("vector"));
    QCOMPARE(filter.rowCount(), 2); // one by name, one by description

    filter.setFilterFixedString(QStringLiteral("chapter"));
    QCOMPARE(filter.rowCount(), 1);

    filter.setFilterFixedString(QString());
    QCOMPARE(filter.rowCount(), 2);
}

void LauncherTest::removingAnEntryDropsItFromSettings()
{
    seed(QStringLiteral("Keep"), QString(), 1);
    seed(QStringLiteral("Drop"), QString(), 2);

    RecentProjectsModel model;
    QCOMPARE(model.rowCount(), 2);
    model.removeAt(1);
    QCOMPARE(model.rowCount(), 1);

    RecentProjectsModel reloaded;
    QCOMPARE(reloaded.rowCount(), 1);
    QCOMPARE(reloaded.index(0, 0).data(RecentProjectsModel::NameRole).toString(), QStringLiteral("Keep"));
}

void LauncherTest::relativeTimesReadNaturally()
{
    const QDateTime now = QDateTime::currentDateTime();
    QCOMPARE(ProjectCardDelegate::describeTime(now), QStringLiteral("just now"));
    QCOMPARE(ProjectCardDelegate::describeTime(now.addSecs(-60 * 12)), QStringLiteral("12 minutes ago"));
    QCOMPARE(ProjectCardDelegate::describeTime(now.addSecs(-3600 * 3)), QStringLiteral("3 hours ago"));
    QCOMPARE(ProjectCardDelegate::describeTime(now.addDays(-2)), QStringLiteral("2 days ago"));
    QCOMPARE(ProjectCardDelegate::describeTime(now.addDays(-1)), QStringLiteral("yesterday"));
    QVERIFY(ProjectCardDelegate::describeTime({}).isEmpty());
}

void LauncherTest::windowShowsSeededProjects()
{
    seed(QStringLiteral("Alpha"), QStringLiteral("first"), 5);
    seed(QStringLiteral("Beta"), QStringLiteral("second"), 90);

    LauncherWindow window;
    auto *list = window.findChild<QListView *>();
    QVERIFY(list);
    QCOMPARE(list->model()->rowCount(), 2);
}

void LauncherTest::searchNarrowsTheVisibleList()
{
    seed(QStringLiteral("Fourier"), QStringLiteral("rotating vectors"), 1);
    seed(QStringLiteral("Backprop"), QStringLiteral("gradients"), 2);

    LauncherWindow window;
    auto *search = window.findChild<QLineEdit *>();
    auto *list = window.findChild<QListView *>();
    QVERIFY(search && list);

    search->setText(QStringLiteral("grad"));
    QCOMPARE(list->model()->rowCount(), 1);
    QCOMPARE(list->model()->index(0, 0).data(RecentProjectsModel::NameRole).toString(),
             QStringLiteral("Backprop"));
}

void LauncherTest::menusGoWherePlatformExpects()
{
    LauncherWindow window;

    auto *bar = window.findChild<TitleBar *>();
    QVERIFY(bar);
    const QList<QToolButton *> menuButtons = bar->findChildren<QToolButton *>();

    if (AppWindow::usesNativeMenuBar()) {
        // The menus belong to the screen's menu bar, not the window's own bar.
        QCOMPARE(menuButtons.size(), 0);
        QCOMPARE(window.menuBar()->actions().size(), 2);
    } else {
        QCOMPARE(menuButtons.size(), 2);
        QCOMPARE(menuButtons.at(0)->text(), QStringLiteral("File"));
        QCOMPARE(menuButtons.at(1)->text(), QStringLiteral("Help"));
    }
}

void LauncherTest::projectWindowOpensTheProjectsScript()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QString error;
    QVERIFY2(project::create(dir.path(), QStringLiteral("Wired Up"), &layout, &error), qPrintable(error));

    ProjectWindow window;
    QVERIFY(window.openProject(layout.projectFile));

    QCOMPARE(window.document().metadata.name, QStringLiteral("Wired Up"));
    QCOMPARE(QFileInfo(window.scriptPath()).fileName(), QStringLiteral("wired_up.py"));
    QVERIFY(window.editor()->toPlainText().contains(QStringLiteral("class WiredUp(Scene):")));
    QVERIFY(!window.isModified());
}

void LauncherTest::projectWindowSavesBackToTheScript()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QVERIFY(project::create(dir.path(), QStringLiteral("Saveable"), &layout, nullptr));

    ProjectWindow window;
    QVERIFY(window.openProject(layout.projectFile));

    window.editor()->insertPlainText(QStringLiteral("# edited\n"));
    QVERIFY(window.isModified());
    QVERIFY(window.save());
    QVERIFY(!window.isModified());

    QFile file(window.scriptPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(file.readAll()).contains(QStringLiteral("# edited")));
}

/// Writes a PNG of the launcher when MANIMATION_UI_SNAPSHOT names a path.
/// Skipped otherwise, so the suite stays headless-friendly.
void LauncherTest::snapshot()
{
    const QByteArray target = qgetenv("MANIMATION_UI_SNAPSHOT");
    if (target.isEmpty())
        QSKIP("MANIMATION_UI_SNAPSHOT is not set");

    seed(QStringLiteral("Fourier Series"), QStringLiteral("Building a square wave from rotating vectors"), 12);
    seed(QStringLiteral("Linear Algebra Intro"), QStringLiteral("Vectors, spans and basis, chapter 1"), 60 * 5);
    seed(QStringLiteral("Neural Net Backprop"), QString(), 60 * 26);
    seed(QStringLiteral("Bezier Curves"), QStringLiteral("De Casteljau, step by step"), 60 * 24 * 9);

    theme::apply(*qApp);

    LauncherWindow window;
    window.resize(1160, 720);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(250);

    // Keep stray keystrokes the window may have received out of the shot.
    if (auto *search = window.findChild<QLineEdit *>())
        search->clear();
    if (auto *list = window.findChild<QListView *>()) {
        list->setFocus();
        list->setCurrentIndex(list->model()->index(0, 0));
    }
    QTest::qWait(120);

    QVERIFY(window.grab().save(QString::fromUtf8(target)));
}

void LauncherTest::generatesRunnablePython()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Demo")), {});
    state.documentForWriting().sceneClassName = QStringLiteral("Demo");

    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    state.setObjectParam(circle, QStringLiteral("position"), QPointF(-2, 0));
    state.setObjectParam(circle, QStringLiteral("color"), QColor(0x58, 0xC4, 0xDD));

    const ObjectId label = state.addObject(QStringLiteral("manim.Text"));
    state.setObjectParam(label, QStringLiteral("text"), QStringLiteral("Hello"));

    const ClipId create = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(create, 0.0, 1.0, 0);
    const ClipId write = state.addClip(label, QStringLiteral("manim.Write"));
    state.setClipTiming(write, 0.5, 1.0, 1);
    const ClipId shift = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(shift, 2.0, 1.0, 0);
    state.setClipParam(shift, QStringLiteral("by"), QPointF(3, 0));

    const QString code = codegen::generate(state.document());

    QVERIFY(code.startsWith(QStringLiteral("from manim import *")));
    QVERIFY(code.contains(QStringLiteral("class Demo(Scene):")));
    QVERIFY(code.contains(QStringLiteral("def construct(self):")));

    // Constructors carry only what differs from Manim's own defaults.
    QVERIFY(code.contains(QStringLiteral("Circle(")));
    QVERIFY(code.contains(QStringLiteral("circle.move_to([-2, 0, 0])")));
    QVERIFY(code.contains(QStringLiteral("Text(\"Hello\"")));

    // The shift became an .animate call carrying its own timing.
    QVERIFY(code.contains(QStringLiteral(".animate(run_time=1).shift([3, 0, 0])")));

    // Nothing left of the placeholder that stood in for timing.
    QVERIFY(!code.contains(QStringLiteral("placeholder")));
}

void LauncherTest::solverExpressesOverlapExactly()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Overlap")), {});

    const ObjectId a = state.addObject(QStringLiteral("manim.Circle"));
    const ObjectId b = state.addObject(QStringLiteral("manim.Square"));

    // Two clips overlapping only partway: A runs 0-2s, B runs 0.5-1.5s.
    const ClipId first = state.addClip(a, QStringLiteral("manim.Create"));
    state.setClipTiming(first, 0.0, 2.0, 0);
    const ClipId second = state.addClip(b, QStringLiteral("manim.FadeIn"));
    state.setClipTiming(second, 0.5, 1.0, 1);

    const QString code = codegen::constructBody(state.document(), 0);

    // One play() holds both, and the later one waits out its offset first.
    QCOMPARE(code.count(QStringLiteral("self.play(")), 1);
    QVERIFY(code.contains(QStringLiteral("Succession(Wait(0.5), FadeIn(")));

    // A clip that starts a clear gap later gets its own play, after a wait.
    const ClipId third = state.addClip(a, QStringLiteral("manim.Shift"));
    state.setClipTiming(third, 5.0, 1.0, 0);

    const QString later = codegen::constructBody(state.document(), 0);
    QCOMPARE(later.count(QStringLiteral("self.play(")), 2);
    QVERIFY(later.contains(QStringLiteral("self.wait(3)")));
}

void LauncherTest::addingAnObjectSelectsIt()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Scratch")), {});

    const ObjectId id = state.addObject(QStringLiteral("manim.Circle"));
    QVERIFY(id != kInvalidObjectId);
    QCOMPARE(state.selectedObject(), id);
    QCOMPARE(state.document().objects.size(), 1);
    // Catalog defaults are filled in, so the object is drawable straight away.
    QCOMPARE(state.document().objects.first().params.value(QStringLiteral("radius")).toDouble(), 1.0);
}

void LauncherTest::animationsChangeWhatTheCanvasWouldDraw()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Scratch")), {});

    const ObjectId id = state.addObject(QStringLiteral("manim.Square"));
    state.setPlayhead(0.0);
    const ClipId clip = state.addClip(id, QStringLiteral("manim.Create"));
    QVERIFY(clip != kInvalidClipId);
    state.setClipTiming(clip, 1.0, 2.0, 0);

    // Before its entrance the object is not on screen at all.
    QVERIFY(evaluator::evaluate(state.document(), 0.5).isEmpty());

    // Halfway through Create it is partly drawn.
    const auto midway = evaluator::evaluate(state.document(), 2.0);
    QCOMPARE(midway.size(), 1);
    QVERIFY(midway.first().drawProgress > 0.0);
    QVERIFY(midway.first().drawProgress < 1.0);

    // Afterwards it stays, fully drawn.
    const auto after = evaluator::evaluate(state.document(), 5.0);
    QCOMPARE(after.size(), 1);
    QCOMPARE(after.first().drawProgress, 1.0);

    // A shift moves it, and the move persists once the clip has finished.
    const ClipId shift = state.addClip(id, QStringLiteral("manim.Shift"));
    state.setClipTiming(shift, 4.0, 1.0, 1);
    state.setClipParam(shift, QStringLiteral("by"), QPointF(2.0, 0.0));

    QCOMPARE(evaluator::evaluate(state.document(), 3.0).first().offset, QPointF(0, 0));
    QCOMPARE(evaluator::evaluate(state.document(), 6.0).first().offset, QPointF(2.0, 0.0));
}

void LauncherTest::undoRestoresWhatWasThere()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Scratch")), {});

    state.addObject(QStringLiteral("manim.Circle"));
    QCOMPARE(state.document().objects.size(), 1);
    QVERIFY(state.canUndo());

    state.undo();
    QCOMPARE(state.document().objects.size(), 0);
    QVERIFY(state.canRedo());

    state.redo();
    QCOMPARE(state.document().objects.size(), 1);
}

void LauncherTest::typingInATextFieldKeepsFocus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QVERIFY(project::create(dir.path(), QStringLiteral("Typing"), &layout, nullptr));

    ProjectWindow window;
    QVERIFY(window.openProject(layout.projectFile));

    EditorState *state = window.state();
    const ObjectId text = state->addObject(QStringLiteral("manim.Text"));
    state->selectObject(text);

    window.resize(1300, 820);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);

    // Find the inspector's text field: the one holding the object's own text.
    QLineEdit *field = nullptr;
    for (QLineEdit *candidate : window.findChildren<QLineEdit *>()) {
        if (candidate->text() == QStringLiteral("Text") && candidate->isVisible())
            field = candidate;
    }
    QVERIFY(field);

    field->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), field);

    // Every keystroke changes the document, which used to rebuild the panel
    // and destroy this very widget.
    QTest::keyClicks(field, QStringLiteral("Hello"));

    QCOMPARE(QApplication::focusWidget(), field);
    QCOMPARE(field->text(), QStringLiteral("TextHello"));
    QCOMPARE(state->document().findObject(text)->params.value(QStringLiteral("text")).toString(),
             QStringLiteral("TextHello"));
}

/// Playback repaints the whole window on every frame, so a frame has to cost
/// less than the frame budget or playback silently runs slow.
void LauncherTest::repaintIsFastEnoughForPlayback()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QVERIFY(project::create(dir.path(), QStringLiteral("Speed"), &layout, nullptr));

    ProjectWindow window;
    QVERIFY(window.openProject(layout.projectFile));

    EditorState *state = window.state();
    for (int i = 0; i < 12; ++i) {
        const ObjectId id = state->addObject(QStringLiteral("manim.Circle"));
        state->setObjectParam(id, QStringLiteral("position"),
                              QPointF(-5.0 + i * 0.9, (i % 3) - 1.0));
        const ClipId clip = state->addClip(id, QStringLiteral("manim.Create"));
        state->setClipTiming(clip, i * 0.2, 1.0, i % 4);
    }

    window.resize(1400, 880);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    constexpr int kFrames = 60;
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < kFrames; ++i) {
        state->setPlayhead(i * (1.0 / 60.0));
        window.grab();
    }
    const double msPerFrame = double(timer.elapsed()) / kFrames;

    qInfo("full-window repaint: %.2f ms per frame", msPerFrame);

    // 60fps leaves 16.7ms. A generous ceiling, so this fails on a real
    // regression rather than on a busy machine.
    QVERIFY2(msPerFrame < 16.0,
             qPrintable(QStringLiteral("%1 ms per frame is too slow for 60fps")
                            .arg(msPerFrame, 0, 'f', 2)));
}

/// Writes a PNG of a project window, so the wired-up editor can be looked at.
void LauncherTest::projectSnapshot()
{
    const QByteArray target = qgetenv("MANIMATION_PROJECT_SNAPSHOT");
    if (target.isEmpty())
        QSKIP("MANIMATION_PROJECT_SNAPSHOT is not set");

    // Its own directory: the launcher snapshot seeds projects into m_dir, and
    // a name that is already taken there would collide.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QString error;
    QVERIFY2(project::create(dir.path(), QStringLiteral("Fourier Series"), &layout, &error),
             qPrintable(error));

    theme::apply(*qApp);

    ProjectWindow window;
    QVERIFY(window.openProject(layout.projectFile));

    // Build a small scene so the editor has something to show.
    EditorState *state = window.state();
    const ObjectId circle = state->addObject(QStringLiteral("manim.Circle"));
    state->setObjectParam(circle, QStringLiteral("position"), QPointF(-3.2, 0.6));
    state->setObjectParam(circle, QStringLiteral("color"), QColor(0x58, 0xC4, 0xDD));
    state->setObjectParam(circle, QStringLiteral("fill_opacity"), 0.35);

    const ObjectId square = state->addObject(QStringLiteral("manim.Square"));
    state->setObjectParam(square, QStringLiteral("position"), QPointF(0.0, 0.6));
    state->setObjectParam(square, QStringLiteral("color"), QColor(0x83, 0xC1, 0x67));
    state->setObjectParam(square, QStringLiteral("rotation"), 15.0);

    const ObjectId star = state->addObject(QStringLiteral("manim.Star"));
    state->setObjectParam(star, QStringLiteral("position"), QPointF(3.2, 0.6));
    state->setObjectParam(star, QStringLiteral("color"), QColor(0xF0, 0xC2, 0x4B));
    state->setObjectParam(star, QStringLiteral("fill_opacity"), 0.5);

    const ObjectId title = state->addObject(QStringLiteral("manim.Text"));
    state->setObjectParam(title, QStringLiteral("text"), QStringLiteral("Fourier Series"));
    state->setObjectParam(title, QStringLiteral("position"), QPointF(0.0, -2.2));
    state->setObjectParam(title, QStringLiteral("font_size"), 54.0);

    state->setPlayhead(0.0);
    const ClipId c1 = state->addClip(circle, QStringLiteral("manim.Create"));
    state->setClipTiming(c1, 0.0, 1.2, 0);
    const ClipId c2 = state->addClip(square, QStringLiteral("manim.GrowFromCenter"));
    state->setClipTiming(c2, 0.6, 1.0, 1);
    const ClipId c3 = state->addClip(star, QStringLiteral("manim.FadeIn"));
    state->setClipTiming(c3, 1.2, 1.0, 2);
    const ClipId c4 = state->addClip(title, QStringLiteral("manim.Write"));
    state->setClipTiming(c4, 1.8, 1.4, 3);
    const ClipId c5 = state->addClip(square, QStringLiteral("manim.Rotate"));
    state->setClipTiming(c5, 3.0, 1.6, 1);
    const ClipId c6 = state->addClip(circle, QStringLiteral("manim.Shift"));
    state->setClipTiming(c6, 3.4, 1.2, 0);

    state->selectClip(c5);
    state->setPlayhead(3.9);


    window.resize(1420, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(250);

    QVERIFY(window.grab().save(QString::fromUtf8(target)));
}

QTEST_MAIN(LauncherTest)
#include "tst_launcher.moc"
