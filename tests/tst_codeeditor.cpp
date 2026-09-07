#include "CodeEditor.h"
#include "CodeEditorWindow.h"
#include "ManimVocabulary.h"
#include "PythonHighlighter.h"
#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>

using namespace mn::ui;

class CodeEditorTest : public QObject
{
    Q_OBJECT

private slots:
    void vocabularyGroupsAreDistinct();
    void vocabularyRecognisesTheNamesItLists();
    void vocabularyCoversEveryGroup();

    void returnKeepsIndentation();
    void returnStepsInAfterAColon();
    void tabIndentsToTheNextStop();
    void tabIndentsEveryLineOfASelection();
    void backtabUnindents();
    void backspaceCollapsesAWholeIndent();
    void backspaceDeletesOneCharacterMidLine();
    void commentToggleAddsAndRemoves();

    void highlighterColoursManimVocabulary();
    void highlighterTreatsTripleQuotedStringsAsOneRun();
    void highlighterDoesNotSeeCodeInsideAComment();

    void windowRoundTripsAFile();
    void windowTracksModification();

    void snapshot();

private:
    /// Type into a fresh editor and return what came out.
    static QString afterTyping(const QString &initial, const QList<QKeyEvent *> &keys,
                               int cursorPosition = -1);
};

void CodeEditorTest::vocabularyGroupsAreDistinct()
{
    // A name should mean one thing. Overlaps between groups would make the
    // highlighter's decision order silently significant.
    const QStringList groups[] = {manim::mobjects(), manim::animations(), manim::scenes()};
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            for (const QString &word : groups[i])
                QVERIFY2(!groups[j].contains(word), qPrintable(QStringLiteral("duplicated: ") + word));
        }
    }
}

void CodeEditorTest::vocabularyRecognisesTheNamesItLists()
{
    QVERIFY(manim::isMobject(QStringLiteral("Circle")));
    QVERIFY(manim::isMobject(QStringLiteral("MathTex")));
    QVERIFY(manim::isAnimation(QStringLiteral("Create")));
    QVERIFY(manim::isAnimation(QStringLiteral("ReplacementTransform")));
    QVERIFY(manim::isScene(QStringLiteral("ThreeDScene")));
    QVERIFY(manim::isColor(QStringLiteral("BLUE_E")));
    QVERIFY(manim::isColor(QStringLiteral("WHITE")));
    QVERIFY(manim::isConstant(QStringLiteral("UP")));
    QVERIFY(manim::isConstant(QStringLiteral("DEGREES")));
    QVERIFY(manim::isRateFunction(QStringLiteral("there_and_back")));

    QVERIFY(!manim::isMobject(QStringLiteral("Circles")));
    QVERIFY(!manim::isAnimation(QStringLiteral("create")));
    QVERIFY(!manim::isColor(QStringLiteral("BLUE_F")));

    QVERIFY(python::isKeyword(QStringLiteral("class")));
    QVERIFY(python::isBuiltin(QStringLiteral("range")));
    QVERIFY(!python::isKeyword(QStringLiteral("Circle")));
}

void CodeEditorTest::vocabularyCoversEveryGroup()
{
    const QStringList all = manim::all();
    for (const QStringList &group : {manim::mobjects(), manim::animations(), manim::scenes(),
                                     manim::colors(), manim::constants(), manim::rateFunctions()}) {
        QVERIFY(!group.isEmpty());
        for (const QString &word : group)
            QVERIFY2(all.contains(word), qPrintable(QStringLiteral("missing from all(): ") + word));
    }
}

QString CodeEditorTest::afterTyping(const QString &initial, const QList<QKeyEvent *> &keys,
                                    int cursorPosition)
{
    CodeEditor editor;
    editor.setPlainText(initial);

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(cursorPosition < 0 ? initial.length() : cursorPosition);
    editor.setTextCursor(cursor);

    for (QKeyEvent *key : keys) {
        QApplication::sendEvent(&editor, key);
        delete key;
    }
    return editor.toPlainText();
}

void CodeEditorTest::returnKeepsIndentation()
{
    const QString result = afterTyping(
        QStringLiteral("        self.play(Create(circle))"),
        {new QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"))});
    QCOMPARE(result, QStringLiteral("        self.play(Create(circle))\n        "));
}

void CodeEditorTest::returnStepsInAfterAColon()
{
    const QString result = afterTyping(
        QStringLiteral("    def construct(self):"),
        {new QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"))});
    QCOMPARE(result, QStringLiteral("    def construct(self):\n        "));
}

void CodeEditorTest::tabIndentsToTheNextStop()
{
    // From column 2, tab should add two spaces, not four.
    const QString result = afterTyping(
        QStringLiteral("ab"),
        {new QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"))});
    QCOMPARE(result, QStringLiteral("ab  "));

    const QString fromZero = afterTyping(
        QString(),
        {new QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"))});
    QCOMPARE(fromZero, QStringLiteral("    "));
}

void CodeEditorTest::tabIndentsEveryLineOfASelection()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("one\ntwo\nthree"));

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(int(QStringLiteral("one\ntwo").length()), QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);

    QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
    QApplication::sendEvent(&editor, &tab);

    QCOMPARE(editor.toPlainText(), QStringLiteral("    one\n    two\nthree"));
}

void CodeEditorTest::backtabUnindents()
{
    const QString result = afterTyping(
        QStringLiteral("        indented"),
        {new QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier, QString())});
    QCOMPARE(result, QStringLiteral("    indented"));
}

void CodeEditorTest::backspaceCollapsesAWholeIndent()
{
    const QString result = afterTyping(
        QStringLiteral("        "),
        {new QKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier, QString())});
    QCOMPARE(result, QStringLiteral("    "));
}

void CodeEditorTest::backspaceDeletesOneCharacterMidLine()
{
    // Mid-line, backspace is just backspace, even at a multiple of four.
    const QString result = afterTyping(
        QStringLiteral("abcd"),
        {new QKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier, QString())});
    QCOMPARE(result, QStringLiteral("abc"));
}

void CodeEditorTest::commentToggleAddsAndRemoves()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("    circle = Circle()\n    self.add(circle)"));

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);

    QKeyEvent toggle(QEvent::KeyPress, Qt::Key_Slash, Qt::ControlModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &toggle);
    QCOMPARE(editor.toPlainText(),
             QStringLiteral("    # circle = Circle()\n    # self.add(circle)"));

    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    QKeyEvent again(QEvent::KeyPress, Qt::Key_Slash, Qt::ControlModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &again);
    QCOMPARE(editor.toPlainText(),
             QStringLiteral("    circle = Circle()\n    self.add(circle)"));
}

void CodeEditorTest::highlighterColoursManimVocabulary()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("c = Circle(color=BLUE)"));

    const QTextBlock block = editor.document()->firstBlock();
    const QList<QTextLayout::FormatRange> formats = block.layout()->formats();
    QVERIFY(!formats.isEmpty());

    auto colourAt = [&](int index) {
        QColor found;
        for (const QTextLayout::FormatRange &range : formats) {
            if (index >= range.start && index < range.start + range.length)
                found = range.format.foreground().color();
        }
        return found;
    };

    const int circleAt = int(QStringLiteral("c = ").length());
    const int blueAt = int(QStringLiteral("c = Circle(color=").length());

    QVERIFY(colourAt(circleAt).isValid());
    QVERIFY(colourAt(blueAt).isValid());
    // A mobject and a colour constant must not look the same.
    QVERIFY(colourAt(circleAt) != colourAt(blueAt));
}

void CodeEditorTest::highlighterTreatsTripleQuotedStringsAsOneRun()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("x = '''\nCircle is not code here\n'''\ny = 1"));

    const QTextDocument *document = editor.document();
    // The middle line sits inside the string, so it ends in the string state.
    QCOMPARE(document->findBlockByNumber(1).userState(), int(PythonHighlighter::InSingleQuotedBlock));
    // The line after the closing quotes is back to ordinary code.
    QCOMPARE(document->findBlockByNumber(3).userState(), int(PythonHighlighter::NotInString));
}

void CodeEditorTest::highlighterDoesNotSeeCodeInsideAComment()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("# Circle(color=BLUE)"));

    const QTextBlock block = editor.document()->firstBlock();
    const QList<QTextLayout::FormatRange> formats = block.layout()->formats();

    // Whatever else happens, the whole line ends up one colour: the comment's.
    QSet<QRgb> colours;
    for (int i = 0; i < block.text().length(); ++i) {
        for (const QTextLayout::FormatRange &range : formats) {
            if (i >= range.start && i < range.start + range.length)
                colours.insert(range.format.foreground().color().rgb());
        }
    }
    QCOMPARE(colours.size(), 1);
}

void CodeEditorTest::windowRoundTripsAFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = QDir(dir.path()).filePath(QStringLiteral("scene.py"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("from manim import *\n\n\nclass Demo(Scene):\n    def construct(self):\n        pass\n");
    file.close();

    CodeEditorWindow window;
    QVERIFY(window.open(path));
    QVERIFY(window.editor()->toPlainText().contains(QStringLiteral("class Demo(Scene):")));
    QVERIFY(!window.isModified());

    window.editor()->appendPlainText(QStringLiteral("# a trailing note"));
    QVERIFY(window.isModified());
    QVERIFY(window.save());
    QVERIFY(!window.isModified());

    CodeEditorWindow reopened;
    QVERIFY(reopened.open(path));
    QVERIFY(reopened.editor()->toPlainText().contains(QStringLiteral("# a trailing note")));
}

void CodeEditorTest::windowTracksModification()
{
    CodeEditorWindow window;
    QVERIFY(!window.isModified());
    QVERIFY(window.filePath().isEmpty());

    // setPlainText is a load, not an edit: Qt clears the modified flag for it.
    window.editor()->setPlainText(QStringLiteral("x = 1"));
    QVERIFY(!window.isModified());

    // Typing is an edit.
    window.editor()->insertPlainText(QStringLiteral("0"));
    QVERIFY(window.isModified());
}

/// Writes a PNG of the editor when MANIMATION_UI_SNAPSHOT names a path, so the
/// highlighting can be looked at rather than only asserted about.
void CodeEditorTest::snapshot()
{
    const QByteArray target = qgetenv("MANIMATION_UI_SNAPSHOT");
    if (target.isEmpty())
        QSKIP("MANIMATION_UI_SNAPSHOT is not set");

    const QByteArray source = qgetenv("MANIMATION_UI_SNAPSHOT_SOURCE");
    QVERIFY(!source.isEmpty());

    mn::theme::apply(*qApp);

    CodeEditorWindow window;
    QVERIFY(window.open(QString::fromUtf8(source)));
    window.resize(980, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(250);

    QVERIFY(window.grab().save(QString::fromUtf8(target)));
}

QTEST_MAIN(CodeEditorTest)
#include "tst_codeeditor.moc"
