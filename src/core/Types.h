#pragma once

#include <QtGlobal>

namespace mn {

/// Stable identifier for a scene object within one document.
using ObjectId = quint64;

/// Stable identifier for a timeline clip within one document.
using ClipId = quint64;

inline constexpr ObjectId kInvalidObjectId = 0;
inline constexpr ClipId kInvalidClipId = 0;

} // namespace mn
