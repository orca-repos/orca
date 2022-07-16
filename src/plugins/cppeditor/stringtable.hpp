// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QString>

namespace CppEditor::Internal {

class StringTable {
public:
  static auto insert(const QString &string) -> QString;
  static auto scheduleGC() -> void;

private:
  friend class CppEditorPluginPrivate;
  StringTable();
  ~StringTable();
};

} // CppEditor::Internal
