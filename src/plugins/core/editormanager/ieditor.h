// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>
#include <core/icontext.h>

#include <QMetaType>

namespace Core {

class IDocument;

class CORE_EXPORT IEditor : public IContext {
  Q_OBJECT

public:
  IEditor();

  auto duplicateSupported() const -> bool;
  auto setDuplicateSupported(bool duplicate_supported) -> void;

  virtual auto document() const -> IDocument* = 0;
  virtual auto duplicate() -> IEditor* { return nullptr; }
  virtual auto saveState() const -> QByteArray { return {}; }
  virtual auto restoreState(const QByteArray & /*state*/) -> void {}
  virtual auto currentLine() const -> int { return 0; }
  virtual auto currentColumn() const -> int { return 0; }

  virtual auto gotoLine(const int line, const int column = 0, const bool center_line = true) -> void
  {
    Q_UNUSED(line)
    Q_UNUSED(column)
    Q_UNUSED(center_line)
  }

  virtual auto toolBar() -> QWidget* = 0;
  virtual auto isDesignModePreferred() const -> bool { return false; }

signals:
  auto editorDuplicated(IEditor *duplicate) -> void;

private:
  bool m_duplicate_supported;
};

} // namespace Core

Q_DECLARE_METATYPE(Core::IEditor*)
