// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "functiontraits.hpp"
#include "optional.hpp"
#include "utils_global.hpp"

#include <QCoreApplication>
#include <QFuture>
#include <QFutureInterface>
#include <QFutureWatcher>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>

#include <functional>

// hasCallOperator & Co must be outside of any namespace
// because of internal compiler error with MSVC2015 Update 2

using testCallOperatorYes = char;
using testCallOperatorNo = struct { char foo[2]; };

template <typename C>
static auto testCallOperator(decltype(&C::operator())) -> testCallOperatorYes;

template <typename>
static auto testCallOperator(...) -> testCallOperatorNo;

template <typename T>
struct hasCallOperator {
  static const bool value = (sizeof(testCallOperator<T>(nullptr)) == sizeof(testCallOperatorYes));
};

namespace Utils {

using StackSizeInBytes = optional<uint>;

namespace Internal {

/*
   resultType<F>::type

   Returns the type of results that would be reported by a callable of type F
   when called through the runAsync methods. I.e. the ResultType in

   void f(QFutureInterface<Result> &fi, ...)
   ResultType f(...)

   Returns void if F is not callable, and if F is a callable that does not take
   a QFutureInterface& as its first parameter and returns void.
*/

template <typename Function>
struct resultType;

template <typename Function, typename Arg>
struct resultTypeWithArgument;

template <typename Function, int index, bool>
struct resultTypeTakesArguments;

template <typename Function, bool>
struct resultTypeIsMemberFunction;

template <typename Function, bool>
struct resultTypeIsFunctionLike;

template <typename Function, bool>
struct resultTypeHasCallOperator;

template <typename Function, typename ResultType>
struct resultTypeWithArgument<Function, QFutureInterface<ResultType>&> {
  using type = ResultType;
};

template <typename Function, typename Arg>
struct resultTypeWithArgument {
  using type = functionResult_t<Function>;
};

template <typename Function, int index>
struct resultTypeTakesArguments<Function, index, true> : public resultTypeWithArgument<Function, typename functionTraits<Function>::template argument<index>::type> {};

template <typename Function, int index>
struct resultTypeTakesArguments<Function, index, false> {
  using type = functionResult_t<Function>;
};

template <typename Function>
struct resultTypeIsFunctionLike<Function, true> : public resultTypeTakesArguments<Function, 0, (functionTraits<Function>::arity > 0)> {};

template <typename Function>
struct resultTypeIsMemberFunction<Function, true> : public resultTypeTakesArguments<Function, 1, (functionTraits<Function>::arity > 1)> {};

template <typename Function>
struct resultTypeIsMemberFunction<Function, false> {
  using type = void;
};

template <typename Function>
struct resultTypeIsFunctionLike<Function, false> : public resultTypeIsMemberFunction<Function, std::is_member_function_pointer<Function>::value> {};

template <typename Function>
struct resultTypeHasCallOperator<Function, false> : public resultTypeIsFunctionLike<Function, std::is_function<std::remove_pointer_t<std::decay_t<Function>>>::value> {};

template <typename Callable>
struct resultTypeHasCallOperator<Callable, true> : public resultTypeTakesArguments<Callable, 0, (functionTraits<Callable>::arity > 0)> {};

template <typename Function>
struct resultType : public resultTypeHasCallOperator<Function, hasCallOperator<Function>::value> {};

template <typename Function>
struct resultType<Function&> : public resultType<Function> {};

template <typename Function>
struct resultType<const Function&> : public resultType<Function> {};

template <typename Function>
struct resultType<Function&&> : public resultType<Function> {};

template <typename Function>
struct resultType<std::reference_wrapper<Function>> : public resultType<Function> {};

template <typename Function>
struct resultType<std::reference_wrapper<const Function>> : public resultType<Function> {};

/*
   Callable object that wraps a member function pointer with the object it
   will be called on.
*/

template <typename Function>
class MemberCallable;

template <typename Result, typename Obj, typename... Args>
class MemberCallable<Result(Obj::*)(Args ...) const> {
public:
  MemberCallable(Result (Obj::* function)(Args ...) const, const Obj *obj) : m_function(function), m_obj(obj) { }

  auto operator()(Args &&... args) const -> Result
  {
    return ((*m_obj).*m_function)(std::forward<Args>(args)...);
  }

private:
  Result (Obj::* m_function)(Args ...) const;
  const Obj *m_obj;
};

template <typename Result, typename Obj, typename... Args>
class MemberCallable<Result(Obj::*)(Args ...)> {
public:
  MemberCallable(Result (Obj::* function)(Args ...), Obj *obj) : m_function(function), m_obj(obj) { }

  auto operator()(Args &&... args) const -> Result
  {
    return ((*m_obj).*m_function)(std::forward<Args>(args)...);
  }

private:
  Result (Obj::* m_function)(Args ...);
  Obj *m_obj;
};

/*
   Helper functions for runAsync that run in the started thread.
*/

// void function that does not take QFutureInterface
template <typename ResultType, typename Function, typename... Args>
auto runAsyncReturnVoidDispatch(std::true_type, QFutureInterface<ResultType> &, Function &&function, Args &&... args) -> void
{
  function(std::forward<Args>(args)...);
}

// non-void function that does not take QFutureInterface
template <typename ResultType, typename Function, typename... Args>
auto runAsyncReturnVoidDispatch(std::false_type, QFutureInterface<ResultType> &futureInterface, Function &&function, Args &&... args) -> void
{
  futureInterface.reportResult(function(std::forward<Args>(args)...));
}

// function that takes QFutureInterface
template <typename ResultType, typename Function, typename... Args>
auto runAsyncQFutureInterfaceDispatch(std::true_type, QFutureInterface<ResultType> &futureInterface, Function &&function, Args &&... args) -> void
{
  function(futureInterface, std::forward<Args>(args)...);
}

// function that does not take QFutureInterface
template <typename ResultType, typename Function, typename... Args>
auto runAsyncQFutureInterfaceDispatch(std::false_type, QFutureInterface<ResultType> &futureInterface, Function &&function, Args &&... args) -> void
{
  runAsyncReturnVoidDispatch(std::is_void<std::invoke_result_t<Function, Args...>>(), futureInterface, std::forward<Function>(function), std::forward<Args>(args)...);
}

// function, function pointer, or other callable object that is no member pointer
template <typename ResultType, typename Function, typename... Args, typename = std::enable_if_t<!std::is_member_pointer<std::decay_t<Function>>::value>>
auto runAsyncMemberDispatch(QFutureInterface<ResultType> &futureInterface, Function &&function, Args &&... args) -> void
{
  runAsyncQFutureInterfaceDispatch(functionTakesArgument<Function, 0, QFutureInterface<ResultType>&>(), futureInterface, std::forward<Function>(function), std::forward<Args>(args)...);
}

// Function = member function
template <typename ResultType, typename Function, typename Obj, typename... Args, typename = std::enable_if_t<std::is_member_pointer<std::decay_t<Function>>::value>>
auto runAsyncMemberDispatch(QFutureInterface<ResultType> &futureInterface, Function &&function, Obj &&obj, Args &&... args) -> void
{
  // Wrap member function with object into callable
  runAsyncImpl(futureInterface, MemberCallable<std::decay_t<Function>>(std::forward<Function>(function), std::forward<Obj>(obj)), std::forward<Args>(args)...);
}

// cref to function/callable
template <typename ResultType, typename Function, typename... Args>
auto runAsyncImpl(QFutureInterface<ResultType> &futureInterface, std::reference_wrapper<Function> functionWrapper, Args &&... args) -> void
{
  runAsyncMemberDispatch(futureInterface, functionWrapper.get(), std::forward<Args>(args)...);
}

// function/callable, no cref
template <typename ResultType, typename Function, typename... Args>
auto runAsyncImpl(QFutureInterface<ResultType> &futureInterface, Function &&function, Args &&... args) -> void
{
  runAsyncMemberDispatch(futureInterface, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*
   AsyncJob is a QRunnable that wraps a function with the
   arguments that are passed to it when it is run in a thread.
*/

template <class T>
auto decayCopy(T &&v) -> std::decay_t<T>
{
  return std::forward<T>(v);
}

template <typename ResultType, typename Function, typename... Args>
class AsyncJob : public QRunnable {
public:
  AsyncJob(Function &&function, Args &&... args)
  // decay copy like std::thread
    : data(decayCopy(std::forward<Function>(function)), decayCopy(std::forward<Args>(args))...)
  {
    // we need to report it as started even though it isn't yet, because someone might
    // call waitForFinished on the future, which does _not_ block if the future is not started
    futureInterface.setRunnable(this);
    futureInterface.reportStarted();
  }

  ~AsyncJob() override
  {
    // QThreadPool can delete runnables even if they were never run (e.g. QThreadPool::clear).
    // Since we reported them as started, we make sure that we always report them as finished.
    // reportFinished only actually sends the signal if it wasn't already finished.
    futureInterface.reportFinished();
  }

  auto future() -> QFuture<ResultType> { return futureInterface.future(); }

  auto run() -> void override
  {
    if (priority != QThread::InheritPriority)
      if (QThread *thread = QThread::currentThread())
        if (thread != qApp->thread())
          thread->setPriority(priority);
    if (futureInterface.isCanceled()) {
      futureInterface.reportFinished();
      return;
    }
    runHelper(std::make_index_sequence<std::tuple_size<Data>::value>());
  }

  auto setThreadPool(QThreadPool *pool) -> void
  {
    futureInterface.setThreadPool(pool);
  }

  auto setThreadPriority(QThread::Priority p) -> void
  {
    priority = p;
  }

private:
  using Data = std::tuple<std::decay_t<Function>, std::decay_t<Args>...>;

  template <std::size_t... index>
  auto runHelper(std::index_sequence<index...>) -> void
  {
    // invalidates data, which is moved into the call
    runAsyncImpl(futureInterface, std::move(std::get<index>(data))...);
    if (futureInterface.isPaused())
      futureInterface.waitForResume();
    futureInterface.reportFinished();
  }

  Data data;
  QFutureInterface<ResultType> futureInterface;
  QThread::Priority priority = QThread::InheritPriority;
};

class ORCA_UTILS_EXPORT RunnableThread : public QThread {
public:
  explicit RunnableThread(QRunnable *runnable, QObject *parent = nullptr);

protected:
  auto run() -> void override;

private:
  QRunnable *m_runnable;
};

template <typename Function, typename... Args, typename ResultType = typename Internal::resultType<Function>::type>
auto runAsync_internal(QThreadPool *pool, StackSizeInBytes stackSize, QThread::Priority priority, Function &&function, Args &&... args) -> QFuture<ResultType>
{
  Q_ASSERT(!(pool && stackSize)); // stack size cannot be changed once a thread is started
  auto job = new Internal::AsyncJob<ResultType, Function, Args...>(std::forward<Function>(function), std::forward<Args>(args)...);
  job->setThreadPriority(priority);
  QFuture<ResultType> future = job->future();
  if (pool) {
    job->setThreadPool(pool);
    pool->start(job);
  } else {
    auto thread = new Internal::RunnableThread(job);
    if (stackSize)
      thread->setStackSize(stackSize.value());
    thread->moveToThread(qApp->thread()); // make sure thread gets deleteLater on main thread
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start(priority);
  }
  return future;
}

} // Internal

/*!
    The interface of \c {runAsync} is similar to the std::thread constructor and \c {std::invoke}.

    The \a function argument can be a member function,
    an object with \c {operator()} (with no overloads),
    a \c {std::function}, lambda, function pointer or function reference.
    The \a args are passed to the function call after they are copied/moved to the thread.

    The \a function can take a \c {QFutureInterface<ResultType>&} as its first argument, followed by
    other custom arguments which need to be passed to this function.
    If it does not take a \c {QFutureInterface<ResultType>&} as its first argument
    and its return type is not void, the function call's result is reported to the QFuture.
    If \a function is a (non-static) member function, the first argument in \a args is expected
    to be the object that the function is called on.

    If a thread \a pool is given, the function is run there. Otherwise a new, independent thread
    is started.

    \sa std::thread
    \sa std::invoke
    \sa QThreadPool
    \sa QThread::Priority
 */
template <typename Function, typename... Args, typename ResultType = typename Internal::resultType<Function>::type>
auto runAsync(QThreadPool *pool, QThread::Priority priority, Function &&function, Args &&... args) -> QFuture<ResultType>
{
  return Internal::runAsync_internal(pool, StackSizeInBytes(), priority, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*!
    Runs \a function with \a args in a new thread with given thread \a priority.
    \sa runAsync(QThreadPool*,QThread::Priority,Function&&,Args&&...)
    \sa QThread::Priority
 */
template <typename Function, typename... Args, typename ResultType = typename Internal::resultType<Function>::type>
auto runAsync(QThread::Priority priority, Function &&function, Args &&... args) -> QFuture<ResultType>
{
  return runAsync(static_cast<QThreadPool*>(nullptr), priority, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*!
    Runs \a function with \a args in a new thread with given thread \a stackSize and
    thread priority QThread::InheritPriority .
    \sa runAsync(QThreadPool*,QThread::Priority,Function&&,Args&&...)
    \sa QThread::Priority
    \sa QThread::setStackSize
*/
template <typename Function, typename... Args, typename ResultType = typename Internal::resultType<Function>::type>
auto runAsync(Utils::StackSizeInBytes stackSize, Function &&function, Args &&... args) -> QFuture<ResultType>
{
  return Internal::runAsync_internal(static_cast<QThreadPool*>(nullptr), stackSize, QThread::InheritPriority, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*!
    Runs \a function with \a args in a new thread with given thread \a stackSize and
    given thread \a priority.
    \sa runAsync(QThreadPool*,QThread::Priority,Function&&,Args&&...)
    \sa QThread::Priority
    \sa QThread::setStackSize
*/
template <typename Function, typename... Args, typename ResultType = typename Internal::resultType<Function>::type>
auto runAsync(Utils::StackSizeInBytes stackSize, QThread::Priority priority, Function &&function, Args &&... args) -> QFuture<ResultType>
{
  return Internal::runAsync_internal(static_cast<QThreadPool*>(nullptr), stackSize, priority, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*!
    Runs \a function with \a args in a new thread with thread priority QThread::InheritPriority.
    \sa runAsync(QThreadPool*,QThread::Priority,Function&&,Args&&...)
    \sa QThread::Priority
 */
template <typename Function, typename... Args, typename = std::enable_if_t<!std::is_same<std::decay_t<Function>, QThreadPool>::value && !std::is_same<std::decay_t<Function>, QThread::Priority>::value>, typename ResultType = typename Internal::resultType<Function>::type>
auto runAsync(Function &&function, Args &&... args) -> QFuture<ResultType>
{
  return runAsync(static_cast<QThreadPool*>(nullptr), QThread::InheritPriority, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*!
    Runs \a function with \a args in a thread \a pool with thread priority QThread::InheritPriority.
    \sa runAsync(QThreadPool*,QThread::Priority,Function&&,Args&&...)
    \sa QThread::Priority
 */
template <typename Function, typename... Args, typename = std::enable_if_t<!std::is_same<std::decay_t<Function>, QThread::Priority>::value>, typename ResultType = typename Internal::resultType<Function>::type>
auto runAsync(QThreadPool *pool, Function &&function, Args &&... args) -> QFuture<ResultType>
{
  return runAsync(pool, QThread::InheritPriority, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*!
    Adds a handler for when a result is ready.
    This creates a new QFutureWatcher. Do not use if you intend to react on multiple conditions
    or create a QFutureWatcher already for other reasons.
*/
template <typename R, typename T>
auto onResultReady(const QFuture<T> &future, R *receiver, void (R::*member)(const T &)) -> const QFuture<T>&
{
  auto watcher = new QFutureWatcher<T>();
  QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
  QObject::connect(watcher, &QFutureWatcherBase::resultReadyAt, receiver, [receiver, member, watcher](int index) {
    (receiver->*member)(watcher->future().resultAt(index));
  });
  watcher->setFuture(future);
  return future;
}

/*!
    Adds a handler for when a result is ready. The guard object determines the lifetime of
    the connection.
    This creates a new QFutureWatcher. Do not use if you intend to react on multiple conditions
    or create a QFutureWatcher already for other reasons.
*/
template <typename T, typename Function>
auto onResultReady(const QFuture<T> &future, QObject *guard, Function f) -> const QFuture<T>&
{
  auto watcher = new QFutureWatcher<T>();
  QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
  QObject::connect(watcher, &QFutureWatcherBase::resultReadyAt, guard, [f, watcher](int index) {
    f(watcher->future().resultAt(index));
  });
  watcher->setFuture(future);
  return future;
}

/*!
    Adds a handler for when a result is ready.
    This creates a new QFutureWatcher. Do not use if you intend to react on multiple conditions
    or create a QFutureWatcher already for other reasons.
*/
template <typename T, typename Function>
auto onResultReady(const QFuture<T> &future, Function f) -> const QFuture<T>&
{
  auto watcher = new QFutureWatcher<T>();
  QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
  QObject::connect(watcher, &QFutureWatcherBase::resultReadyAt, [f, watcher](int index) {
    f(watcher->future().resultAt(index));
  });
  watcher->setFuture(future);
  return future;
}

/*!
    Adds a handler for when the future is finished.
    This creates a new QFutureWatcher. Do not use if you intend to react on multiple conditions
    or create a QFutureWatcher already for other reasons.
*/
template <typename R, typename T>
auto onFinished(const QFuture<T> &future, R *receiver, void (R::*member)(const QFuture<T> &)) -> const QFuture<T>&
{
  auto watcher = new QFutureWatcher<T>();
  QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
  QObject::connect(watcher, &QFutureWatcherBase::finished, receiver, [receiver, member, watcher]() { (receiver->*member)(watcher->future()); });
  watcher->setFuture(future);
  return future;
}

/*!
    Adds a handler for when the future is finished. The guard object determines the lifetime of
    the connection.
    This creates a new QFutureWatcher. Do not use if you intend to react on multiple conditions
    or create a QFutureWatcher already for other reasons.
*/
template <typename T, typename Function>
auto onFinished(const QFuture<T> &future, QObject *guard, Function f) -> const QFuture<T>&
{
  auto watcher = new QFutureWatcher<T>();
  QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
  QObject::connect(watcher, &QFutureWatcherBase::finished, guard, [f, watcher]() {
    f(watcher->future());
  });
  watcher->setFuture(future);
  return future;
}

/*!
    Adds a handler for when the future is finished.
    This creates a new QFutureWatcher. Do not use if you intend to react on multiple conditions
    or create a QFutureWatcher already for other reasons.
*/
template <typename T, typename Function>
auto onFinished(const QFuture<T> &future, Function f) -> const QFuture<T>&
{
  auto watcher = new QFutureWatcher<T>();
  QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
  QObject::connect(watcher, &QFutureWatcherBase::finished, [f, watcher]() {
    f(watcher->future());
  });
  watcher->setFuture(future);
  return future;
}

} // namespace Utils