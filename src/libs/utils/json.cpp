// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "json.hpp"

#include <utils/qtcassert.hpp>
#include <utils/fileutils.hpp>

#include <QDir>
#include <QDebug>
#include <QJsonDocument>

using namespace Utils;

JsonMemoryPool::~JsonMemoryPool()
{
  for (char *obj : qAsConst(_objs)) {
    reinterpret_cast<JsonValue*>(obj)->~JsonValue();
    delete[] obj;
  }
}

JsonValue::JsonValue(Kind kind) : m_kind(kind) {}

JsonValue::~JsonValue() = default;

auto JsonValue::create(const QString &s, JsonMemoryPool *pool) -> JsonValue*
{
  const QJsonDocument document = QJsonDocument::fromJson(s.toUtf8());
  if (document.isNull())
    return nullptr;

  return build(document.toVariant(), pool);
}

auto JsonValue::operator new(size_t size, JsonMemoryPool *pool) -> void* { return pool->allocate(size); }
auto JsonValue::operator delete(void *) -> void { }
auto JsonValue::operator delete(void *, JsonMemoryPool *) -> void { }

auto JsonValue::kindToString(JsonValue::Kind kind) -> QString
{
  if (kind == String)
    return QLatin1String("string");
  if (kind == Double)
    return QLatin1String("number");
  if (kind == Int)
    return QLatin1String("integer");
  if (kind == Object)
    return QLatin1String("object");
  if (kind == Array)
    return QLatin1String("array");
  if (kind == Boolean)
    return QLatin1String("boolean");
  if (kind == Null)
    return QLatin1String("null");

  return QLatin1String("unknown");
}

auto JsonValue::build(const QVariant &variant, JsonMemoryPool *pool) -> JsonValue*
{
  switch (variant.type()) {

  case QVariant::List: {
    auto newValue = new(pool) JsonArrayValue;
    const QList<QVariant> list = variant.toList();
    for (const QVariant &element : list)
      newValue->addElement(build(element, pool));
    return newValue;
  }

  case QVariant::Map: {
    auto newValue = new(pool) JsonObjectValue;
    const QVariantMap variantMap = variant.toMap();
    for (QVariantMap::const_iterator it = variantMap.begin(); it != variantMap.end(); ++it)
      newValue->addMember(it.key(), build(it.value(), pool));
    return newValue;
  }

  case QVariant::String:
    return new(pool) JsonStringValue(variant.toString());

  case QVariant::Int:
    return new(pool) JsonIntValue(variant.toInt());

  case QVariant::Double:
    return new(pool) JsonDoubleValue(variant.toDouble());

  case QVariant::Bool:
    return new(pool) JsonBooleanValue(variant.toBool());

  case QVariant::Invalid:
    return new(pool) JsonNullValue;

  default:
    break;
  }

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

auto JsonSchema::kType() -> QString { return QStringLiteral("type"); }
auto JsonSchema::kProperties() -> QString { return QStringLiteral("properties"); }
auto JsonSchema::kPatternProperties() -> QString { return QStringLiteral("patternProperties"); }
auto JsonSchema::kAdditionalProperties() -> QString { return QStringLiteral("additionalProperties"); }
auto JsonSchema::kItems() -> QString { return QStringLiteral("items"); }
auto JsonSchema::kAdditionalItems() -> QString { return QStringLiteral("additionalItems"); }
auto JsonSchema::kRequired() -> QString { return QStringLiteral("required"); }
auto JsonSchema::kDependencies() -> QString { return QStringLiteral("dependencies"); }
auto JsonSchema::kMinimum() -> QString { return QStringLiteral("minimum"); }
auto JsonSchema::kMaximum() -> QString { return QStringLiteral("maximum"); }
auto JsonSchema::kExclusiveMinimum() -> QString { return QStringLiteral("exclusiveMinimum"); }
auto JsonSchema::kExclusiveMaximum() -> QString { return QStringLiteral("exclusiveMaximum"); }
auto JsonSchema::kMinItems() -> QString { return QStringLiteral("minItems"); }
auto JsonSchema::kMaxItems() -> QString { return QStringLiteral("maxItems"); }
auto JsonSchema::kUniqueItems() -> QString { return QStringLiteral("uniqueItems"); }
auto JsonSchema::kPattern() -> QString { return QStringLiteral("pattern"); }
auto JsonSchema::kMinLength() -> QString { return QStringLiteral("minLength"); }
auto JsonSchema::kMaxLength() -> QString { return QStringLiteral("maxLength"); }
auto JsonSchema::kTitle() -> QString { return QStringLiteral("title"); }
auto JsonSchema::kDescription() -> QString { return QStringLiteral("description"); }
auto JsonSchema::kExtends() -> QString { return QStringLiteral("extends"); }
auto JsonSchema::kRef() -> QString { return QStringLiteral("$ref"); }

JsonSchema::JsonSchema(JsonObjectValue *rootObject, const JsonSchemaManager *manager) : m_manager(manager)
{
  enter(rootObject);
}

auto JsonSchema::isTypeConstrained() const -> bool
{
  // Simple types
  if (JsonStringValue *sv = getStringValue(kType(), currentValue()))
    return isCheckableType(sv->value());

  // Union types
  if (JsonArrayValue *av = getArrayValue(kType(), currentValue())) {
    QTC_ASSERT(currentIndex() != -1, return false);
    QTC_ASSERT(av->elements().at(currentIndex())->kind() == JsonValue::String, return false);
    JsonStringValue *sv = av->elements().at(currentIndex())->toString();
    return isCheckableType(sv->value());
  }

  return false;
}

auto JsonSchema::acceptsType(const QString &type) const -> bool
{
  // Simple types
  if (JsonStringValue *sv = getStringValue(kType(), currentValue()))
    return typeMatches(sv->value(), type);

  // Union types
  if (JsonArrayValue *av = getArrayValue(kType(), currentValue())) {
    QTC_ASSERT(currentIndex() != -1, return false);
    QTC_ASSERT(av->elements().at(currentIndex())->kind() == JsonValue::String, return false);
    JsonStringValue *sv = av->elements().at(currentIndex())->toString();
    return typeMatches(sv->value(), type);
  }

  return false;
}

auto JsonSchema::validTypes(JsonObjectValue *v) -> QStringList
{
  QStringList all;

  if (JsonStringValue *sv = getStringValue(kType(), v))
    all.append(sv->value());

  if (JsonObjectValue *ov = getObjectValue(kType(), v))
    return validTypes(ov);

  if (JsonArrayValue *av = getArrayValue(kType(), v)) {
    const QList<JsonValue*> elements = av->elements();
    for (JsonValue *v : elements) {
      if (JsonStringValue *sv = v->toString())
        all.append(sv->value());
      else if (JsonObjectValue *ov = v->toObject())
        all.append(validTypes(ov));
    }
  }

  return all;
}

auto JsonSchema::typeMatches(const QString &expected, const QString &actual) -> bool
{
  if (expected == QLatin1String("number") && actual == QLatin1String("integer"))
    return true;

  return expected == actual;
}

auto JsonSchema::isCheckableType(const QString &s) -> bool
{
  return s == QLatin1String("string") || s == QLatin1String("number") || s == QLatin1String("integer") || s == QLatin1String("boolean") || s == QLatin1String("object") || s == QLatin1String("array") || s == QLatin1String("null");
}

auto JsonSchema::validTypes() const -> QStringList
{
  return validTypes(currentValue());
}

auto JsonSchema::hasTypeSchema() const -> bool
{
  return getObjectValue(kType(), currentValue());
}

auto JsonSchema::enterNestedTypeSchema() -> void
{
  QTC_ASSERT(hasTypeSchema(), return);

  enter(getObjectValue(kType(), currentValue()));
}

auto JsonSchema::properties(JsonObjectValue *v) const -> QStringList
{
  using Members = QHash<QString, JsonValue*>;

  QStringList all;

  if (JsonObjectValue *ov = getObjectValue(kProperties(), v)) {
    const Members members = ov->members();
    const Members::ConstIterator cend = members.constEnd();
    for (Members::ConstIterator it = members.constBegin(); it != cend; ++it)
      if (hasPropertySchema(it.key()))
        all.append(it.key());
  }

  if (JsonObjectValue *base = resolveBase(v))
    all.append(properties(base));

  return all;
}

auto JsonSchema::properties() const -> QStringList
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Object)), return QStringList());

  return properties(currentValue());
}

auto JsonSchema::propertySchema(const QString &property, JsonObjectValue *v) const -> JsonObjectValue*
{
  if (JsonObjectValue *ov = getObjectValue(kProperties(), v)) {
    JsonValue *member = ov->member(property);
    if (member && member->kind() == JsonValue::Object)
      return member->toObject();
  }

  if (JsonObjectValue *base = resolveBase(v))
    return propertySchema(property, base);

  return nullptr;
}

auto JsonSchema::hasPropertySchema(const QString &property) const -> bool
{
  return propertySchema(property, currentValue());
}

auto JsonSchema::enterNestedPropertySchema(const QString &property) -> void
{
  QTC_ASSERT(hasPropertySchema(property), return);

  JsonObjectValue *schema = propertySchema(property, currentValue());

  enter(schema);
}

/*!
 * An array schema is allowed to have its \e items specification in the form of
 * another schema
 * or in the form of an array of schemas [Sec. 5.5]. This functions checks whether this is case
 * in which the items are a schema.
 *
 * Returns whether or not the items from the array are a schema.
 */
auto JsonSchema::hasItemSchema() const -> bool
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Array)), return false);

  return getObjectValue(kItems(), currentValue());
}

auto JsonSchema::enterNestedItemSchema() -> void
{
  QTC_ASSERT(hasItemSchema(), return);

  enter(getObjectValue(kItems(), currentValue()));
}

/*!
 * An array schema is allowed to have its \e items specification in the form of another schema
 * or in the form of an array of schemas [Sec. 5.5]. This functions checks whether this is case
 * in which the items are an array of schemas.
 *
 * Returns whether or not the items from the array are a an array of schemas.
 */
auto JsonSchema::hasItemArraySchema() const -> bool
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Array)), return false);

  return getArrayValue(kItems(), currentValue());
}

auto JsonSchema::itemArraySchemaSize() const -> int
{
  QTC_ASSERT(hasItemArraySchema(), return false);

  return getArrayValue(kItems(), currentValue())->size();
}

/*!
 * When evaluating the items of an array it might be necessary to \e enter a
 * particular schema,
 * since this API assumes that there's always a valid schema in context (the one the user is
 * interested on). This shall only happen if the item at the supplied array index is of type
 * object, which is then assumed to be a schema.
 *
 * The function also marks the context as being inside an array evaluation.
 *
 * Returns whether it was necessary to enter a schema for the supplied
 * array \a index, false if index is out of bounds.
 */
auto JsonSchema::maybeEnterNestedArraySchema(int index) -> bool
{
  QTC_ASSERT(itemArraySchemaSize(), return false);
  QTC_ASSERT(index >= 0 && index < itemArraySchemaSize(), return false);

  JsonValue *v = getArrayValue(kItems(), currentValue())->elements().at(index);

  return maybeEnter(v, Array, index);
}

/*!
 * The type of a schema can be specified in the form of a union type, which is basically an
 * array of allowed types for the particular instance [Sec. 5.1]. This function checks whether
 * the current schema is one of such.
 *
 * Returns whether or not the current schema specifies a union type.
 */
auto JsonSchema::hasUnionSchema() const -> bool
{
  return getArrayValue(kType(), currentValue());
}

auto JsonSchema::unionSchemaSize() const -> int
{
  return getArrayValue(kType(), currentValue())->size();
}

/*!
 * When evaluating union types it might be necessary to enter a particular
 * schema, since this
 * API assumes that there's always a valid schema in context (the one the user is interested on).
 * This shall only happen if the item at the supplied union \a index, which is then assumed to be
 * a schema.
 *
 * The function also marks the context as being inside an union evaluation.
 *
 * Returns whether or not it was necessary to enter a schema for the
 * supplied union index.
 */
auto JsonSchema::maybeEnterNestedUnionSchema(int index) -> bool
{
  QTC_ASSERT(unionSchemaSize(), return false);
  QTC_ASSERT(index >= 0 && index < unionSchemaSize(), return false);

  JsonValue *v = getArrayValue(kType(), currentValue())->elements().at(index);

  return maybeEnter(v, Union, index);
}

auto JsonSchema::leaveNestedSchema() -> void
{
  QTC_ASSERT(!m_schemas.isEmpty(), return);

  leave();
}

auto JsonSchema::required() const -> bool
{
  if (JsonBooleanValue *bv = getBooleanValue(kRequired(), currentValue()))
    return bv->value();

  return false;
}

auto JsonSchema::hasMinimum() const -> bool
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Int)), return false);

  return getDoubleValue(kMinimum(), currentValue());
}

auto JsonSchema::minimum() const -> double
{
  QTC_ASSERT(hasMinimum(), return 0);

  return getDoubleValue(kMinimum(), currentValue())->value();
}

auto JsonSchema::hasExclusiveMinimum() -> bool
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Int)), return false);

  if (JsonBooleanValue *bv = getBooleanValue(kExclusiveMinimum(), currentValue()))
    return bv->value();

  return false;
}

auto JsonSchema::hasMaximum() const -> bool
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Int)), return false);

  return getDoubleValue(kMaximum(), currentValue());
}

auto JsonSchema::maximum() const -> double
{
  QTC_ASSERT(hasMaximum(), return 0);

  return getDoubleValue(kMaximum(), currentValue())->value();
}

auto JsonSchema::hasExclusiveMaximum() -> bool
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Int)), return false);

  if (JsonBooleanValue *bv = getBooleanValue(kExclusiveMaximum(), currentValue()))
    return bv->value();

  return false;
}

auto JsonSchema::pattern() const -> QString
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::String)), return QString());

  if (JsonStringValue *sv = getStringValue(kPattern(), currentValue()))
    return sv->value();

  return QString();
}

auto JsonSchema::minimumLength() const -> int
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::String)), return -1);

  if (JsonDoubleValue *dv = getDoubleValue(kMinLength(), currentValue()))
    return dv->value();

  return -1;
}

auto JsonSchema::maximumLength() const -> int
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::String)), return -1);

  if (JsonDoubleValue *dv = getDoubleValue(kMaxLength(), currentValue()))
    return dv->value();

  return -1;
}

auto JsonSchema::hasAdditionalItems() const -> bool
{
  QTC_ASSERT(acceptsType(JsonValue::kindToString(JsonValue::Array)), return false);

  return currentValue()->member(kAdditionalItems());
}

auto JsonSchema::maybeSchemaName(const QString &s) -> bool
{
  if (s.isEmpty() || s == QLatin1String("any"))
    return false;

  return !isCheckableType(s);
}

auto JsonSchema::rootValue() const -> JsonObjectValue*
{
  QTC_ASSERT(!m_schemas.isEmpty(), return nullptr);

  return m_schemas.first().m_value;
}

auto JsonSchema::currentValue() const -> JsonObjectValue*
{
  QTC_ASSERT(!m_schemas.isEmpty(), return nullptr);

  return m_schemas.last().m_value;
}

auto JsonSchema::currentIndex() const -> int
{
  QTC_ASSERT(!m_schemas.isEmpty(), return 0);

  return m_schemas.last().m_index;
}

auto JsonSchema::evaluate(EvaluationMode eval, int index) -> void
{
  QTC_ASSERT(!m_schemas.isEmpty(), return);

  m_schemas.last().m_eval = eval;
  m_schemas.last().m_index = index;
}

auto JsonSchema::enter(JsonObjectValue *ov, EvaluationMode eval, int index) -> void
{
  Context context;
  context.m_eval = eval;
  context.m_index = index;
  context.m_value = resolveReference(ov);

  m_schemas.push_back(context);
}

auto JsonSchema::maybeEnter(JsonValue *v, EvaluationMode eval, int index) -> bool
{
  evaluate(eval, index);

  if (v->kind() == JsonValue::Object) {
    enter(v->toObject());
    return true;
  }

  if (v->kind() == JsonValue::String) {
    const QString &s = v->toString()->value();
    if (maybeSchemaName(s)) {
      JsonSchema *schema = m_manager->schemaByName(s);
      if (schema) {
        enter(schema->rootValue());
        return true;
      }
    }
  }

  return false;
}

auto JsonSchema::leave() -> void
{
  QTC_ASSERT(!m_schemas.isEmpty(), return);

  m_schemas.pop_back();
}

auto JsonSchema::resolveReference(JsonObjectValue *ov) const -> JsonObjectValue*
{
  if (JsonStringValue *sv = getStringValue(kRef(), ov)) {
    JsonSchema *referenced = m_manager->schemaByName(sv->value());
    if (referenced)
      return referenced->rootValue();
  }

  return ov;
}

auto JsonSchema::resolveBase(JsonObjectValue *ov) const -> JsonObjectValue*
{
  if (JsonValue *v = ov->member(kExtends())) {
    if (v->kind() == JsonValue::String) {
      JsonSchema *schema = m_manager->schemaByName(v->toString()->value());
      if (schema)
        return schema->rootValue();
    } else if (v->kind() == JsonValue::Object) {
      return resolveReference(v->toObject());
    }
  }

  return nullptr;
}

auto JsonSchema::getStringValue(const QString &name, JsonObjectValue *value) -> JsonStringValue*
{
  JsonValue *v = value->member(name);
  if (!v)
    return nullptr;

  return v->toString();
}

auto JsonSchema::getObjectValue(const QString &name, JsonObjectValue *value) -> JsonObjectValue*
{
  JsonValue *v = value->member(name);
  if (!v)
    return nullptr;

  return v->toObject();
}

auto JsonSchema::getBooleanValue(const QString &name, JsonObjectValue *value) -> JsonBooleanValue*
{
  JsonValue *v = value->member(name);
  if (!v)
    return nullptr;

  return v->toBoolean();
}

auto JsonSchema::getArrayValue(const QString &name, JsonObjectValue *value) -> JsonArrayValue*
{
  JsonValue *v = value->member(name);
  if (!v)
    return nullptr;

  return v->toArray();
}

auto JsonSchema::getDoubleValue(const QString &name, JsonObjectValue *value) -> JsonDoubleValue*
{
  JsonValue *v = value->member(name);
  if (!v)
    return nullptr;

  return v->toDouble();
}

///////////////////////////////////////////////////////////////////////////////

JsonSchemaManager::JsonSchemaManager(const QStringList &searchPaths) : m_searchPaths(searchPaths)
{
  for (const QString &path : searchPaths) {
    QDir dir(path);
    if (!dir.exists())
      continue;
    dir.setNameFilters(QStringList(QLatin1String("*.json")));
    const QList<QFileInfo> entries = dir.entryInfoList();
    for (const QFileInfo &fi : entries)
      m_schemas.insert(fi.baseName(), JsonSchemaData(fi.absoluteFilePath()));
  }
}

JsonSchemaManager::~JsonSchemaManager()
{
  for (const JsonSchemaData &schemaData : qAsConst(m_schemas))
    delete schemaData.m_schema;
}

/*!
 * Tries to find a JSON schema to validate \a fileName against. According
 * to the specification, how the schema/instance association is done is implementation defined.
 * Currently we use a quite naive approach which is simply based on file names. Specifically,
 * if one opens a foo.json file we'll look for a schema named foo.json. We should probably
 * investigate alternative settings later.
 *
 * Returns a valid schema or 0.
 */
auto JsonSchemaManager::schemaForFile(const QString &fileName) const -> JsonSchema*
{
  QString baseName(QFileInfo(fileName).baseName());

  return schemaByName(baseName);
}

auto JsonSchemaManager::schemaByName(const QString &baseName) const -> JsonSchema*
{
  QHash<QString, JsonSchemaData>::iterator it = m_schemas.find(baseName);
  if (it == m_schemas.end()) {
    for (const QString &path : m_searchPaths) {
      QFileInfo candidate(path + baseName + ".json");
      if (candidate.exists()) {
        m_schemas.insert(baseName, candidate.absoluteFilePath());
        break;
      }
    }
  }

  it = m_schemas.find(baseName);
  if (it == m_schemas.end())
    return nullptr;

  JsonSchemaData *schemaData = &it.value();
  if (!schemaData->m_schema) {
    // Schemas are built on-demand.
    QFileInfo currentSchema(schemaData->m_absoluteFileName);
    Q_ASSERT(currentSchema.exists());
    if (schemaData->m_lastParseAttempt.isNull() || schemaData->m_lastParseAttempt < currentSchema.lastModified()) {
      schemaData->m_schema = parseSchema(currentSchema.absoluteFilePath());
    }
  }

  return schemaData->m_schema;
}

auto JsonSchemaManager::parseSchema(const QString &schemaFileName) const -> JsonSchema*
{
  FileReader reader;
  if (reader.fetch(FilePath::fromString(schemaFileName), QIODevice::Text)) {
    const QString &contents = QString::fromUtf8(reader.data());
    JsonValue *json = JsonValue::create(contents, &m_pool);
    if (json && json->kind() == JsonValue::Object)
      return new JsonSchema(json->toObject(), this);
  }

  return nullptr;
}
