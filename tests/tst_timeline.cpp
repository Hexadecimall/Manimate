#include "CodeGenerator.h"
#include "CodeParser.h"
#include "Document.h"
#include "EditorState.h"
#include "TimelineView.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using namespace mn;
using namespace mn::ui;

namespace {

/// The timeline's own layout, mirrored so a test can point at a clip: the
/// header column on the left, the ruler above the lanes, and the row pitch.
constexpr int kHeaderWidth = 116;
constexpr int kLeftPadding = 8;
constexpr int kRulerHeight = 28;
constexpr int kRowPitch = 44;

double xAt(const TimelineView &view, double time)
{
    return kHeaderWidth + kLeftPadding + time * view.scale() - view.offset();
}

/// The middle of row `row` of the first lane.
double yOfRow(int row)
{
    return kRulerHeight + row * kRowPitch + kRowPitch / 2.0;
}

/// Below every lane a scene of one object has, where nothing can be hit.
constexpr int kEmptyY = 150;

} // namespace

class TimelineTest : public QObject
{
    Q_OBJECT

private slots:
    void rowsSurviveJson();
    void aProjectFromBeforeRowsStacksItsOverlaps();
    void aNewClipTakesARowWithRoomForIt();
    void anEmptyRowCollapses();
    void movingPastTheLastRowMakesANewOne();
    void undoRestoresTheRow();
    void rowsDoNotReachThePython();
    void rowsSurviveTheCodeRoundTrip();

    void marqueeTakesEveryClipItTouches();
    void marqueeFollowsTheScroll();
    void escapeAbandonsTheMarquee();
    void aPressOnAClipDragsItInstead();
    void theRulerStillScrubs();
    void draggingAClipDownMovesItToAnotherRow();
};

void TimelineTest::rowsSurviveJson()
{
    Document document = Document::createNew(QStringLiteral("Rows"));

    SceneObject circle;
    circle.type = QStringLiteral("manim.Circle");
    circle.name = QStringLiteral("Circle");
    const ObjectId object = document.addObject(circle);

    Clip clip;
    clip.objectId = object;
    clip.type = QStringLiteral("manim.Create");
    clip.row = 2;
    const ClipId id = document.addClip(clip);

    const Document read = Document::fromJson(document.toJson());
    QCOMPARE(read.findClip(id)->row, 2);
}

void TimelineTest::aProjectFromBeforeRowsStacksItsOverlaps()
{
    // Format 1 knew nothing of rows: the timeline stacked overlapping clips
    // itself, and loading one has to reproduce that arrangement.
    Document document = Document::createNew(QStringLiteral("Old"));

    SceneObject circle;
    circle.type = QStringLiteral("manim.Circle");
    circle.name = QStringLiteral("Circle");
    const ObjectId object = document.addObject(circle);

    QJsonObject root = document.toJson();
    root.insert(QStringLiteral("formatVersion"), 1);

    QJsonArray clips;
    for (const auto &[id, start, duration] : {std::tuple{1, 0.0, 2.0}, std::tuple{2, 1.0, 2.0},
                                              std::tuple{3, 5.0, 1.0}}) {
        QJsonObject clip;
        clip.insert(QStringLiteral("id"), id);
        clip.insert(QStringLiteral("object"), qint64(object));
        clip.insert(QStringLiteral("type"), QStringLiteral("manim.Create"));
        clip.insert(QStringLiteral("start"), start);
        clip.insert(QStringLiteral("duration"), duration);
        clips.append(clip);
    }
    QJsonObject timeline = root.value(QStringLiteral("timeline")).toObject();
    timeline.insert(QStringLiteral("clips"), clips);
    root.insert(QStringLiteral("timeline"), timeline);

    const Document read = Document::fromJson(root);
    QCOMPARE(read.timeline.clips.size(), 3);
    QCOMPARE(read.findClip(1)->row, 0);
    QCOMPARE(read.findClip(2)->row, 1);
    QCOMPARE(read.findClip(3)->row, 0);
}

void TimelineTest::aNewClipTakesARowWithRoomForIt()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Rows")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));

    state.setPlayhead(0.0);
    const ClipId first = state.addClip(circle, QStringLiteral("manim.Create"));
    // The second one starts inside the first, so it cannot share its row.
    state.setPlayhead(0.2);
    const ClipId second = state.addClip(circle, QStringLiteral("manim.Rotate"));
    state.setPlayhead(9.0);
    const ClipId third = state.addClip(circle, QStringLiteral("manim.Shift"));

    QCOMPARE(state.document().findClip(first)->row, 0);
    QCOMPARE(state.document().findClip(second)->row, 1);
    QCOMPARE(state.document().findClip(third)->row, 0);
}

void TimelineTest::anEmptyRowCollapses()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Rows")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));

    const ClipId first = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setPlayhead(4.0);
    const ClipId second = state.addClip(circle, QStringLiteral("manim.Shift"));

    state.setClipRow(second, 3);
    // Nothing is on rows 1 and 2, so the clip lands directly under the first.
    QCOMPARE(state.document().findClip(second)->row, 1);

    state.setClipRow(second, 0);
    QCOMPARE(state.document().findClip(first)->row, 0);
    QCOMPARE(state.document().findClip(second)->row, 0);
}

void TimelineTest::movingPastTheLastRowMakesANewOne()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Rows")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));

    const ClipId first = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setPlayhead(4.0);
    const ClipId second = state.addClip(circle, QStringLiteral("manim.Shift"));
    QCOMPARE(state.document().findClip(second)->row, 0);

    state.setClipRow(second, 1);
    QCOMPARE(state.document().findClip(first)->row, 0);
    QCOMPARE(state.document().findClip(second)->row, 1);

    // Alone on the last row, it has nowhere further to go.
    state.setClipRow(second, 2);
    QCOMPARE(state.document().findClip(second)->row, 1);
}

void TimelineTest::undoRestoresTheRow()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Rows")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));

    state.addClip(circle, QStringLiteral("manim.Create"));
    state.setPlayhead(4.0);
    const ClipId second = state.addClip(circle, QStringLiteral("manim.Shift"));

    state.beginEdit();
    state.setClipRow(second, 1);
    QCOMPARE(state.document().findClip(second)->row, 1);

    state.undo();
    QCOMPARE(state.document().findClip(second)->row, 0);
    state.redo();
    QCOMPARE(state.document().findClip(second)->row, 1);
}

void TimelineTest::rowsDoNotReachThePython()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Rows")), {});
    state.documentForWriting().sceneClassName = QStringLiteral("Rows");

    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    const ClipId create = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(create, 0.0, 1.0, 0);
    const ClipId shift = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(shift, 0.5, 1.0, 0);
    const ClipId rotate = state.addClip(circle, QStringLiteral("manim.Rotate"));
    state.setClipTiming(rotate, 3.0, 1.0, 0);

    const QString before = codegen::generate(state.document());

    state.setClipRow(shift, 0);
    state.setClipRow(rotate, 1);
    state.setClipRow(create, 1);

    QCOMPARE(codegen::generate(state.document()), before);
}

void TimelineTest::rowsSurviveTheCodeRoundTrip()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Trip")), {});
    state.documentForWriting().sceneClassName = QStringLiteral("Trip");

    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    const ClipId create = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(create, 0.0, 1.0, 0);
    const ClipId shift = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(shift, 0.5, 1.0, 0);
    state.setClipRow(shift, 1);

    const QString original = codegen::generate(state.document());

    Document rebuilt = Document::createNew(QStringLiteral("Trip"));
    const codeparser::Result outcome = codeparser::parse(original, &rebuilt);
    QVERIFY2(outcome.ok(), qPrintable(outcome.error));
    QCOMPARE(codegen::generate(rebuilt), original);
}

void TimelineTest::marqueeTakesEveryClipItTouches()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Sweep")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));

    const ClipId first = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(first, 0.0, 1.0, 0);
    const ClipId second = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(second, 4.0, 1.0, 0);

    TimelineView view(&state);
    view.resize(900, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // Swept up from below the lane, over the first clip only.
    const QPoint from(int(xAt(view, 0.1)), kEmptyY);
    const QPoint to(int(xAt(view, 0.9)), int(yOfRow(0)));
    QTest::mousePress(&view, Qt::LeftButton, {}, from);
    QTest::mouseMove(&view, to);
    QTest::mouseRelease(&view, Qt::LeftButton, {}, to);

    QCOMPARE(state.selectedClips(), QVector<ClipId>{first});
    // What the clip animates comes with it, so the canvas agrees.
    QCOMPARE(state.selectedObject(), circle);

    // Wide enough for both.
    const QPoint wide(int(xAt(view, 4.9)), int(yOfRow(0)));
    QTest::mousePress(&view, Qt::LeftButton, {}, from);
    QTest::mouseMove(&view, wide);
    QTest::mouseRelease(&view, Qt::LeftButton, {}, wide);
    QCOMPARE(state.selectedClips().size(), 2);
    QVERIFY(state.isClipSelected(second));

    // A press on nothing, without a drag, selects nothing.
    QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 8.0)), kEmptyY));
    QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 8.0)), kEmptyY));
    QVERIFY(state.selectedClips().isEmpty());
}

void TimelineTest::marqueeFollowsTheScroll()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Sweep")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));

    const ClipId first = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(first, 0.0, 1.0, 0);
    const ClipId second = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(second, 6.0, 1.0, 0);

    TimelineView view(&state);
    view.resize(900, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // Scrolled along, the same rectangle over the same seconds still catches
    // the clip that is now under it and not the one that has gone past.
    view.setOffset(5.5 * view.scale());
    QVERIFY(view.offset() > 0.0);

    const QPoint from(int(xAt(view, 5.9)), kEmptyY);
    const QPoint to(int(xAt(view, 7.1)), int(yOfRow(0)));
    QTest::mousePress(&view, Qt::LeftButton, {}, from);
    QTest::mouseMove(&view, to);
    QTest::mouseRelease(&view, Qt::LeftButton, {}, to);

    QCOMPARE(state.selectedClips(), QVector<ClipId>{second});
}

void TimelineTest::escapeAbandonsTheMarquee()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Sweep")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    const ClipId clip = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(clip, 0.0, 1.0, 0);

    TimelineView view(&state);
    view.resize(900, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // The same sweep that would otherwise catch the clip.
    QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 0.1)), kEmptyY));
    QTest::mouseMove(&view, QPoint(int(xAt(view, 0.9)), int(yOfRow(0))));
    QTest::keyClick(&view, Qt::Key_Escape);
    QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 0.9)), int(yOfRow(0))));

    QVERIFY(state.selectedClips().isEmpty());
}

void TimelineTest::aPressOnAClipDragsItInstead()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Drag")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    const ClipId clip = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(clip, 1.0, 2.0, 0);

    TimelineView view(&state);
    view.resize(900, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 2.0)), int(yOfRow(0))));
    QTest::mouseMove(&view, QPoint(int(xAt(view, 3.0)), int(yOfRow(0))));
    QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 3.0)), int(yOfRow(0))));

    QCOMPARE(state.document().findClip(clip)->start, 2.0);
    QCOMPARE(state.selectedClips(), QVector<ClipId>{clip});
}

void TimelineTest::theRulerStillScrubs()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Scrub")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));
    const ClipId clip = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(clip, 0.0, 1.0, 0);
    state.selectClip(clip);

    TimelineView view(&state);
    view.resize(900, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 3.0)), 6));
    QTest::mouseMove(&view, QPoint(int(xAt(view, 4.0)), 6));
    QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 4.0)), 6));

    QCOMPARE(state.playhead(), 4.0);
    // Scrubbing is not a selection: the clip is still the one being edited.
    QCOMPARE(state.selectedClip(), clip);
}

void TimelineTest::draggingAClipDownMovesItToAnotherRow()
{
    EditorState state;
    state.setDocument(Document::createNew(QStringLiteral("Rows")), {});
    const ObjectId circle = state.addObject(QStringLiteral("manim.Circle"));

    const ClipId first = state.addClip(circle, QStringLiteral("manim.Create"));
    state.setClipTiming(first, 0.0, 1.0, 0);
    const ClipId second = state.addClip(circle, QStringLiteral("manim.Shift"));
    state.setClipTiming(second, 4.0, 1.0, 0);

    TimelineView view(&state);
    view.resize(900, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // Down onto the spare row the lane keeps while a clip is being dragged.
    QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 4.5)), int(yOfRow(0))));
    QTest::mouseMove(&view, QPoint(int(xAt(view, 4.5)), int(yOfRow(1))));
    QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 4.5)), int(yOfRow(1))));

    QCOMPARE(state.document().findClip(second)->row, 1);
    QCOMPARE(state.document().findClip(first)->row, 0);
    // Sideways is untouched by the trip downwards.
    QCOMPARE(state.document().findClip(second)->start, 4.0);

    // Back up, and the row it left collapses.
    QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 4.5)), int(yOfRow(1))));
    QTest::mouseMove(&view, QPoint(int(xAt(view, 4.5)), int(yOfRow(0))));
    QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(int(xAt(view, 4.5)), int(yOfRow(0))));

    QCOMPARE(state.document().findClip(second)->row, 0);
}

QTEST_MAIN(TimelineTest)
#include "tst_timeline.moc"
