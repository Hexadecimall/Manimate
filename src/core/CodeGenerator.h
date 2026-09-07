#pragma once

#include <QString>

namespace mn {

class Document;

/// Turning a scene into the Python that renders it.
///
/// The hard part is time. The timeline places animations at absolute times and
/// lets them overlap however they like; Manim has no clock at all, only a
/// sequence of play() calls. The solver here bridges the two exactly: clips
/// that overlap are gathered into one play(), and a clip starting partway
/// through its neighbours is wrapped in Succession(Wait(offset), …), which is
/// how an arbitrary offset is expressed in Manim without approximating it.
namespace codegen {

/// The complete, runnable module for `document`.
QString generate(const Document &document);

/// The body of construct(), without the class around it. Exposed for tests and
/// for showing a fragment in the interface.
QString constructBody(const Document &document, int indentLevel = 2);

} // namespace codegen
} // namespace mn
