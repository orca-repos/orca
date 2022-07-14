// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-generated-file.hpp"
#include "core-global.hpp"

#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QWizardPage;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class IWizardFactory;

/*!
  Hook to add generic wizard pages to implementations of IWizard.
  Used e.g. to add "Add to Project File/Add to Version Control" page
  */
class CORE_EXPORT IFileWizardExtension : public QObject {
  Q_OBJECT

public:
  IFileWizardExtension();
  ~IFileWizardExtension() override;

  /* Return a list of pages to be added to the Wizard (empty list if not
   * applicable). */
  virtual auto extensionPages(const IWizardFactory *wizard) -> QList<QWizardPage*> = 0;
  /* Process the files using the extension parameters */
  virtual auto processFiles(const QList<GeneratedFile> &files, bool *remove_open_project_attribute, QString *error_message) -> bool = 0;
  /* Applies code style settings which may depend on the project to which
   * the files will be added.
   * This function is called before the files are actually written out,
   * before processFiles() is called*/
  virtual auto applyCodeStyle(GeneratedFile *file) const -> void = 0;

public slots:
  /* Notification about the first extension page being shown. */
  virtual auto firstExtensionPageShown(const QList<GeneratedFile> &files, const QVariantMap &extraValues) -> void
  {
    Q_UNUSED(files)
    Q_UNUSED(extraValues)
  }
};

} // namespace Orca::Plugin::Core
