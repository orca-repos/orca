// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QObject>

QT_BEGIN_NAMESPACE
class QVariant;
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

namespace Internal {
class ICodeStylePreferencesPrivate;
}

class TabSettings;
class CodeStylePool;

class TEXTEDITOR_EXPORT ICodeStylePreferences : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
  // pool is a pool which will be used by this preferences for setting delegates
  explicit ICodeStylePreferences(QObject *parentObject = nullptr);
  ~ICodeStylePreferences() override;

  auto id() const -> QByteArray;
  auto setId(const QByteArray &name) -> void;
  auto displayName() const -> QString;
  auto setDisplayName(const QString &name) -> void;
  auto isReadOnly() const -> bool;
  auto setReadOnly(bool on) -> void;
  auto setTabSettings(const TabSettings &settings) -> void;
  auto tabSettings() const -> TabSettings;
  auto currentTabSettings() const -> TabSettings;

  virtual auto value() const -> QVariant = 0;
  virtual auto setValue(const QVariant &) -> void = 0;

  auto currentValue() const -> QVariant;                     // may be from grandparent
  auto currentPreferences() const -> ICodeStylePreferences*; // may be grandparent
  auto delegatingPool() const -> CodeStylePool*;
  auto setDelegatingPool(CodeStylePool *pool) -> void;
  auto currentDelegate() const -> ICodeStylePreferences*; // null or one of delegates from the pool
  auto setCurrentDelegate(ICodeStylePreferences *delegate) -> void;
  auto currentDelegateId() const -> QByteArray;
  auto setCurrentDelegate(const QByteArray &id) -> void;
  auto setSettingsSuffix(const QString &suffix) -> void;
  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;

  // make below 2 protected?
  virtual auto toMap() const -> QVariantMap;
  virtual auto fromMap(const QVariantMap &map) -> void;

signals:
  auto tabSettingsChanged(const TabSettings &settings) -> void;
  auto currentTabSettingsChanged(const TabSettings &settings) -> void;
  auto valueChanged(const QVariant &) -> void;
  auto currentValueChanged(const QVariant &) -> void;
  auto currentDelegateChanged(ICodeStylePreferences *currentDelegate) -> void;
  auto currentPreferencesChanged(ICodeStylePreferences *currentPreferences) -> void;
  auto displayNameChanged(const QString &newName) -> void;

private:
  auto codeStyleRemoved(ICodeStylePreferences *preferences) -> void;

  Internal::ICodeStylePreferencesPrivate *d;
};


} // namespace TextEditor
