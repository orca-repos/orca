// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QString>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

class SnippetsSettings {
public:
  SnippetsSettings() = default;

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;
  auto setLastUsedSnippetGroup(const QString &lastUsed) -> void;
  auto lastUsedSnippetGroup() const -> const QString&;
  auto equals(const SnippetsSettings &snippetsSettings) const -> bool;

  friend auto operator==(const SnippetsSettings &a, const SnippetsSettings &b) -> bool { return a.equals(b); }
  friend auto operator!=(const SnippetsSettings &a, const SnippetsSettings &b) -> bool { return !a.equals(b); }

private:
  QString m_lastUsedSnippetGroup;
};

} // TextEditor
