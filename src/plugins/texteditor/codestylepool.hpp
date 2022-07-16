// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QObject>

namespace Utils {
class FilePath;
}

namespace TextEditor {

class ICodeStylePreferences;
class ICodeStylePreferencesFactory;
class TabSettings;

namespace Internal {
class CodeStylePoolPrivate;
}

class TEXTEDITOR_EXPORT CodeStylePool : public QObject {
  Q_OBJECT

public:
  explicit CodeStylePool(ICodeStylePreferencesFactory *factory, QObject *parent = nullptr);
  ~CodeStylePool() override;

  auto codeStyles() const -> QList<ICodeStylePreferences*>;
  auto builtInCodeStyles() const -> QList<ICodeStylePreferences*>;
  auto customCodeStyles() const -> QList<ICodeStylePreferences*>;
  auto cloneCodeStyle(ICodeStylePreferences *originalCodeStyle) -> ICodeStylePreferences*;
  auto createCodeStyle(const QByteArray &id, const TabSettings &tabSettings, const QVariant &codeStyleData, const QString &displayName) -> ICodeStylePreferences*;
  // ownership is passed to the pool
  auto addCodeStyle(ICodeStylePreferences *codeStyle) -> void;
  // is removed and deleted
  auto removeCodeStyle(ICodeStylePreferences *codeStyle) -> void;
  auto codeStyle(const QByteArray &id) const -> ICodeStylePreferences*;
  auto loadCustomCodeStyles() -> void;
  auto importCodeStyle(const Utils::FilePath &fileName) -> ICodeStylePreferences*;
  auto exportCodeStyle(const Utils::FilePath &fileName, ICodeStylePreferences *codeStyle) const -> void;

signals:
  auto codeStyleAdded(ICodeStylePreferences *) -> void;
  auto codeStyleRemoved(ICodeStylePreferences *) -> void;

private:
  auto slotSaveCodeStyle() -> void;
  auto settingsDir() const -> QString;
  auto settingsPath(const QByteArray &id) const -> Utils::FilePath;
  auto loadCodeStyle(const Utils::FilePath &fileName) -> ICodeStylePreferences*;
  auto saveCodeStyle(ICodeStylePreferences *codeStyle) const -> void;

  Internal::CodeStylePoolPrivate *d;
};

} // namespace TextEditor
