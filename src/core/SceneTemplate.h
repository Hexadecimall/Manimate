#pragma once

#include <QString>

namespace mn {

class Document;
struct ProjectLayout;

/// The Python a project starts life with.
namespace scene_template {

/// A minimal runnable scene named after the document, used when a project has
/// no script yet. Written to be edited: it does something visible, and nothing
/// clever.
QString starterScript(const Document &document);

/// Path of the script a project edits: the one imported into export/, or the
/// one named after the project.
QString scriptPathFor(const ProjectLayout &layout, const Document &document);

/// Return the project's script, creating it from the starter template if the
/// project does not have one yet.
QString ensureScript(const ProjectLayout &layout, const Document &document,
                     QString *errorOut = nullptr);

} // namespace scene_template
} // namespace mn
