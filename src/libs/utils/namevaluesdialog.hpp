// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environmentfwd.hpp"
#include "namevalueitem.hpp"
#include "optional.hpp"
#include "utils_global.hpp"

#include <QDialog>

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Utils {
namespace Internal {

class NameValueItemsWidget : public QWidget {
  Q_OBJECT

public:
  explicit NameValueItemsWidget(QWidget *parent = nullptr);

  auto setEnvironmentItems(const EnvironmentItems &items) -> void;
  auto environmentItems() const -> EnvironmentItems;
  auto setPlaceholderText(const QString &text) -> void;

private:
  QPlainTextEdit *m_editor;
};
} // namespace Internal

class ORCA_UTILS_EXPORT NameValuesDialog : public QDialog {
  Q_OBJECT

public:
  using Polisher = std::function<void(QWidget *)>;

  auto setNameValueItems(const NameValueItems &items) -> void;
  auto nameValueItems() const -> NameValueItems;
  auto setPlaceholderText(const QString &text) -> void;
  static auto getNameValueItems(QWidget *parent = nullptr, const NameValueItems &initial = {}, const QString &placeholderText = {}, Polisher polish = {}, const QString &windowTitle = {}, const QString &helpText = {}) -> Utils::optional<NameValueItems>;

protected:
  explicit NameValuesDialog(const QString &windowTitle, const QString &helpText, QWidget *parent = {});

private:
  Internal::NameValueItemsWidget *m_editor;
};

} // namespace Utils
