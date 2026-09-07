#include "LauncherWindow.h"
#include "ProjectCardDelegate.h"
#include "Project.h"
#include "RecentProjects.h"
#include "RecentProjectsModel.h"
#include "AppWindow.h"
#include "CodeEditor.h"
#include "Catalog.h"
#include "CodeGenerator.h"
#include "CodeParser.h"
#include "EditorState.h"
#include "SceneEvaluator.h"
#include "SceneRenderer.h"
#include "TimelineView.h"
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
#include <QListWidget>
#include <QTreeWidget>
#include <QListView>
#include <QTemporaryDir>
#include <QTest>
#include <QListWidget>
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
    void positionCentresTheBoundingBox();
    void cameraMatchesManimsOwnProjection();
    void solidsMakeItAThreeDScene();
    void pythonBlocksBecomeControlFlow();
    void groupsCarryTheirChildren();
    void tracksCanBeAddedAndRemoved();
    void listsDrawTheirOwnSelection();
    void severalObjectsCanBeSelectedAndMovedTogether();
    void theWindowRemembersHowItWasLeft();
    void audioBecomesAddSound();
    void codeRoundTripsBackIntoTheScene();
    void parserReportsWhatItCannotRead();
    void catalogCoversABroadRangeOfManim();
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
    QCoreApplication::setOrganizationName(QStringLiteral("Manimate"));
    QCoreApplication::setApplicationName(QStringLiteral("ManimateLauncherTest"));
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

/// Writes a PNG of the launcher when MANIMATE_UI_SNAPSHOT names a path.
/// Skipped otherwise, so the suite stays headless-friendly.
void LauncherTest::snapshot()
{
    const QByteArray target = qgetenv("MANIMATE_UI_SNAPSHOT");
    if (target.isEmpty())
        QSKIP("MANIMATE_UI_SNAPSHOT is not set");

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

void LauncherTest::positionCentresTheBoundingBox()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Centring")), {});

    // A triangle's circumcentre sits above its bounding box's centre, so it is
    // the shape that shows whether position means one or the other.
    const ObjectId triangle = state.addObject(QStringLiteral("manim.Triangle"));
    const ObjectId square = state.addObject(QStringLiteral("manim.Square"));
    for (const ObjectId id : {triangle, square})
        state.setObjectParam(id, QStringLiteral("position"), QPointF(0.0, 0.0));

    const auto states = evaluator::evaluate(state.document(), 0.0);
    QCOMPARE(states.size(), 2);

    for (const evaluator::ObjectState &object : states) {
        const QPainterPath shape = SceneRenderer::shapeOf(object);
        const QRectF placed = SceneRenderer::transformOf(object, shape).map(shape).boundingRect();
        QVERIFY2(qAbs(placed.center().y()) < 1e-6,
                 qPrintable(QStringLiteral("%1 centred at y=%2, not 0")
                                .arg(object.type)
                                .arg(placed.center().y())));
    }

    // Measured against real Manim: Triangle().move_to(ORIGIN) spans -0.75 to
    // 0.75, being 1.5 units tall with its box centred. The canvas must agree.
    for (const evaluator::ObjectState &object : states) {
        if (object.type != QLatin1String("manim.Triangle"))
            continue;
        const QPainterPath shape = SceneRenderer::shapeOf(object);
        const QRectF placed = SceneRenderer::transformOf(object, shape).map(shape).boundingRect();
        QVERIFY2(qAbs(placed.height() - 1.5) < 1e-6,
                 qPrintable(QStringLiteral("triangle is %1 units tall, expected 1.5")
                                .arg(placed.height())));
    }

    // And the generated Python says the same thing, so the render agrees with
    // the canvas rather than placing the triangle somewhere else.
    const QString code = codegen::constructBody(state.document(), 0);
    QVERIFY(code.contains(QStringLiteral("triangle.move_to([0, 0, 0])")));

    // A line is positioned by its endpoints, so its position shifts instead.
    const ObjectId line = state.addObject(QStringLiteral("manim.Line"));
    state.setObjectParam(line, QStringLiteral("position"), QPointF(2.0, 1.0));
    const QString withLine = codegen::constructBody(state.document(), 0);
    QVERIFY(withLine.contains(QStringLiteral("line.shift([2, 1, 0])")));
    QVERIFY(!withLine.contains(QStringLiteral("line.move_to")));
}

void LauncherTest::cameraMatchesManimsOwnProjection()
{
    // Measured from a real render: markers placed at (2,0,0), (0,2,0) and
    // (0,0,2) with the camera at phi=70, theta=-45, then located in the image.
    // Manim's camera is perspective, so these are deliberately not symmetric.
    Camera3D camera;
    camera.enabled = true;
    camera.phi = 70.0;
    camera.theta = -45.0;

    const struct {
        double x, y, z;
        QPointF expected;
    } samples[] = {
        {2, 0, 0, QPointF(1.5181, -0.5279)},
        {0, 2, 0, QPointF(1.3347, 0.4408)},
        {0, 0, 2, QPointF(0.0075, 1.9362)},
    };

    for (const auto &sample : samples) {
        const QPointF mine = SceneRenderer::project(camera, sample.x, sample.y, sample.z);
        const double error = QLineF(mine, sample.expected).length();
        QVERIFY2(error < 0.03,
                 qPrintable(QStringLiteral("(%1,%2,%3): got (%4,%5), Manim gives (%6,%7)")
                                .arg(sample.x).arg(sample.y).arg(sample.z)
                                .arg(mine.x()).arg(mine.y())
                                .arg(sample.expected.x()).arg(sample.expected.y())));
    }

    // A flat camera leaves the scene exactly as it was, so a 2D project is
    // untouched by any of this.
    Camera3D flat;
    QCOMPARE(SceneRenderer::project(flat, 1.5, -2.5, 3.0), QPointF(1.5, -2.5));
}

void LauncherTest::solidsMakeItAThreeDScene()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Solid")), {});
    state.documentForWriting().sceneClassName = QStringLiteral("Solid");

    // A flat scene stays a plain Scene.
    state.addObject(QStringLiteral("manim.Circle"));
    QVERIFY(codegen::generate(state.document()).contains(QStringLiteral("class Solid(Scene):")));

    // One solid is enough to need the three-dimensional camera.
    state.addObject(QStringLiteral("manim.Cube"));
    const QString code = codegen::generate(state.document());
    QVERIFY(code.contains(QStringLiteral("class Solid(ThreeDScene):")));
    QVERIFY(code.contains(QStringLiteral("self.set_camera_orientation(")));

    // Turning the camera on carries its angles through.
    Camera3D camera;
    camera.enabled = true;
    camera.phi = 65.0;
    camera.theta = -30.0;
    camera.ambientRotation = true;
    camera.rotationRate = 0.15;
    state.setCamera(camera);

    const QString turned = codegen::generate(state.document());
    QVERIFY(turned.contains(QStringLiteral("phi=65 * DEGREES")));
    QVERIFY(turned.contains(QStringLiteral("theta=-30 * DEGREES")));
    QVERIFY(turned.contains(QStringLiteral("self.begin_ambient_camera_rotation(rate=0.15)")));
    QVERIFY(turned.contains(QStringLiteral("self.stop_ambient_camera_rotation()")));
}

void LauncherTest::pythonBlocksBecomeControlFlow()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Logic")), {});

    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    const ClipId create = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(create, 0.0, 1.0, 0);

    const ClipId block = state.addClip(circle, QStringLiteral("manim.Code"));
    state.setClipTiming(block, 2.0, 0.5, 1);
    state.setClipParam(block, QStringLiteral("body"),
                       QStringLiteral("for i in range(3):\n"
                                      "    if i % 2 == 0:\n"
                                      "        self.play(Indicate(TARGET))"));

    const QString code = codegen::constructBody(state.document(), 0);

    // The loop and the condition survive verbatim, indented into construct().
    QVERIFY(code.contains(QStringLiteral("for i in range(3):")));
    QVERIFY(code.contains(QStringLiteral("if i % 2 == 0:")));

    // TARGET is replaced by the variable the block is attached to.
    QVERIFY(code.contains(QStringLiteral("self.play(Indicate(circle))")));
    QVERIFY(!code.contains(QStringLiteral("TARGET")));

    // A block is not an animation, so it is not wrapped in a play() of its own.
    QVERIFY(!code.contains(QStringLiteral("self.play(Code")));
}

void LauncherTest::groupsCarryTheirChildren()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Grouping")), {});

    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    const ObjectId square = state.addObject(QStringLiteral("manim.Square"));
    state.setObjectParam(circle, QStringLiteral("position"), QPointF(-1, 0));
    state.setObjectParam(square, QStringLiteral("position"), QPointF(1, 0));

    const ObjectId group = state.groupObjects({circle, square});
    QVERIFY(group != kInvalidObjectId);
    QCOMPARE(state.document().findObject(circle)->parentId, group);

    // Moving the group moves what it holds, without touching their own values.
    state.setObjectParam(group, QStringLiteral("position"), QPointF(0, 2));
    const evaluator::ObjectState child =
        evaluator::evaluateObject(state.document(), circle, 0.0);
    QCOMPARE(child.offset, QPointF(0, 2));
    QCOMPARE(state.document().findObject(circle)->params.value(QStringLiteral("position")).toPointF(),
             QPointF(-1, 0));

    // The group is written after its members, and adds them itself.
    const QString code = codegen::constructBody(state.document(), 0);
    const int circleAt = int(code.indexOf(QStringLiteral("circle = Circle")));
    const int groupAt = int(code.indexOf(QStringLiteral("group = VGroup(")));
    QVERIFY(circleAt >= 0 && groupAt > circleAt);
    QVERIFY(code.contains(QStringLiteral("group = VGroup(circle, square)")));
    QVERIFY(code.contains(QStringLiteral("self.add(group)")));
    QVERIFY(!code.contains(QStringLiteral("self.add(circle")));

    // Deleting the group takes its children with it.
    state.removeObject(group);
    QCOMPARE(state.document().objects.size(), 0);
}

void LauncherTest::tracksCanBeAddedAndRemoved()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Tracks")), {});

    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    state.addTrack();
    state.addTrack();
    QCOMPARE(state.document().timeline.tracks.size(), 3);

    const ClipId onFirst = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(onFirst, 0.0, 1.0, 0);
    const ClipId onSecond = state.addClip(circle, QStringLiteral("manim.FadeIn"));
    state.setClipTiming(onSecond, 2.0, 1.0, 1);
    const ClipId onThird = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(onThird, 4.0, 1.0, 2);

    state.renameTrack(1, QStringLiteral("Middle"));
    QCOMPARE(state.document().timeline.tracks.at(1).name, QStringLiteral("Middle"));

    // Removing a track takes its clips and pulls the ones below it up, so no
    // clip is left naming a lane that no longer exists.
    state.removeTrack(1);
    QCOMPARE(state.document().timeline.tracks.size(), 2);
    QCOMPARE(state.document().timeline.clips.size(), 2);
    QVERIFY(!state.document().findClip(onSecond));
    QCOMPARE(state.document().findClip(onFirst)->track, 0);
    QCOMPARE(state.document().findClip(onThird)->track, 1);

    for (const Clip &clip : state.document().timeline.clips)
        QVERIFY(clip.track < state.document().timeline.tracks.size());
}

void LauncherTest::listsDrawTheirOwnSelection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QVERIFY(project::create(dir.path(), QStringLiteral("Selection"), &layout, nullptr));

    ProjectWindow window;
    QVERIFY(window.openProject(layout.projectFile));

    // The stylesheet paints each row. Anything the style would paint beside it,
    // in the indentation column, comes from the palette's Highlight — which
    // showed as a square block next to the rounded selection. It has to stay
    // invisible in every list that styles its own rows.
    const auto trees = window.findChildren<QTreeWidget *>();
    QVERIFY(!trees.isEmpty());
    for (const QTreeWidget *tree : trees) {
        QCOMPARE(tree->palette().color(QPalette::Highlight).alpha(), 0);
        QCOMPARE(tree->palette().color(QPalette::Inactive, QPalette::Highlight).alpha(), 0);
    }

    const auto lists = window.findChildren<QListWidget *>();
    QVERIFY(!lists.isEmpty());
    for (const QListWidget *list : lists)
        QCOMPARE(list->palette().color(QPalette::Highlight).alpha(), 0);
}

void LauncherTest::theWindowRemembersHowItWasLeft()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QVERIFY(project::create(dir.path(), QStringLiteral("Remembered"), &layout, nullptr));

    // Leave a window in a particular state and close it.
    {
        ProjectWindow window;
        QVERIFY(window.openProject(layout.projectFile));
        window.resize(1234, 806);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        window.showPage(ProjectWindow::Page::Export);
        window.findChild<TimelineView *>()->setScale(210.0);
        window.close();
    }

    // A new one comes back the same way.
    {
        ProjectWindow window;
        QVERIFY(window.openProject(layout.projectFile));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QCOMPARE(window.currentPage(), ProjectWindow::Page::Export);
        QCOMPARE(window.findChild<TimelineView *>()->scale(), 210.0);
        QCOMPARE(window.size(), QSize(1234, 806));
    }
}

void LauncherTest::severalObjectsCanBeSelectedAndMovedTogether()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Many")), {});

    const ObjectId a = state.addObject(QStringLiteral("manim.Circle"));
    const ObjectId b = state.addObject(QStringLiteral("manim.Square"));
    const ObjectId c = state.addObject(QStringLiteral("manim.Star"));
    state.setObjectParam(a, QStringLiteral("position"), QPointF(-2, 0));
    state.setObjectParam(b, QStringLiteral("position"), QPointF(0, 0));
    state.setObjectParam(c, QStringLiteral("position"), QPointF(2, 0));

    state.setSelection({a, b});
    QCOMPARE(state.selectedObjects().size(), 2);
    QVERIFY(state.isSelected(a));
    QVERIFY(!state.isSelected(c));
    // The inspector edits the last one chosen.
    QCOMPARE(state.selectedObject(), b);

    state.toggleSelected(c);
    QCOMPARE(state.selectedObjects().size(), 3);
    state.toggleSelected(a);
    QVERIFY(!state.isSelected(a));

    // Selecting one thing replaces the whole selection.
    state.selectObject(a);
    QCOMPARE(state.selectedObjects(), QVector<ObjectId>{a});

    // Deleting with several selected removes all of them.
    state.setSelection({b, c});
    state.deleteSelection();
    QCOMPARE(state.document().objects.size(), 1);
    QCOMPARE(state.document().objects.first().id, a);

    // A selection naming something deleted does not keep pointing at it.
    QVERIFY(!state.isSelected(b));
}

void LauncherTest::audioBecomesAddSound()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QVERIFY(project::create(dir.path(), QStringLiteral("Sound"), &layout, nullptr));

    // A file to add. Its contents do not matter; it is only ever copied.
    const QString source = QDir(dir.path()).filePath(QStringLiteral("chime.wav"));
    QFile file(source);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("RIFF----WAVEfmt ");
    file.close();

    Document document;
    QVERIFY(Document::load(layout.projectFile, &document, nullptr));

    EditorState state;
    state.setDocument(std::move(document), layout);
    state.setPlayhead(1.5);

    const ClipId audio = state.addAudio(source);
    QVERIFY(audio != kInvalidClipId);

    // Copied into the project, so the project carries its own sound.
    QVERIFY(QFileInfo(QDir(layout.assetsDir).filePath(QStringLiteral("chime.wav"))).isFile());

    state.setAudioGain(audio, -6.0);

    const QString code = codegen::constructBody(state.document(), 0);
    QVERIFY(code.contains(
        QStringLiteral("self.add_sound(\"assets/chime.wav\", time_offset=1.5, gain=-6)")));

    state.removeAudio(audio);
    QVERIFY(!codegen::constructBody(state.document(), 0).contains(QStringLiteral("add_sound")));
}

void LauncherTest::codeRoundTripsBackIntoTheScene()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Trip")), {});
    state.documentForWriting().sceneClassName = QStringLiteral("Trip");

    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    state.setObjectParam(circle, QStringLiteral("position"), QPointF(-2, 1));
    state.setObjectParam(circle, QStringLiteral("radius"), 1.5);
    state.setObjectParam(circle, QStringLiteral("color"), QColor(0x58, 0xC4, 0xDD));

    const ObjectId label = state.addObject(QStringLiteral("manim.Text"));
    state.setObjectParam(label, QStringLiteral("text"), QStringLiteral("Round trip"));
    state.setObjectParam(label, QStringLiteral("position"), QPointF(0, -2));

    const ClipId create = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(create, 0.0, 1.2, 0);
    const ClipId write = state.addClip(label, QStringLiteral("manim.Write"));
    state.setClipTiming(write, 0.5, 1.0, 1);
    const ClipId shift = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(shift, 3.0, 0.8, 0);
    state.setClipParam(shift, QStringLiteral("by"), QPointF(2.5, 0));

    const QString original = codegen::generate(state.document());

    // Read it back into a fresh document.
    Document rebuilt = Document::createNew(QStringLiteral("Trip"));
    const codeparser::Result outcome = codeparser::parse(original, &rebuilt);
    QVERIFY2(outcome.ok(), qPrintable(outcome.error));
    QVERIFY2(outcome.complete,
             qPrintable(QStringLiteral("did not understand: %1")
                            .arg(outcome.unrecognised.join(QStringLiteral(" | ")))));

    QCOMPARE(rebuilt.objects.size(), 2);
    QCOMPARE(rebuilt.timeline.clips.size(), 3);

    const SceneObject *readCircle = nullptr;
    for (const SceneObject &object : rebuilt.objects) {
        if (object.type == QLatin1String("manim.Circle"))
            readCircle = &object;
    }
    QVERIFY(readCircle);
    QCOMPARE(readCircle->params.value(QStringLiteral("radius")).toDouble(), 1.5);
    QCOMPARE(readCircle->params.value(QStringLiteral("position")).toPointF(), QPointF(-2, 1));

    // The real test: generating from what was read produces the same file.
    QCOMPARE(codegen::generate(rebuilt), original);
}

void LauncherTest::parserReportsWhatItCannotRead()
{
    const QString source = QStringLiteral(
        "from manim import *\n"
        "\n"
        "\n"
        "class Hand(Scene):\n"
        "    def construct(self):\n"
        "        circle = Circle(radius=1)\n"
        "        circle.move_to([0, 0, 0])\n"
        "        circle.set_sheen(0.4)\n"
        "        weird = SomethingNobodyHasHeardOf()\n"
        "\n"
        "        self.play(Create(circle, run_time=1))\n");

    Document document = Document::createNew(QStringLiteral("Hand"));
    const codeparser::Result outcome = codeparser::parse(source, &document);

    // What it understood, it kept.
    QVERIFY(outcome.ok());
    QCOMPARE(document.objects.size(), 1);
    QCOMPARE(document.timeline.clips.size(), 1);

    // What it did not, it named, rather than dropping in silence.
    QVERIFY(!outcome.complete);
    QCOMPARE(outcome.unrecognised.size(), 2);
    QVERIFY(outcome.unrecognised.join(QChar()).contains(QStringLiteral("set_sheen")));
    QVERIFY(outcome.unrecognised.join(QChar()).contains(QStringLiteral("SomethingNobodyHasHeardOf")));

    // A file with no scene at all is an error, not a silent empty document.
    Document empty = Document::createNew(QStringLiteral("Empty"));
    QVERIFY(!codeparser::parse(QStringLiteral("x = 1\n"), &empty).ok());
}

void LauncherTest::catalogCoversABroadRangeOfManim()
{
    // Every entry must be drawable and nameable, or it is not usable.
    for (const catalog::MobjectSpec &spec : catalog::mobjects()) {
        QVERIFY2(!spec.pythonName.isEmpty(), qPrintable(spec.id));
        QVERIFY2(!spec.displayName.isEmpty(), qPrintable(spec.id));
        QVERIFY2(!spec.category.isEmpty(), qPrintable(spec.id));
        QVERIFY2(catalog::findMobject(spec.id) == &spec, qPrintable(spec.id));

        // A default instance has to produce something the canvas can draw,
        // apart from a group, which is drawn by its children.
        evaluator::ObjectState state;
        state.type = spec.id;
        state.params = catalog::defaultParams(spec);
        if (spec.shape != catalog::ShapeKind::Group) {
            QVERIFY2(!SceneRenderer::shapeOf(state).isEmpty(), qPrintable(spec.id));
        }
    }

    for (const catalog::AnimationSpec &spec : catalog::animations()) {
        QVERIFY2(!spec.pythonName.isEmpty(), qPrintable(spec.id));
        QVERIFY2(catalog::findAnimation(spec.id) == &spec, qPrintable(spec.id));
    }

    qInfo("catalog: %lld mobjects, %lld animations",
          catalog::mobjects().size(), catalog::animations().size());
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

    // The window restores whichever page was last open, so say which one this
    // needs rather than depending on what another test left behind.
    window.showPage(ProjectWindow::Page::Edit);

    window.resize(1300, 820);
    window.show();
    window.raise();
    window.activateWindow();
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
    // A widget only takes application focus once its window is active, which
    // is not instant.
    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), field, 3000);

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
    const QByteArray target = qgetenv("MANIMATE_PROJECT_SNAPSHOT");
    if (target.isEmpty())
        QSKIP("MANIMATE_PROJECT_SNAPSHOT is not set");

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

    // The window restores whichever page was last open; this wants the editor.
    window.showPage(ProjectWindow::Page::Edit);

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
