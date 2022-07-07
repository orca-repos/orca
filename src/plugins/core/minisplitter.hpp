// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <QSplitter>

QT_BEGIN_NAMESPACE
class QSplitterHandle;
QT_END_NAMESPACE

namespace Core {

/*! This is a simple helper-class to obtain mac-style 1-pixel wide splitters */
class CORE_EXPORT MiniSplitter : public QSplitter {
public:
  enum SplitterStyle {
    Dark,
    Light
  };

  explicit MiniSplitter(QWidget *parent = nullptr, SplitterStyle style = Dark);
  explicit MiniSplitter(Qt::Orientation orientation, QWidget *parent = nullptr, SplitterStyle style = Dark);

protected:
  auto createHandle() -> QSplitterHandle* override;

private:
  SplitterStyle m_style;
};

class CORE_EXPORT NonResizingSplitter : public MiniSplitter {
public:
  explicit NonResizingSplitter(QWidget *parent, SplitterStyle style = Light);

protected:
  auto resizeEvent(QResizeEvent *ev) -> void override;
};


} // namespace Core
