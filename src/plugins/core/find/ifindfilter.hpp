// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "textfindconstants.hpp"

QT_BEGIN_NAMESPACE
class QWidget;
class QSettings;
class QKeySequence;
class Pixmap;
QT_END_NAMESPACE

namespace Core {

class CORE_EXPORT IFindFilter : public QObject {
  Q_OBJECT

public:
  IFindFilter();
  ~IFindFilter() override;

  static auto allFindFilters() -> QList<IFindFilter*>;
  virtual auto id() const -> QString = 0;
  virtual auto displayName() const -> QString = 0;
  virtual auto isEnabled() const -> bool = 0;
  virtual auto isValid() const -> bool { return true; }
  virtual auto defaultShortcut() const -> QKeySequence;
  virtual auto isReplaceSupported() const -> bool { return false; }
  virtual auto showSearchTermInput() const -> bool { return true; }
  virtual auto supportedFindFlags() const -> FindFlags;
  virtual auto findAll(const QString &txt, FindFlags find_flags) -> void = 0;

  virtual auto replaceAll(const QString &txt, const FindFlags find_flags) -> void
  {
    Q_UNUSED(txt)
    Q_UNUSED(find_flags)
  }

  virtual auto createConfigWidget() -> QWidget*
  {
    return nullptr;
  }

  virtual auto writeSettings(QSettings *settings) -> void
  {
    Q_UNUSED(settings)
  }

  virtual auto readSettings(QSettings *settings) -> void
  {
    Q_UNUSED(settings)
  }

  static auto pixmapForFindFlags(FindFlags flags) -> QPixmap;
  static auto descriptionForFindFlags(FindFlags flags) -> QString;

signals:
  auto enabledChanged(bool enabled) -> void;
  auto validChanged(bool valid) -> void;
  auto displayNameChanged() -> void;
};

} // namespace Core
