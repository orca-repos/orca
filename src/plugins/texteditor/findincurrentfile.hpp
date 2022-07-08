// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "basefilefind.hpp"

#include <QPointer>

namespace Core {
class IEditor;
class IDocument;
} // namespace Core

namespace TextEditor {
namespace Internal {

class FindInCurrentFile final : public BaseFileFind {
  Q_OBJECT

public:
  FindInCurrentFile();

  auto id() const -> QString override;
  auto displayName() const -> QString override;
  auto isEnabled() const -> bool override;
  auto writeSettings(QSettings *settings) -> void override;
  auto readSettings(QSettings *settings) -> void override;

protected:
  auto files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator* override;
  auto additionalParameters() const -> QVariant override;
  auto label() const -> QString override;
  auto toolTip() const -> QString override;

private:
  auto handleFileChange(Core::IEditor *editor) -> void;

  QPointer<Core::IDocument> m_currentDocument;
};

} // namespace Internal
} // namespace TextEditor
