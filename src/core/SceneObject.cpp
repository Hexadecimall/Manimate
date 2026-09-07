#include "SceneObject.h"

#include "Json.h"

namespace mn {

QJsonObject SceneObject::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), qint64(id));
    object.insert(QStringLiteral("type"), type);
    object.insert(QStringLiteral("name"), name);
    if (parentId != kInvalidObjectId)
        object.insert(QStringLiteral("parent"), qint64(parentId));
    object.insert(QStringLiteral("params"), json::encodeMap(params));
    object.insert(QStringLiteral("z"), zOrder);
    object.insert(QStringLiteral("visible"), visible);
    object.insert(QStringLiteral("locked"), locked);
    if (!rawPython.isEmpty())
        object.insert(QStringLiteral("raw"), rawPython);
    return object;
}

SceneObject SceneObject::fromJson(const QJsonObject &object)
{
    SceneObject result;
    result.id = ObjectId(object.value(QStringLiteral("id")).toInteger());
    result.type = object.value(QStringLiteral("type")).toString();
    result.name = object.value(QStringLiteral("name")).toString();
    result.parentId = ObjectId(object.value(QStringLiteral("parent")).toInteger(0));
    result.params = json::decodeMap(object.value(QStringLiteral("params")).toObject());
    result.zOrder = object.value(QStringLiteral("z")).toInt();
    result.visible = object.value(QStringLiteral("visible")).toBool(true);
    result.locked = object.value(QStringLiteral("locked")).toBool(false);
    result.rawPython = object.value(QStringLiteral("raw")).toString();
    return result;
}

} // namespace mn
