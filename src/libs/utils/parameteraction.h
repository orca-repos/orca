// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QAction>

namespace Utils {

class ORCA_UTILS_EXPORT ParameterAction : public QAction {
  Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText)
  Q_PROPERTY(QString parameterText READ parameterText WRITE setParameterText)
  Q_PROPERTY(EnablingMode enablingMode READ enablingMode WRITE setEnablingMode)
  Q_OBJECT

public:
  enum EnablingMode {
    AlwaysEnabled,
    EnabledWithParameter
  };

  Q_ENUM(EnablingMode)

  explicit ParameterAction(const QString &emptyText, const QString &parameterText, EnablingMode em = AlwaysEnabled, QObject *parent = nullptr);

  auto emptyText() const -> QString;
  auto setEmptyText(const QString &) -> void;
  auto parameterText() const -> QString;
  auto setParameterText(const QString &) -> void;
  auto enablingMode() const -> EnablingMode;
  auto setEnablingMode(EnablingMode m) -> void;

public slots:
  auto setParameter(const QString &) -> void;

private:
  QString m_emptyText;
  QString m_parameterText;
  EnablingMode m_enablingMode;
};

} // namespace Utils
