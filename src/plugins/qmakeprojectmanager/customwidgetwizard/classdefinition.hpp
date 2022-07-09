// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_classdefinition.h"
#include "filenamingparameters.hpp"
#include "pluginoptions.hpp"

#include <QTabWidget>

namespace QmakeProjectManager {
namespace Internal {

class ClassDefinition : public QTabWidget {
  Q_OBJECT

public:
  explicit ClassDefinition(QWidget *parent = nullptr);

  auto setClassName(const QString &name) -> void;
  auto fileNamingParameters() const -> FileNamingParameters { return m_fileNamingParameters; }
  auto setFileNamingParameters(const FileNamingParameters &fnp) -> void { m_fileNamingParameters = fnp; }
  auto widgetOptions(const QString &className) const -> PluginOptions::WidgetOptions;
  auto enableButtons() -> void;

private Q_SLOTS:
  auto widgetLibraryChanged(const QString &text) -> void;
  auto widgetHeaderChanged(const QString &text) -> void;
  auto pluginClassChanged(const QString &text) -> void;
  auto pluginHeaderChanged(const QString &text) -> void;

private:
  Ui::ClassDefinition m_ui;
  FileNamingParameters m_fileNamingParameters;
  bool m_domXmlChanged;
};

}
}
