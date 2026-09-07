#include "Document.h"
#include "Json.h"
#include "Project.h"
#include "Version.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace mn;

class DocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void newDocumentHasIdentityAndOneTrack();
    void objectIdsAreUniqueAndNeverReused();
    void objectNamesAreDeduplicated();
    void removingObjectRemovesItsClips();
    void roundTripsThroughJson();
    void roundTripsThroughFile();
    void rejectsNewerFormatVersion();
    void rejectsMalformedFile();
    void savePreservesIdAllocatorAcrossReload();
    void frameWidthFollowsAspectRatio();

    void jsonPreservesRichParamTypes();

    void sanitizeNameStripsIllegalCharacters();
    void pythonNamesAreLegalIdentifiers();
    void createBuildsFullFolderStructure();
    void createRefusesToOverwriteExistingFolder();
    void resolveFindsProjectFromFileOrFolder();
    void ensureDirectoriesRestoresDeletedFolders();
};

void DocumentTest::newDocumentHasIdentityAndOneTrack()
{
    const Document document = Document::createNew(QStringLiteral("Demo"));
    QCOMPARE(document.metadata.name, QStringLiteral("Demo"));
    QVERIFY(!document.metadata.uuid.isNull());
    QVERIFY(document.metadata.created.isValid());
    QCOMPARE(document.timeline.tracks.size(), 1);
    QVERIFY(document.objects.isEmpty());
}

void DocumentTest::objectIdsAreUniqueAndNeverReused()
{
    Document document = Document::createNew(QStringLiteral("Demo"));

    SceneObject circle;
    circle.type = QStringLiteral("manim.Circle");
    circle.name = QStringLiteral("Circle");
    const ObjectId first = document.addObject(circle);

    SceneObject square;
    square.type = QStringLiteral("manim.Square");
    square.name = QStringLiteral("Square");
    const ObjectId second = document.addObject(square);

    QVERIFY(first != kInvalidObjectId);
    QVERIFY(second != first);

    QVERIFY(document.removeObject(first));
    const ObjectId third = document.addObject(circle);
    QVERIFY(third != first);
    QVERIFY(third != second);
}

void DocumentTest::objectNamesAreDeduplicated()
{
    Document document = Document::createNew(QStringLiteral("Demo"));

    SceneObject object;
    object.type = QStringLiteral("manim.Circle");
    object.name = QStringLiteral("Circle");

    document.addObject(object);
    document.addObject(object);
    document.addObject(object);

    QCOMPARE(document.objects.at(0).name, QStringLiteral("Circle"));
    QCOMPARE(document.objects.at(1).name, QStringLiteral("Circle 2"));
    QCOMPARE(document.objects.at(2).name, QStringLiteral("Circle 3"));
}

void DocumentTest::removingObjectRemovesItsClips()
{
    Document document = Document::createNew(QStringLiteral("Demo"));

    SceneObject object;
    object.type = QStringLiteral("manim.Circle");
    object.name = QStringLiteral("Circle");
    const ObjectId id = document.addObject(object);

    Clip clip;
    clip.objectId = id;
    clip.type = QStringLiteral("manim.Create");
    document.addClip(clip);

    SceneObject other;
    other.type = QStringLiteral("manim.Square");
    other.name = QStringLiteral("Square");
    const ObjectId otherId = document.addObject(other);
    Clip otherClip;
    otherClip.objectId = otherId;
    otherClip.type = QStringLiteral("manim.Create");
    const ClipId survivor = document.addClip(otherClip);

    QCOMPARE(document.timeline.clips.size(), 2);
    QVERIFY(document.removeObject(id));
    QCOMPARE(document.timeline.clips.size(), 1);
    QCOMPARE(document.timeline.clips.first().id, survivor);
}

void DocumentTest::roundTripsThroughJson()
{
    Document document = Document::createNew(QStringLiteral("Demo"));
    document.metadata.description = QStringLiteral("A test project");
    document.metadata.tags = {QStringLiteral("maths"), QStringLiteral("intro")};
    document.render.fps = 30;
    document.render.width = 1280;
    document.render.height = 720;
    document.render.background = QColor(QStringLiteral("#101014"));
    document.sceneClassName = QStringLiteral("Demo");

    SceneObject object;
    object.type = QStringLiteral("manim.Circle");
    object.name = QStringLiteral("Circle");
    object.params.insert(QStringLiteral("radius"), 1.5);
    object.params.insert(QStringLiteral("color"), QColor(QStringLiteral("#58c4dd")));
    object.rawPython = QStringLiteral("circle.set_z_index(3)");
    const ObjectId id = document.addObject(object);

    Clip clip;
    clip.objectId = id;
    clip.type = QStringLiteral("manim.Create");
    clip.start = 0.5;
    clip.duration = 2.25;
    clip.rateFunc = QStringLiteral("linear");
    document.addClip(clip);
    document.timeline.duration = 12.5;

    QString error;
    const Document reloaded = Document::fromJson(document.toJson(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QCOMPARE(reloaded.metadata.uuid, document.metadata.uuid);
    QCOMPARE(reloaded.metadata.description, document.metadata.description);
    QCOMPARE(reloaded.metadata.tags, document.metadata.tags);
    QCOMPARE(reloaded.render.fps, 30);
    QCOMPARE(reloaded.render.background, document.render.background);
    QCOMPARE(reloaded.sceneClassName, QStringLiteral("Demo"));
    QCOMPARE(reloaded.objects.size(), 1);
    QCOMPARE(reloaded.objects.first().params.value(QStringLiteral("radius")).toDouble(), 1.5);
    QCOMPARE(reloaded.objects.first().params.value(QStringLiteral("color")).value<QColor>(),
             QColor(QStringLiteral("#58c4dd")));
    QCOMPARE(reloaded.objects.first().rawPython, QStringLiteral("circle.set_z_index(3)"));
    QCOMPARE(reloaded.timeline.clips.size(), 1);
    QCOMPARE(reloaded.timeline.clips.first().start, 0.5);
    QCOMPARE(reloaded.timeline.clips.first().duration, 2.25);
    QCOMPARE(reloaded.timeline.clips.first().rateFunc, QStringLiteral("linear"));
    QCOMPARE(reloaded.timeline.duration, 12.5);
}

void DocumentTest::roundTripsThroughFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("Demo.manproj"));

    Document document = Document::createNew(QStringLiteral("Demo"));
    QString error;
    QVERIFY2(document.save(path, &error), qPrintable(error));
    QCOMPARE(document.metadata.revision, 1);

    Document reloaded;
    QVERIFY2(Document::load(path, &reloaded, &error), qPrintable(error));
    QCOMPARE(reloaded.metadata.uuid, document.metadata.uuid);
    QCOMPARE(reloaded.metadata.revision, 1);
    QCOMPARE(reloaded.metadata.writtenBy, version::string());
}

void DocumentTest::rejectsNewerFormatVersion()
{
    QJsonObject root = Document::createNew(QStringLiteral("Demo")).toJson();
    root.insert(QStringLiteral("formatVersion"), version::kProjectFormat + 1);

    QString error;
    Document::fromJson(root, &error);
    QVERIFY(!error.isEmpty());
}

void DocumentTest::rejectsMalformedFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("Broken.manproj"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not json");
    file.close();

    Document document;
    QString error;
    QVERIFY(!Document::load(path, &document, &error));
    QVERIFY(!error.isEmpty());
}

void DocumentTest::savePreservesIdAllocatorAcrossReload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("Demo.manproj"));

    Document document = Document::createNew(QStringLiteral("Demo"));
    SceneObject object;
    object.type = QStringLiteral("manim.Circle");
    object.name = QStringLiteral("Circle");
    const ObjectId first = document.addObject(object);
    document.removeObject(first);
    const ObjectId second = document.addObject(object);

    QString error;
    QVERIFY2(document.save(path, &error), qPrintable(error));

    Document reloaded;
    QVERIFY2(Document::load(path, &reloaded, &error), qPrintable(error));
    const ObjectId third = reloaded.addObject(object);
    QVERIFY(third > second);
}

void DocumentTest::frameWidthFollowsAspectRatio()
{
    RenderSettings settings;
    settings.width = 1920;
    settings.height = 1080;
    QVERIFY(qAbs(settings.frameWidthUnits() - 14.2222) < 0.001);

    settings.width = 1080;
    settings.height = 1080;
    QVERIFY(qAbs(settings.frameWidthUnits() - 8.0) < 0.001);
}

void DocumentTest::jsonPreservesRichParamTypes()
{
    QVariantMap params;
    params.insert(QStringLiteral("count"), 7);
    params.insert(QStringLiteral("scale"), 0.25);
    params.insert(QStringLiteral("label"), QStringLiteral("hi"));
    params.insert(QStringLiteral("on"), true);
    params.insert(QStringLiteral("color"), QColor(QStringLiteral("#ff5c5c")));
    params.insert(QStringLiteral("at"), QPointF(1.5, -2.5));
    params.insert(QStringLiteral("list"), QVariantList{1, 2, 3});

    const QVariantMap back = json::decodeMap(json::encodeMap(params));

    QCOMPARE(back.value(QStringLiteral("count")).toInt(), 7);
    QCOMPARE(back.value(QStringLiteral("scale")).toDouble(), 0.25);
    QCOMPARE(back.value(QStringLiteral("label")).toString(), QStringLiteral("hi"));
    QCOMPARE(back.value(QStringLiteral("on")).toBool(), true);
    QCOMPARE(back.value(QStringLiteral("color")).value<QColor>(), QColor(QStringLiteral("#ff5c5c")));
    QCOMPARE(back.value(QStringLiteral("at")).toPointF(), QPointF(1.5, -2.5));
    QCOMPARE(back.value(QStringLiteral("list")).toList().size(), 3);
}

void DocumentTest::sanitizeNameStripsIllegalCharacters()
{
    QCOMPARE(project::sanitizeName(QStringLiteral("My/Project")), QStringLiteral("My Project"));
    QCOMPARE(project::sanitizeName(QStringLiteral("  spaced   out  ")), QStringLiteral("spaced out"));
    QCOMPARE(project::sanitizeName(QStringLiteral("trailing.")), QStringLiteral("trailing"));
    QCOMPARE(project::sanitizeName(QStringLiteral("///")), QString());
}

void DocumentTest::pythonNamesAreLegalIdentifiers()
{
    QCOMPARE(project::pythonModuleName(QStringLiteral("My Project")), QStringLiteral("my_project"));
    QCOMPARE(project::pythonModuleName(QStringLiteral("3 Blue")), QStringLiteral("scene_3_blue"));
    QCOMPARE(project::pythonClassName(QStringLiteral("my project")), QStringLiteral("MyProject"));
    QCOMPARE(project::pythonClassName(QStringLiteral("3 blue")), QStringLiteral("Scene3Blue"));
}

void DocumentTest::createBuildsFullFolderStructure()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QString error;
    QVERIFY2(project::create(dir.path(), QStringLiteral("My Project"), &layout, &error), qPrintable(error));

    QVERIFY(QFileInfo(layout.projectFile).isFile());
    QCOMPARE(QFileInfo(layout.projectFile).fileName(), QStringLiteral("My Project.manproj"));
    QVERIFY(QFileInfo(layout.internalDir).isDir());
    QVERIFY(QFileInfo(layout.cacheDir).isDir());
    QVERIFY(QFileInfo(layout.intermediateDir).isDir());
    QVERIFY(QFileInfo(layout.backupsDir).isDir());
    QVERIFY(QFileInfo(layout.exportDir).isDir());
    QVERIFY(QFileInfo(layout.outputDir).isDir());
    QVERIFY(QFileInfo(layout.assetsDir).isDir());

    Document document;
    QVERIFY2(Document::load(layout.projectFile, &document, &error), qPrintable(error));
    QCOMPARE(document.metadata.name, QStringLiteral("My Project"));
    QCOMPARE(document.sceneClassName, QStringLiteral("MyProject"));
}

void DocumentTest::createRefusesToOverwriteExistingFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString error;
    QVERIFY(project::create(dir.path(), QStringLiteral("Twice"), nullptr, &error));
    QVERIFY(!project::create(dir.path(), QStringLiteral("Twice"), nullptr, &error));
    QVERIFY(!error.isEmpty());
}

void DocumentTest::resolveFindsProjectFromFileOrFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout created;
    QVERIFY(project::create(dir.path(), QStringLiteral("Findable"), &created, nullptr));

    const auto fromFile = project::resolve(created.projectFile);
    QVERIFY(fromFile.has_value());
    QCOMPARE(QFileInfo(fromFile->projectFile).absoluteFilePath(),
             QFileInfo(created.projectFile).absoluteFilePath());

    const auto fromFolder = project::resolve(created.root);
    QVERIFY(fromFolder.has_value());
    QCOMPARE(QFileInfo(fromFolder->projectFile).absoluteFilePath(),
             QFileInfo(created.projectFile).absoluteFilePath());

    QVERIFY(!project::resolve(dir.path()).has_value());
    QVERIFY(!project::resolve(QDir(dir.path()).filePath(QStringLiteral("nope"))).has_value());
}

void DocumentTest::ensureDirectoriesRestoresDeletedFolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ProjectLayout layout;
    QVERIFY(project::create(dir.path(), QStringLiteral("Recoverable"), &layout, nullptr));
    QVERIFY(QDir(layout.internalDir).removeRecursively());
    QVERIFY(!QFileInfo(layout.cacheDir).isDir());

    QString error;
    QVERIFY2(project::ensureDirectories(layout, &error), qPrintable(error));
    QVERIFY(QFileInfo(layout.cacheDir).isDir());
    QVERIFY(QFileInfo(layout.backupsDir).isDir());
}

QTEST_MAIN(DocumentTest)
#include "tst_document.moc"
