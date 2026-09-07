#pragma once

#include <QString>
#include <QStringList>

namespace mn {

class Document;
struct ProjectLayout;

/// Bringing an existing Manim script into a project.
///
/// This reads Python without parsing it. A real parser is not needed to answer
/// the only questions that matter here — which Scene subclasses does this file
/// define, and does it import Manim at all — and a regex that admits what it
/// cannot see is more honest than a parser that pretends to understand every
/// file it is given.
namespace python_import {

/// What a quick read of a Python file can tell us about it.
struct Scan
{
    /// Classes whose bases mention a Manim scene type, in the order found.
    QStringList sceneClasses;

    /// True if the file imports manim in any of its usual forms.
    bool importsManim = false;

    int lineCount = 0;

    /// A file is worth importing if it defines at least one scene.
    bool looksLikeManim() const { return !sceneClasses.isEmpty(); }
};

Scan scan(const QString &source);
Scan scanFile(const QString &filePath, QString *errorOut = nullptr);

/// Copy `sourceFile` into the project's export folder and point `document` at
/// the scene it defines. The file is left exactly as it was written.
bool into(const ProjectLayout &layout, const QString &sourceFile, Document *document,
          QString *errorOut = nullptr);

} // namespace python_import
} // namespace mn
