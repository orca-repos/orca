// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/changeset.hpp>

#include <QFutureWatcher>
#include <QString>

QT_BEGIN_NAMESPACE
class QChar;
class QTextCursor;
QT_END_NAMESPACE

namespace TextEditor {

class TabSettings;

class Formatter {
public:
  Formatter() = default;
  virtual ~Formatter() = default;

  virtual auto format(const QTextCursor & /*cursor*/, const TabSettings & /*tabSettings*/) -> QFutureWatcher<Utils::ChangeSet>*
  {
    return nullptr;
  }

  virtual auto isElectricCharacter(const QChar & /*ch*/) const -> bool { return false; }
  virtual auto supportsAutoFormat() const -> bool { return false; }

  virtual auto autoFormat(const QTextCursor & /*cursor*/, const TabSettings & /*tabSettings*/) -> QFutureWatcher<Utils::ChangeSet>*
  {
    return nullptr;
  }

  virtual auto supportsFormatOnSave() const -> bool { return false; }

  virtual auto formatOnSave(const QTextCursor & /*cursor*/, const TabSettings & /*tabSettings*/) -> QFutureWatcher<Utils::ChangeSet>*
  {
    return nullptr;
  }
};

} // namespace TextEditor
