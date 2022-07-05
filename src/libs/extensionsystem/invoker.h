// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.h"

#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaType>
#include <QVarLengthArray>

namespace ExtensionSystem {

class EXTENSIONSYSTEM_EXPORT InvokerBase {
public:
  InvokerBase();
  ~InvokerBase();

  auto wasSuccessful() const -> bool;
  auto setConnectionType(Qt::ConnectionType connectionType) -> void;

  template <class T>
  auto addArgument(const T &t) -> void
  {
    arg[lastArg++] = QGenericArgument(typeName<T>(), &t);
  }

  template <class T>
  auto setReturnValue(T &t) -> void
  {
    useRet = true;
    ret = QGenericReturnArgument(typeName<T>(), &t);
  }

  auto invoke(QObject *target, const char *slot) -> void;

private:
  InvokerBase(const InvokerBase &); // Unimplemented.
  template <class T>

  auto typeName() -> const char*
  {
    return QMetaType::typeName(qMetaTypeId<T>());
  }

  QObject *target;
  QGenericArgument arg[10];
  QGenericReturnArgument ret;
  QVarLengthArray<char, 512> sig;
  int lastArg;
  bool success;
  bool useRet;
  Qt::ConnectionType connectionType;
  mutable bool nag;
};

template <class Result>
class Invoker : public InvokerBase {
public:
  Invoker(QObject *target, const char *slot)
  {
    invoke(target, slot);
  }

  template <class T0>
  Invoker(QObject *target, const char *slot, const T0 &t0)
  {
    setReturnValue(result);
    addArgument(t0);
    invoke(target, slot);
  }

  template <class T0, class T1>
  Invoker(QObject *target, const char *slot, const T0 &t0, const T1 &t1)
  {
    setReturnValue(result);
    addArgument(t0);
    addArgument(t1);
    invoke(target, slot);
  }

  template <class T0, class T1, class T2>
  Invoker(QObject *target, const char *slot, const T0 &t0, const T1 &t1, const T2 &t2)
  {
    setReturnValue(result);
    addArgument(t0);
    addArgument(t1);
    addArgument(t2);
    invoke(target, slot);
  }

  operator Result() const { return result; }

private:
  Result result;
};

template <>
class Invoker<void> : public InvokerBase {
public:
  Invoker(QObject *target, const char *slot)
  {
    invoke(target, slot);
  }

  template <class T0>
  Invoker(QObject *target, const char *slot, const T0 &t0)
  {
    addArgument(t0);
    invoke(target, slot);
  }

  template <class T0, class T1>
  Invoker(QObject *target, const char *slot, const T0 &t0, const T1 &t1)
  {
    addArgument(t0);
    addArgument(t1);
    invoke(target, slot);
  }

  template <class T0, class T1, class T2>
  Invoker(QObject *target, const char *slot, const T0 &t0, const T1 &t1, const T2 &t2)
  {
    addArgument(t0);
    addArgument(t1);
    addArgument(t2);
    invoke(target, slot);
  }
};

#ifndef Q_QDOC
template <class Result>
auto invokeHelper(InvokerBase &in, QObject *target, const char *slot) -> Result
{
  Result result;
  in.setReturnValue(result);
  in.invoke(target, slot);
  return result;
}

template <>
inline auto invokeHelper<void>(InvokerBase &in, QObject *target, const char *slot) -> void
{
  in.invoke(target, slot);
}
#endif

template <class Result>
auto invoke(QObject *target, const char *slot) -> Result
{
  InvokerBase in;
  return invokeHelper<Result>(in, target, slot);
}

template <class Result, class T0>
auto invoke(QObject *target, const char *slot, const T0 &t0) -> Result
{
  InvokerBase in;
  in.addArgument(t0);
  return invokeHelper<Result>(in, target, slot);
}

template <class Result, class T0, class T1>
auto invoke(QObject *target, const char *slot, const T0 &t0, const T1 &t1) -> Result
{
  InvokerBase in;
  in.addArgument(t0);
  in.addArgument(t1);
  return invokeHelper<Result>(in, target, slot);
}

template <class Result, class T0, class T1, class T2>
auto invoke(QObject *target, const char *slot, const T0 &t0, const T1 &t1, const T2 &t2) -> Result
{
  InvokerBase in;
  in.addArgument(t0);
  in.addArgument(t1);
  in.addArgument(t2);
  return invokeHelper<Result>(in, target, slot);
}

} // namespace ExtensionSystem
