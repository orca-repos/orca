// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QWizard>

namespace Utils {

class Wizard;
class WizardProgress;
class WizardPrivate;

const char SHORT_TITLE_PROPERTY[] = "shortTitle";

class ORCA_UTILS_EXPORT Wizard : public QWizard {
  Q_OBJECT
  Q_PROPERTY(bool automaticProgressCreationEnabled READ isAutomaticProgressCreationEnabled WRITE setAutomaticProgressCreationEnabled)

public:
  explicit Wizard(QWidget *parent = nullptr, Qt::WindowFlags flags = {});
  ~Wizard() override;

  auto isAutomaticProgressCreationEnabled() const -> bool;
  auto setAutomaticProgressCreationEnabled(bool enabled) -> void;
  auto setStartId(int pageId) -> void;
  auto wizardProgress() const -> WizardProgress*;

  template <class T>
  auto find() const -> T*
  {
    const QList<int> pages = pageIds();
    for (int id : pages) {
      if (T *result = qobject_cast<T*>(page(id)))
        return result;
    }
    return 0;
  }

  // will return true for all fields registered via Utils::WizardPage::registerFieldWithName(...)
  auto hasField(const QString &name) const -> bool;
  auto registerFieldName(const QString &name) -> void;
  auto fieldNames() const -> QSet<QString>;
  virtual auto variables() const -> QHash<QString, QVariant>;
  auto showVariables() -> void;

protected:
  virtual auto stringify(const QVariant &v) const -> QString;
  virtual auto evaluate(const QVariant &v) const -> QString;
  auto event(QEvent *event) -> bool override;

private:
  auto _q_currentPageChanged(int pageId) -> void;
  auto _q_pageAdded(int pageId) -> void;
  auto _q_pageRemoved(int pageId) -> void;

  Q_DECLARE_PRIVATE(Wizard)
  WizardPrivate *d_ptr;
};

class WizardProgressItem;
class WizardProgressPrivate;

class ORCA_UTILS_EXPORT WizardProgress : public QObject {
  Q_OBJECT

public:
  WizardProgress(QObject *parent = nullptr);
  ~WizardProgress() override;

  auto addItem(const QString &title) -> WizardProgressItem*;
  auto removeItem(WizardProgressItem *item) -> void;
  auto removePage(int pageId) -> void;
  static auto pages(WizardProgressItem *item) -> QList<int>;
  auto item(int pageId) const -> WizardProgressItem*;
  auto currentItem() const -> WizardProgressItem*;
  auto items() const -> QList<WizardProgressItem*>;
  auto startItem() const -> WizardProgressItem*;
  auto visitedItems() const -> QList<WizardProgressItem*>;
  auto directlyReachableItems() const -> QList<WizardProgressItem*>;
  auto isFinalItemDirectlyReachable() const -> bool; // return  availableItems().last()->isFinalItem();

Q_SIGNALS:
  auto currentItemChanged(WizardProgressItem *item) -> void;

  auto itemChanged(WizardProgressItem *item) -> void; // contents of the item: title or icon
  auto itemAdded(WizardProgressItem *item) -> void;
  auto itemRemoved(WizardProgressItem *item) -> void;
  auto nextItemsChanged(WizardProgressItem *item, const QList<WizardProgressItem*> &items) -> void;
  auto nextShownItemChanged(WizardProgressItem *item, WizardProgressItem *nextShownItem) -> void;
  auto startItemChanged(WizardProgressItem *item) -> void;

private:
  auto setCurrentPage(int pageId) -> void;
  auto setStartPage(int pageId) -> void;

  friend class Wizard;
  friend class WizardProgressItem;
  friend ORCA_UTILS_EXPORT auto operator<<(QDebug &debug, const WizardProgress &progress) -> QDebug&;
  Q_DECLARE_PRIVATE(WizardProgress)

  WizardProgressPrivate *d_ptr;
};

class WizardProgressItemPrivate;

class ORCA_UTILS_EXPORT WizardProgressItem // managed by WizardProgress
{

public:
  auto addPage(int pageId) -> void;
  auto pages() const -> QList<int>;
  auto setNextItems(const QList<WizardProgressItem*> &items) -> void;
  auto nextItems() const -> QList<WizardProgressItem*>;
  auto setNextShownItem(WizardProgressItem *item) -> void;
  auto nextShownItem() const -> WizardProgressItem*;
  auto isFinalItem() const -> bool; // return nextItems().isEmpty();
  auto setTitle(const QString &title) -> void;
  auto title() const -> QString;
  auto setTitleWordWrap(bool wrap) -> void;
  auto titleWordWrap() const -> bool;

protected:
  WizardProgressItem(WizardProgress *progress, const QString &title);
  virtual ~WizardProgressItem();

private:
  friend class WizardProgress;
  friend ORCA_UTILS_EXPORT auto operator<<(QDebug &d, const WizardProgressItem &item) -> QDebug&;

  Q_DECLARE_PRIVATE(WizardProgressItem)

  WizardProgressItemPrivate *d_ptr;
};

ORCA_UTILS_EXPORT auto operator<<(QDebug &debug, const WizardProgress &progress) -> QDebug&;

ORCA_UTILS_EXPORT auto operator<<(QDebug &debug, const WizardProgressItem &item) -> QDebug&;

} // namespace Utils
