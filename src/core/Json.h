#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QVariantMap>

namespace mn::json {

/// Encode a QVariant as JSON. Types JSON cannot represent natively (colors,
/// points) are written as tagged objects so they survive a round-trip with
/// their type intact.
QJsonValue encode(const QVariant &value);

/// Inverse of encode(). Unrecognised input decodes to an invalid QVariant.
QVariant decode(const QJsonValue &value);

QJsonObject encodeMap(const QVariantMap &map);
QVariantMap decodeMap(const QJsonObject &object);

} // namespace mn::json
