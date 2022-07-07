// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../utils_global.hpp"

#include <QLabel>
#include <QPixmap>
#include <QSharedPointer>
#include <QVariant>
#include <QVBoxLayout>

namespace Utils {
namespace Internal {

class TipLabel : public QLabel {
public:
  TipLabel(QWidget *parent);

  virtual auto setContent(const QVariant &content) -> void = 0;
  virtual auto isInteractive() const -> bool { return false; }
  virtual auto showTime() const -> int = 0;
  virtual auto configure(const QPoint &pos) -> void = 0;
  virtual auto canHandleContentReplacement(int typeId) const -> bool = 0;
  virtual auto equals(int typeId, const QVariant &other, const QVariant &contextHelp) const -> bool = 0;
  virtual auto setContextHelp(const QVariant &help) -> void;
  virtual auto contextHelp() const -> QVariant;

protected:
  auto metaObject() const -> const QMetaObject* override;

private:
  QVariant m_contextHelp;
};

using TextItem = std::pair<QString, Qt::TextFormat>;

class TextTip : public TipLabel {
  Q_OBJECT

public:
  TextTip(QWidget *parent);

  auto setContent(const QVariant &content) -> void override;
  auto isInteractive() const -> bool override;
  auto configure(const QPoint &pos) -> void override;
  auto canHandleContentReplacement(int typeId) const -> bool override;
  auto showTime() const -> int override;
  auto equals(int typeId, const QVariant &other, const QVariant &otherContextHelp) const -> bool override;
  auto paintEvent(QPaintEvent *event) -> void override;
  auto resizeEvent(QResizeEvent *event) -> void override;

private:
  QString m_text;
  Qt::TextFormat m_format = Qt::AutoText;
};

class ColorTip : public TipLabel {
  Q_OBJECT

public:
  ColorTip(QWidget *parent);

  auto setContent(const QVariant &content) -> void override;
  auto configure(const QPoint &pos) -> void override;
  auto canHandleContentReplacement(int typeId) const -> bool override;
  auto showTime() const -> int override { return 4000; }
  auto equals(int typeId, const QVariant &other, const QVariant &otherContextHelp) const -> bool override;
  auto paintEvent(QPaintEvent *event) -> void override;

private:
  QColor m_color;
  QPixmap m_tilePixmap;
};

class WidgetTip : public TipLabel {
  Q_OBJECT

public:
  explicit WidgetTip(QWidget *parent = nullptr);
  auto pinToolTipWidget(QWidget *parent) -> void;

  auto setContent(const QVariant &content) -> void override;
  auto configure(const QPoint &pos) -> void override;
  auto canHandleContentReplacement(int typeId) const -> bool override;
  auto showTime() const -> int override { return 30000; }
  auto equals(int typeId, const QVariant &other, const QVariant &otherContextHelp) const -> bool override;
  auto isInteractive() const -> bool override { return true; }

private:
  QWidget *m_widget;
  QVBoxLayout *m_layout;
};

} // namespace Internal
} // namespace Utils

Q_DECLARE_METATYPE(Utils::Internal::TextItem)
