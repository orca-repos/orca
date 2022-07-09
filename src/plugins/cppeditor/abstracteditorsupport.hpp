// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QString>
#include <QObject>

namespace CppEditor {
class CppModelManager;

class CPPEDITOR_EXPORT AbstractEditorSupport : public QObject {
  Q_OBJECT

public:
  explicit AbstractEditorSupport(CppModelManager *modelmanager, QObject *parent = nullptr);
  ~AbstractEditorSupport() override;

  /// \returns the contents, encoded as UTF-8
  virtual auto contents() const -> QByteArray = 0;
  virtual auto fileName() const -> QString = 0;
  virtual auto sourceFileName() const -> QString = 0;

  auto updateDocument() -> void;
  auto notifyAboutUpdatedContents() const -> void;
  auto revision() const -> unsigned { return m_revision; }

  static auto licenseTemplate(const QString &file = QString(), const QString &className = QString()) -> QString;
  static auto usePragmaOnce() -> bool;

private:
  CppModelManager *m_modelmanager;
  unsigned m_revision;
};

} // namespace CppEditor
