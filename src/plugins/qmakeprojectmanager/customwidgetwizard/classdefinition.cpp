// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classdefinition.hpp"

#include <QFileInfo>

namespace QmakeProjectManager {
namespace Internal {

ClassDefinition::ClassDefinition(QWidget *parent) : QTabWidget(parent), m_domXmlChanged(false)
{
  m_ui.setupUi(this);
  m_ui.iconPathChooser->setExpectedKind(Utils::PathChooser::File);
  m_ui.iconPathChooser->setHistoryCompleter(QLatin1String("Qmake.Icon.History"));
  m_ui.iconPathChooser->setPromptDialogTitle(tr("Select Icon"));
  m_ui.iconPathChooser->setPromptDialogFilter(tr("Icon files (*.png *.ico *.jpg *.xpm *.tif *.svg)"));

  connect(m_ui.libraryRadio, &QRadioButton::toggled, this, &ClassDefinition::enableButtons);
  connect(m_ui.skeletonCheck, &QCheckBox::toggled, this, &ClassDefinition::enableButtons);
  connect(m_ui.widgetLibraryEdit, &QLineEdit::textChanged, this, &ClassDefinition::widgetLibraryChanged);
  connect(m_ui.widgetHeaderEdit, &QLineEdit::textChanged, this, &ClassDefinition::widgetHeaderChanged);
  connect(m_ui.pluginClassEdit, &QLineEdit::textChanged, this, &ClassDefinition::pluginClassChanged);
  connect(m_ui.pluginHeaderEdit, &QLineEdit::textChanged, this, &ClassDefinition::pluginHeaderChanged);
  connect(m_ui.domXmlEdit, &QTextEdit::textChanged, this, [this] { m_domXmlChanged = true; });
}

auto ClassDefinition::enableButtons() -> void
{
  const bool enLib = m_ui.libraryRadio->isChecked();
  m_ui.widgetLibraryLabel->setEnabled(enLib);
  m_ui.widgetLibraryEdit->setEnabled(enLib);

  const bool enSrc = m_ui.skeletonCheck->isChecked();
  m_ui.widgetSourceLabel->setEnabled(enSrc);
  m_ui.widgetSourceEdit->setEnabled(enSrc);
  m_ui.widgetBaseClassLabel->setEnabled(enSrc);
  m_ui.widgetBaseClassEdit->setEnabled(enSrc);

  const bool enPrj = !enLib || enSrc;
  m_ui.widgetProjectLabel->setEnabled(enPrj);
  m_ui.widgetProjectEdit->setEnabled(enPrj);
  m_ui.widgetProjectEdit->setText(QFileInfo(m_ui.widgetProjectEdit->text()).completeBaseName() + (m_ui.libraryRadio->isChecked() ? QLatin1String(".pro") : QLatin1String(".pri")));
}

static inline auto xmlFromClassName(const QString &name) -> QString
{
  QString rc = QLatin1String("<widget class=\"");
  rc += name;
  rc += QLatin1String("\" name=\"");
  if (!name.isEmpty()) {
    rc += name.left(1).toLower();
    if (name.size() > 1)
      rc += name.mid(1);
  }
  rc += QLatin1String("\">\n</widget>\n");
  return rc;
}

auto ClassDefinition::setClassName(const QString &name) -> void
{
  m_ui.widgetLibraryEdit->setText(name.toLower());
  m_ui.widgetHeaderEdit->setText(m_fileNamingParameters.headerFileName(name));
  m_ui.pluginClassEdit->setText(name + QLatin1String("Plugin"));
  if (!m_domXmlChanged) {
    m_ui.domXmlEdit->setText(xmlFromClassName(name));
    m_domXmlChanged = false;
  }
}

auto ClassDefinition::widgetLibraryChanged(const QString &text) -> void
{
  m_ui.widgetProjectEdit->setText(text + (m_ui.libraryRadio->isChecked() ? QLatin1String(".pro") : QLatin1String(".pri")));
}

auto ClassDefinition::widgetHeaderChanged(const QString &text) -> void
{
  m_ui.widgetSourceEdit->setText(m_fileNamingParameters.headerToSourceFileName(text));
}

auto ClassDefinition::pluginClassChanged(const QString &text) -> void
{
  m_ui.pluginHeaderEdit->setText(m_fileNamingParameters.headerFileName(text));
}

auto ClassDefinition::pluginHeaderChanged(const QString &text) -> void
{
  m_ui.pluginSourceEdit->setText(m_fileNamingParameters.headerToSourceFileName(text));
}

auto ClassDefinition::widgetOptions(const QString &className) const -> PluginOptions::WidgetOptions
{
  PluginOptions::WidgetOptions wo;
  wo.createSkeleton = m_ui.skeletonCheck->isChecked();
  wo.sourceType = m_ui.libraryRadio->isChecked() ? PluginOptions::WidgetOptions::LinkLibrary : PluginOptions::WidgetOptions::IncludeProject;
  wo.widgetLibrary = m_ui.widgetLibraryEdit->text();
  wo.widgetProjectFile = m_ui.widgetProjectEdit->text();
  wo.widgetClassName = className;
  wo.widgetHeaderFile = m_ui.widgetHeaderEdit->text();
  wo.widgetSourceFile = m_ui.widgetSourceEdit->text();
  wo.widgetBaseClassName = m_ui.widgetBaseClassEdit->text();
  wo.pluginClassName = m_ui.pluginClassEdit->text();
  wo.pluginHeaderFile = m_ui.pluginHeaderEdit->text();
  wo.pluginSourceFile = m_ui.pluginSourceEdit->text();
  wo.iconFile = m_ui.iconPathChooser->filePath().toString();
  wo.group = m_ui.groupEdit->text();
  wo.toolTip = m_ui.tooltipEdit->text();
  wo.whatsThis = m_ui.whatsthisEdit->toPlainText();
  wo.isContainer = m_ui.containerCheck->isChecked();
  wo.domXml = m_ui.domXmlEdit->toPlainText();
  return wo;
}

}
}
