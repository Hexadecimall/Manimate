#include "LauncherWindow.h"
#include "ProjectCardDelegate.h"
#include "Project.h"
#include "RecentProjects.h"
#include "RecentProjectsModel.h"
#include "AppWindow.h"
#include "CodeEditor.h"
#include "ProjectWindow.h"
#include "Theme.h"
#include "TitleBar.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolButton>
#include <QDateTime>
#include <QDir>
#include <QLineEdit>
#include <QListView>
#include <QTemporaryDir>
#include <QTest>

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

/// Writes a PNG of a project window, so the wired-up editor can be looked at.
void LauncherTest::projectSnapshot()
{
    const QByteArray target = qgetenv("MANIMATION_PROJECT_SNAPSHOT");
    if (target.isEmpty())
        QSKIP("MANIMATION_PROJECT_SNAPSHOT is not set");

    ProjectLayout layout;
    QString error;
    QVERIFY2(project::create(m_dir.path(), QStringLiteral("Fourier Series"), &layout, &error),
             qPrintable(error));

    theme::apply(*qApp);

    ProjectWindow window;
    QVERIFY(window.openProject(layout.projectFile));
    window.resize(1000, 660);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(250);

    QVERIFY(window.grab().save(QString::fromUtf8(target)));
}

QTEST_MAIN(LauncherTest)
#include "tst_launcher.moc"
