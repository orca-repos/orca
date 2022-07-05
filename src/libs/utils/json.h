// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QHash>
#include <QVector>
#include <QStringList>
#include <QDateTime>

QT_FORWARD_DECLARE_CLASS(QVariant)

namespace Utils {

class JsonStringValue;
class JsonDoubleValue;
class JsonIntValue;
class JsonObjectValue;
class JsonArrayValue;
class JsonBooleanValue;
class JsonNullValue;

class ORCA_UTILS_EXPORT JsonMemoryPool {
public:
  ~JsonMemoryPool();

  auto allocate(size_t size) -> void*
  {
    auto obj = new char[size];
    _objs.append(obj);
    return obj;
  }

private:
  QVector<char*> _objs;
};

/*!
 * \brief The JsonValue class
 */
class ORCA_UTILS_EXPORT JsonValue {
public:
  enum Kind {
    String,
    Double,
    Int,
    Object,
    Array,
    Boolean,
    Null,
    Unknown
  };

  virtual ~JsonValue();

  auto kind() const -> Kind { return m_kind; }
  static auto kindToString(Kind kind) -> QString;
  virtual auto toString() -> JsonStringValue* { return nullptr; }
  virtual auto toDouble() -> JsonDoubleValue* { return nullptr; }
  virtual auto toInt() -> JsonIntValue* { return nullptr; }
  virtual auto toObject() -> JsonObjectValue* { return nullptr; }
  virtual auto toArray() -> JsonArrayValue* { return nullptr; }
  virtual auto toBoolean() -> JsonBooleanValue* { return nullptr; }
  virtual auto toNull() -> JsonNullValue* { return nullptr; }
  static auto create(const QString &s, JsonMemoryPool *pool) -> JsonValue*;
  auto operator new(size_t size, JsonMemoryPool *pool) -> void*;
  auto operator delete(void *) -> void;
  auto operator delete(void *, JsonMemoryPool *) -> void;

protected:
  JsonValue(Kind kind);

private:
  static auto build(const QVariant &varixant, JsonMemoryPool *pool) -> JsonValue*;

  Kind m_kind;
};


/*!
 * \brief The JsonStringValue class
 */
class ORCA_UTILS_EXPORT JsonStringValue : public JsonValue {
public:
  JsonStringValue(const QString &value) : JsonValue(String), m_value(value) {}
  auto toString() -> JsonStringValue* override { return this; }
  auto value() const -> const QString& { return m_value; }

private:
  QString m_value;
};


/*!
 * \brief The JsonDoubleValue class
 */
class ORCA_UTILS_EXPORT JsonDoubleValue : public JsonValue {
public:
  JsonDoubleValue(double value) : JsonValue(Double), m_value(value) {}
  auto toDouble() -> JsonDoubleValue* override { return this; }
  auto value() const -> double { return m_value; }

private:
  double m_value;
};

/*!
 * \brief The JsonIntValue class
 */
class ORCA_UTILS_EXPORT JsonIntValue : public JsonValue {
public:
  JsonIntValue(int value) : JsonValue(Int), m_value(value) {}

  auto toInt() -> JsonIntValue* override { return this; }
  auto value() const -> int { return m_value; }

private:
  int m_value;
};


/*!
 * \brief The JsonObjectValue class
 */
class ORCA_UTILS_EXPORT JsonObjectValue : public JsonValue {
public:
  JsonObjectValue() : JsonValue(Object) {}

  auto toObject() -> JsonObjectValue* override { return this; }
  auto addMember(const QString &name, JsonValue *value) -> void { m_members.insert(name, value); }
  auto hasMember(const QString &name) const -> bool { return m_members.contains(name); }
  auto member(const QString &name) const -> JsonValue* { return m_members.value(name); }
  auto members() const -> QHash<QString, JsonValue*> { return m_members; }
  auto isEmpty() const -> bool { return m_members.isEmpty(); }

protected:
  JsonObjectValue(Kind kind) : JsonValue(kind) {}

private:
  QHash<QString, JsonValue*> m_members;
};


/*!
 * \brief The JsonArrayValue class
 */
class ORCA_UTILS_EXPORT JsonArrayValue : public JsonValue {
public:
  JsonArrayValue() : JsonValue(Array) {}

  auto toArray() -> JsonArrayValue* override { return this; }
  auto addElement(JsonValue *value) -> void { m_elements.append(value); }
  auto elements() const -> QList<JsonValue*> { return m_elements; }
  auto size() const -> int { return m_elements.size(); }

private:
  QList<JsonValue*> m_elements;
};


/*!
 * \brief The JsonBooleanValue class
 */
class ORCA_UTILS_EXPORT JsonBooleanValue : public JsonValue {
public:
  JsonBooleanValue(bool value) : JsonValue(Boolean), m_value(value) {}

  auto toBoolean() -> JsonBooleanValue* override { return this; }
  auto value() const -> bool { return m_value; }

private:
  bool m_value;
};

class ORCA_UTILS_EXPORT JsonNullValue : public JsonValue {
public:
  JsonNullValue() : JsonValue(Null) {}

  auto toNull() -> JsonNullValue* override { return this; }
};

class JsonSchemaManager;

/*!
 * \brief The JsonSchema class
 *
 * [NOTE: This is an incomplete implementation and a work in progress.]
 *
 * This class provides an interface for traversing and evaluating a JSON schema, as described
 * in the draft http://tools.ietf.org/html/draft-zyp-json-schema-03.
 *
 * JSON schemas are recursive in concept. This means that a particular attribute from a schema
 * might be also another schema. Therefore, the basic working principle of this API is that
 * from within some schema, one can investigate its attributes and if necessary "enter" a
 * corresponding nested schema. Afterwards, it's expected that one would "leave" such nested
 * schema.
 *
 * All functions assume that the current "context" is a valid schema. Once an instance of this
 * class is created the root schema is put on top of the stack.
 *
 */
class ORCA_UTILS_EXPORT JsonSchema {
public:
  auto isTypeConstrained() const -> bool;
  auto acceptsType(const QString &type) const -> bool;
  auto validTypes() const -> QStringList;

  // Applicable on schemas of any type.
  auto required() const -> bool;
  auto hasTypeSchema() const -> bool;
  auto enterNestedTypeSchema() -> void;
  auto hasUnionSchema() const -> bool;
  auto unionSchemaSize() const -> int;
  auto maybeEnterNestedUnionSchema(int index) -> bool;
  auto leaveNestedSchema() -> void;

  // Applicable on schemas of type number/integer.
  auto hasMinimum() const -> bool;
  auto hasMaximum() const -> bool;
  auto hasExclusiveMinimum() -> bool;
  auto hasExclusiveMaximum() -> bool;
  auto minimum() const -> double;
  auto maximum() const -> double;

  // Applicable on schemas of type string.
  auto pattern() const -> QString;
  auto minimumLength() const -> int;
  auto maximumLength() const -> int;

  // Applicable on schemas of type object.
  auto properties() const -> QStringList;
  auto hasPropertySchema(const QString &property) const -> bool;
  auto enterNestedPropertySchema(const QString &property) -> void;

  // Applicable on schemas of type array.
  auto hasAdditionalItems() const -> bool;
  auto hasItemSchema() const -> bool;
  auto enterNestedItemSchema() -> void;
  auto hasItemArraySchema() const -> bool;
  auto itemArraySchemaSize() const -> int;
  auto maybeEnterNestedArraySchema(int index) -> bool;

private:
  friend class JsonSchemaManager;
  JsonSchema(JsonObjectValue *rootObject, const JsonSchemaManager *manager);
  Q_DISABLE_COPY(JsonSchema)

  enum EvaluationMode {
    Normal,
    Array,
    Union
  };

  auto enter(JsonObjectValue *ov, EvaluationMode eval = Normal, int index = -1) -> void;
  auto maybeEnter(JsonValue *v, EvaluationMode eval, int index) -> bool;
  auto evaluate(EvaluationMode eval, int index) -> void;
  auto leave() -> void;
  auto resolveReference(JsonObjectValue *ov) const -> JsonObjectValue*;
  auto resolveBase(JsonObjectValue *ov) const -> JsonObjectValue*;
  auto currentValue() const -> JsonObjectValue*;
  auto currentIndex() const -> int;
  auto rootValue() const -> JsonObjectValue*;

  static auto getStringValue(const QString &name, JsonObjectValue *value) -> JsonStringValue*;
  static auto getObjectValue(const QString &name, JsonObjectValue *value) -> JsonObjectValue*;
  static auto getBooleanValue(const QString &name, JsonObjectValue *value) -> JsonBooleanValue*;
  static auto getArrayValue(const QString &name, JsonObjectValue *value) -> JsonArrayValue*;
  static auto getDoubleValue(const QString &name, JsonObjectValue *value) -> JsonDoubleValue*;

  static auto validTypes(JsonObjectValue *v) -> QStringList;
  static auto typeMatches(const QString &expected, const QString &actual) -> bool;
  static auto isCheckableType(const QString &s) -> bool;

  auto properties(JsonObjectValue *v) const -> QStringList;
  auto propertySchema(const QString &property, JsonObjectValue *v) const -> JsonObjectValue*;
  // TODO: Similar functions for other attributes which require looking into base schemas.

  static auto maybeSchemaName(const QString &s) -> bool;

  static auto kType() -> QString;
  static auto kProperties() -> QString;
  static auto kPatternProperties() -> QString;
  static auto kAdditionalProperties() -> QString;
  static auto kItems() -> QString;
  static auto kAdditionalItems() -> QString;
  static auto kRequired() -> QString;
  static auto kDependencies() -> QString;
  static auto kMinimum() -> QString;
  static auto kMaximum() -> QString;
  static auto kExclusiveMinimum() -> QString;
  static auto kExclusiveMaximum() -> QString;
  static auto kMinItems() -> QString;
  static auto kMaxItems() -> QString;
  static auto kUniqueItems() -> QString;
  static auto kPattern() -> QString;
  static auto kMinLength() -> QString;
  static auto kMaxLength() -> QString;
  static auto kTitle() -> QString;
  static auto kDescription() -> QString;
  static auto kExtends() -> QString;
  static auto kRef() -> QString;

  struct Context {
    JsonObjectValue *m_value;
    EvaluationMode m_eval;
    int m_index;
  };

  QVector<Context> m_schemas;
  const JsonSchemaManager *m_manager;
};


/*!
 * \brief The JsonSchemaManager class
 */
class ORCA_UTILS_EXPORT JsonSchemaManager {
public:
  JsonSchemaManager(const QStringList &searchPaths);
  ~JsonSchemaManager();

  auto schemaForFile(const QString &fileName) const -> JsonSchema*;
  auto schemaByName(const QString &baseName) const -> JsonSchema*;

private:
  struct JsonSchemaData {
    JsonSchemaData(const QString &absoluteFileName, JsonSchema *schema = nullptr) : m_absoluteFileName(absoluteFileName), m_schema(schema) {}
    QString m_absoluteFileName;
    JsonSchema *m_schema;
    QDateTime m_lastParseAttempt;
  };

  auto parseSchema(const QString &schemaFileName) const -> JsonSchema*;

  QStringList m_searchPaths;
  mutable QHash<QString, JsonSchemaData> m_schemas;
  mutable JsonMemoryPool m_pool;
};

} // namespace Utils
