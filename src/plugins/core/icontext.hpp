// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"
#include "helpitem.hpp"

#include <utils/id.hpp>

#include <QList>
#include <QObject>
#include <QPointer>
#include <QWidget>

#include <functional>

namespace Core {

class CORE_EXPORT Context {
public:
  Context() = default;

  explicit Context(const Utils::Id c1)
  {
    add(c1);
  }

  Context(const Utils::Id c1, const Utils::Id c2)
  {
    add(c1);
    add(c2);
  }

  Context(const Utils::Id c1, const Utils::Id c2, const Utils::Id c3)
  {
    add(c1);
    add(c2);
    add(c3);
  }

  auto contains(const Utils::Id c) const -> bool { return d.contains(c); }
  auto size() const -> int { return static_cast<int>(d.size()); }
  auto isEmpty() const -> bool { return d.isEmpty(); }
  auto at(const int i) const -> Utils::Id { return d.at(i); }

  // FIXME: Make interface slimmer.
  using const_iterator = QList<Utils::Id>::const_iterator;
  auto begin() const -> const_iterator { return d.begin(); }
  auto end() const -> const_iterator { return d.end(); }
  auto indexOf(const Utils::Id c) const -> int { return static_cast<int>(d.indexOf(c)); }
  auto removeAt(const int i) -> void { d.removeAt(i); }
  auto prepend(const Utils::Id c) -> void { d.prepend(c); }
  auto add(const Context &c) -> void { d += c.d; }
  auto add(const Utils::Id c) -> void { d.append(c); }
  auto operator==(const Context &c) const -> bool { return d == c.d; }

private:
  QList<Utils::Id> d;
};

class CORE_EXPORT IContext : public QObject {
  Q_OBJECT

public:
  explicit IContext(QObject *parent = nullptr) : QObject(parent) {}

  using HelpCallback = std::function<void(const HelpItem &item)>;

  virtual auto context() const -> Context { return m_context; }
  virtual auto widget() const -> QWidget* { return m_widget; }
  virtual auto contextHelp(const HelpCallback &callback) const -> void { callback(m_context_help); }
  virtual auto setContext(const Context &context) -> void { m_context = context; }
  virtual auto setWidget(QWidget *widget) -> void { m_widget = widget; }
  virtual auto setContextHelp(const HelpItem &id) -> void { m_context_help = id; }

  friend CORE_EXPORT auto operator<<(QDebug debug, const Core::Context &context) -> QDebug;

protected:
  Context m_context;
  QPointer<QWidget> m_widget;
  HelpItem m_context_help;
};

} // namespace Core
