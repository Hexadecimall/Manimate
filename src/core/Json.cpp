#include "Json.h"

#include <QColor>
#include <QJsonArray>
#include <QPointF>

namespace mn::json {
namespace {

constexpr auto kTypeKey = "$type";

QJsonObject tagged(const char *type)
{
    QJsonObject o;
    o.insert(QLatin1String(kTypeKey), QLatin1String(type));
    return o;
}

} // namespace

QJsonValue encode(const QVariant &value)
{
    switch (value.typeId()) {
    case QMetaType::UnknownType:
        return QJsonValue::Null;
    case QMetaType::Bool:
        return value.toBool();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return value.toLongLong();
    case QMetaType::Float:
    case QMetaType::Double:
        return value.toDouble();
    case QMetaType::QString:
        return value.toString();
    case QMetaType::QColor: {
        QJsonObject o = tagged("color");
        o.insert(QStringLiteral("v"), value.value<QColor>().name(QColor::HexArgb));
        return o;
    }
    case QMetaType::QPointF: {
        const QPointF p = value.toPointF();
        QJsonObject o = tagged("point");
        o.insert(QStringLiteral("x"), p.x());
        o.insert(QStringLiteral("y"), p.y());
        return o;
    }
    case QMetaType::QVariantList: {
        QJsonArray array;
        for (const QVariant &item : value.toList())
            array.append(encode(item));
        return array;
    }
    case QMetaType::QStringList: {
        QJsonArray array;
        for (const QString &item : value.toStringList())
            array.append(item);
        return array;
    }
    case QMetaType::QVariantMap:
        return encodeMap(value.toMap());
    default:
        return value.toString();
    }
}

QVariant decode(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        return {};
    case QJsonValue::Bool:
        return value.toBool();
    case QJsonValue::Double: {
        const double d = value.toDouble();
        // Integral values come back as integers so params keep their type.
        if (d == qint64(d))
            return QVariant(qint64(d));
        return d;
    }
    case QJsonValue::String:
        return value.toString();
    case QJsonValue::Array: {
        QVariantList list;
        const QJsonArray array = value.toArray();
        list.reserve(array.size());
        for (const QJsonValue &item : array)
            list.append(decode(item));
        return list;
    }
    case QJsonValue::Object: {
        const QJsonObject object = value.toObject();
        const QString type = object.value(QLatin1String(kTypeKey)).toString();
        if (type == QLatin1String("color"))
            return QColor::fromString(object.value(QStringLiteral("v")).toString());
        if (type == QLatin1String("point"))
            return QPointF(object.value(QStringLiteral("x")).toDouble(),
                           object.value(QStringLiteral("y")).toDouble());
        return decodeMap(object);
    }
    }
    return {};
}

QJsonObject encodeMap(const QVariantMap &map)
{
    QJsonObject object;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        object.insert(it.key(), encode(it.value()));
    return object;
}

QVariantMap decodeMap(const QJsonObject &object)
{
    QVariantMap map;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.key() == QLatin1String(kTypeKey))
            continue;
        map.insert(it.key(), decode(it.value()));
    }
    return map;
}

} // namespace mn::json
