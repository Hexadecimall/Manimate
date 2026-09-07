#pragma once

#include <QString>
#include <QStringList>

namespace mn {

class Document;

/// Reading a scene back out of Python.
///
/// This is the harder direction, and it is honest about its limits. It reads
/// the shape of code the generator writes: constructor assignments, move_to
/// and friends, and the play calls the solver produces. Anything it does not
/// recognise is reported rather than dropped silently, so the editor can say
/// what it could not take in instead of quietly losing it.
///
/// It is not a Python parser and does not pretend to be one. Code outside the
/// shape it knows is left to the code page, which is the right place for it.
namespace codeparser {

struct Result
{
    /// True when the whole file was understood.
    bool complete = false;

    /// Lines the parser could not account for, with their numbers.
    QStringList unrecognised;

    /// Why the file could not be read at all, if it could not.
    QString error;

    bool ok() const { return error.isEmpty(); }
};

/// Read `source` into `document`, replacing its objects and timeline.
/// The document's metadata and render settings are left alone.
Result parse(const QString &source, Document *document);

} // namespace codeparser
} // namespace mn
