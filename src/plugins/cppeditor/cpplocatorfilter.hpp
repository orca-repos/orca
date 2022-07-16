// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cpplocatordata.hpp"
#include "searchsymbols.hpp"

#include <core/core-locator-filter-interface.hpp>

namespace CppEditor {

class CPPEDITOR_EXPORT CppLocatorFilter : public Orca::Plugin::Core::ILocatorFilter {
  Q_OBJECT

public:
  explicit CppLocatorFilter(CppLocatorData *locatorData);
  ~CppLocatorFilter() override;

  auto matchesFor(QFutureInterface<Orca::Plugin::Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Orca::Plugin::Core::LocatorFilterEntry> override;
  auto accept(const Orca::Plugin::Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void override;

protected:
  virtual auto matchTypes() const -> IndexItem::ItemType { return IndexItem::All; }
  virtual auto filterEntryFromIndexItem(IndexItem::Ptr info) -> Orca::Plugin::Core::LocatorFilterEntry;
  
  CppLocatorData *m_data = nullptr;
};

class CPPEDITOR_EXPORT CppClassesFilter : public CppLocatorFilter {
  Q_OBJECT

public:
  explicit CppClassesFilter(CppLocatorData *locatorData);
  ~CppClassesFilter() override;

protected:
  auto matchTypes() const -> IndexItem::ItemType override { return IndexItem::Class; }
  auto filterEntryFromIndexItem(IndexItem::Ptr info) -> Orca::Plugin::Core::LocatorFilterEntry override;
};

class CPPEDITOR_EXPORT CppFunctionsFilter : public CppLocatorFilter {
  Q_OBJECT

public:
  explicit CppFunctionsFilter(CppLocatorData *locatorData);
  ~CppFunctionsFilter() override;

protected:
  auto matchTypes() const -> IndexItem::ItemType override { return IndexItem::Function; }
  auto filterEntryFromIndexItem(IndexItem::Ptr info) -> Orca::Plugin::Core::LocatorFilterEntry override;
};

} // namespace CppEditor
