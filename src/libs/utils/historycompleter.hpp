// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QCompleter>

namespace Utils {

class QtcSettings;

namespace Internal { class HistoryCompleterPrivate; }

class ORCA_UTILS_EXPORT HistoryCompleter : public QCompleter {
  Q_OBJECT

public:
  static auto setSettings(QtcSettings *settings) -> void;
  HistoryCompleter(const QString &historyKey, QObject *parent = nullptr);
  auto removeHistoryItem(int index) -> bool;
  auto historyItem() const -> QString;
  auto hasHistory() const -> bool { return historySize() > 0; }
  static auto historyExistsFor(const QString &historyKey) -> bool;

private:
  ~HistoryCompleter() override;
  auto historySize() const -> int;
  auto maximalHistorySize() const -> int;
  auto setMaximalHistorySize(int numberOfEntries) -> void;

public Q_SLOTS:
  auto clearHistory() -> void;
  auto addEntry(const QString &str) -> void;

private:
  Internal::HistoryCompleterPrivate *d;
};

} // namespace Utils
