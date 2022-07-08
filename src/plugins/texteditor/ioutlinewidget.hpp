// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>
#include <QWidget>

namespace Core {
class IEditor;
}

namespace TextEditor {

class TEXTEDITOR_EXPORT IOutlineWidget : public QWidget {
  Q_OBJECT

public:
  IOutlineWidget(QWidget *parent = nullptr) : QWidget(parent) {}

  virtual auto filterMenuActions() const -> QList<QAction*> = 0;
  virtual auto setCursorSynchronization(bool syncWithCursor) -> void = 0;
  virtual auto setSorted(bool /*sorted*/) -> void {}
  virtual auto isSorted() const -> bool { return false; }
  virtual auto restoreSettings(const QVariantMap & /*map*/) -> void { }
  virtual auto settings() const -> QVariantMap { return QVariantMap(); }
};

class TEXTEDITOR_EXPORT IOutlineWidgetFactory : public QObject {
  Q_OBJECT

public:
  IOutlineWidgetFactory();
  ~IOutlineWidgetFactory() override;

  virtual auto supportsEditor(Core::IEditor *editor) const -> bool = 0;
  virtual auto supportsSorting() const -> bool { return false; }
  virtual auto createWidget(Core::IEditor *editor) -> IOutlineWidget* = 0;
  static auto updateOutline() -> void;
};

} // namespace TextEditor
