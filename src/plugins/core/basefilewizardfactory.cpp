// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "basefilewizardfactory.hpp"
#include "basefilewizard.hpp"
#include "icontext.hpp"
#include "editormanager/editormanager.hpp"
#include "dialogs/promptoverwritedialog.hpp"

#include <utils/filewizardpage.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/wizard.hpp>

#include <QDir>
#include <QFileInfo>
#include <QDebug>

enum {
  debugWizard = 0
};

using namespace Utils;

namespace Core {

static auto indexOfFile(const GeneratedFiles &f, const QString &path) -> int
{
  const auto size = f.size();
  for (auto i = 0; i < size; ++i)
    if (f.at(i).path() == path)
      return i;
  return -1;
}

/*!
    \class Core::BaseFileWizard
    \inheaderfile coreplugin/basefilewizardfactory.h
    \inmodule Orca

    \brief The BaseFileWizard class implements a is a convenience class for
    creating files.

    \sa Core::BaseFileWizardFactory
*/

auto BaseFileWizardFactory::runWizardImpl(const FilePath &path, QWidget *parent, const Id platform, const QVariantMap &extra_values, const bool show_wizard) -> Wizard*
{
  Q_UNUSED(show_wizard);
  QTC_ASSERT(!path.isEmpty(), return nullptr);

  // Create dialog and run it. Ensure that the dialog is deleted when
  // leaving the func, but not before the IFileWizardExtension::process
  // has been called

  WizardDialogParameters::DialogParameterFlags dialog_parameter_flags;

  if (flags().testFlag(ForceCapitalLetterForFileName))
    dialog_parameter_flags |= WizardDialogParameters::ForceCapitalLetterForFileName;

  Wizard *wizard = create(parent, WizardDialogParameters(path, platform, requiredFeatures(), dialog_parameter_flags, extra_values));
  QTC_CHECK(wizard);
  return wizard;
}

/*!
    \class Core::BaseFileWizardFactory
    \inheaderfile coreplugin/basefilewizardfactory.h
    \inmodule Orca

    \brief The BaseFileWizardFactory class implements a generic wizard for
    creating files.

    The following abstract functions must be implemented:
    \list
    \li create(): Called to create the QWizard dialog to be shown.
    \li generateFiles(): Generates file content.
    \endlist

    The behavior can be further customized by overwriting the virtual function
    postGenerateFiles(), which is called after generating the files.

    \note Instead of using this class, we recommend that you create JSON-based
    wizards, as instructed in \l{https://doc.qt.io/orca/creator-project-wizards.html}
    {Adding New Custom Wizards}.

    \sa Core::GeneratedFile, Core::WizardDialogParameters, Core::BaseFileWizard
*/

/*!
    \fn Core::BaseFileWizard *Core::BaseFileWizardFactory::create(QWidget *parent,
                                                                  const Core::WizardDialogParameters &parameters) const

    Creates the wizard on the \a parent with the \a parameters.
*/

/*!
    \fn virtual Core::GeneratedFiles Core::BaseFileWizardFactory::generateFiles(const QWizard *w,
                                                                                QString *errorMessage) const
    Overwrite to query the parameters from the wizard \a w and generate the
    files.

    Possible errors are held in \a errorMessage.

    \note This does not generate physical files, but merely the list of
    Core::GeneratedFile.
*/

/*!
    Physically writes \a files.

    If the files cannot be written, returns \c false and sets \a errorMessage
    to the message that is displayed to users.

    Re-implement (calling the base implementation) to create files with
    GeneratedFile::CustomGeneratorAttribute set.
*/

auto BaseFileWizardFactory::writeFiles(const GeneratedFiles &files, QString *error_message) const -> bool
{
  constexpr auto no_write_attributes = GeneratedFile::CustomGeneratorAttribute | GeneratedFile::KeepExistingFileAttribute;

  return std::ranges::all_of(files, [this, error_message](const GeneratedFile& file){
    if (!(file.attributes() & no_write_attributes) && !file.write(error_message))
      return false;
    return true;
  });
}

/*!
    Overwrite to perform steps to be done by the wizard \a w after the files
    specified by \a l are actually created.

    The default implementation opens editors with the newly generated files
    that have GeneratedFile::OpenEditorAttribute set.

    Returns \a errorMessage if errors occur.
*/

auto BaseFileWizardFactory::postGenerateFiles(const QWizard *, const GeneratedFiles &l, QString *error_message) const -> bool
{
  return postGenerateOpenEditors(l, error_message);
}

/*!
    Opens the editors for the files \a l if their
    GeneratedFile::OpenEditorAttribute attribute
    is set accordingly.

    If the editorrs cannot be opened, returns \c false and dand sets
    \a errorMessage to the message that is displayed to users.
*/

auto BaseFileWizardFactory::postGenerateOpenEditors(const GeneratedFiles &l, QString *error_message) -> bool
{
  for(const auto &file: l) {
    if (file.attributes() & GeneratedFile::OpenEditorAttribute) {
      if (!EditorManager::openEditor(FilePath::fromString(file.path()), file.editorId())) {
        if (error_message)
          *error_message = tr("Failed to open an editor for \"%1\".").arg(QDir::toNativeSeparators(file.path()));
        return false;
      }
    }
  }
  return true;
}

/*!
    Performs an overwrite check on a set of \a files. Checks if the file exists and
    can be overwritten at all, and then prompts the user with a summary.

    Returns \a errorMessage if the file cannot be overwritten.
*/

auto BaseFileWizardFactory::promptOverwrite(GeneratedFiles *files, QString *error_message) -> OverwriteResult
{
  if constexpr (debugWizard)
    qDebug() << Q_FUNC_INFO << files;

  QStringList existing_files;
  auto odd_stuff_found = false;

  static const auto read_only_msg = tr("[read only]");
  static const auto directory_msg = tr("[folder]");
  static const auto sym_link_msg = tr("[symbolic link]");

  for(const auto &file: *files) {
    if (const auto path = file.path(); QFileInfo::exists(path))
      existing_files.append(path);
  }

  if (existing_files.isEmpty())
    return OverwriteOk;

  // Before prompting to overwrite existing files, loop over files and check
  // if there is anything blocking overwriting them (like them being links or folders).
  // Format a file list message as ( "<file1> [readonly], <file2> [folder]").
  const auto common_existing_path = commonPath(existing_files);
  QString file_names_msg_part;

  for(const auto &file_name: existing_files) {
    if (const QFileInfo fi(file_name); fi.exists()) {
      if (!file_names_msg_part.isEmpty())
        file_names_msg_part += QLatin1String(", ");
      file_names_msg_part += QDir::toNativeSeparators(file_name.mid(common_existing_path.size() + 1));
      do {
        if (fi.isDir()) {
          odd_stuff_found = true;
          file_names_msg_part += QLatin1Char(' ') + directory_msg;
          break;
        }
        if (fi.isSymLink()) {
          odd_stuff_found = true;
          file_names_msg_part += QLatin1Char(' ') + sym_link_msg;
          break;
        }
        if (!fi.isWritable()) {
          odd_stuff_found = true;
          file_names_msg_part += QLatin1Char(' ') + read_only_msg;
        }
      } while (false);
    }
  }

  if (odd_stuff_found) {
    *error_message = tr("The project directory %1 contains files which cannot be overwritten:\n%2.").arg(QDir::toNativeSeparators(common_existing_path), file_names_msg_part);
    return OverwriteError;
  }

  // Prompt to overwrite existing files.
  PromptOverwriteDialog overwrite_dialog;
  // Scripts cannot handle overwrite
  overwrite_dialog.setFiles(existing_files);

  for(const auto &file: *files) if (file.attributes() & GeneratedFile::CustomGeneratorAttribute)
    overwrite_dialog.setFileEnabled(file.path(), false);

  if (overwrite_dialog.exec() != QDialog::Accepted)
    return OverwriteCanceled;

  const auto existing_files_to_keep = overwrite_dialog.uncheckedFiles();

  if (existing_files_to_keep.size() == files->size()) // All exist & all unchecked->Cancel.
    return OverwriteCanceled;

  // Set 'keep' attribute in files
  for(const auto &keep_file: existing_files_to_keep) {
    const auto i = indexOfFile(*files, keep_file);
    QTC_ASSERT(i != -1, return OverwriteCanceled);
    auto &file = (*files)[i];
    file.setAttributes(file.attributes() | GeneratedFile::KeepExistingFileAttribute);
  }

  return OverwriteOk;
}

/*!
    Constructs a file name including \a path, adding the \a extension unless
    \a baseName already has one.
*/

auto BaseFileWizardFactory::buildFileName(const FilePath &path, const QString &base_name, const QString &extension) -> FilePath
{
  auto rc = path.pathAppended(base_name);

  // Add extension unless user specified something else
  if (constexpr QChar dot = '.'; !extension.isEmpty() && !base_name.contains(dot)) {
    if (!extension.startsWith(dot))
      rc = rc.stringAppended(dot);
    rc = rc.stringAppended(extension);
  }

  if constexpr (debugWizard)
    qDebug() << Q_FUNC_INFO << rc;

  return rc;
}

/*!
    Returns the preferred suffix for \a mimeType.
*/

auto BaseFileWizardFactory::preferredSuffix(const QString &mime_type) -> QString
{
  QString rc;

  if (const auto mt = mimeTypeForName(mime_type); mt.isValid())
    rc = mt.preferredSuffix();

  if (rc.isEmpty())
    qWarning("%s: WARNING: Unable to find a preferred suffix for %s.", Q_FUNC_INFO, mime_type.toUtf8().constData());

  return rc;
}

/*!
    \class Core::WizardDialogParameters
    \inheaderfile coreplugin/basefilewizardfactory.h
    \inmodule Orca

    \brief The WizardDialogParameters class holds parameters for the new file
    wizard dialog.

    \sa Core::GeneratedFile, Core::BaseFileWizardFactory
*/

/*!
    \enum Core::WizardDialogParameters::DialogParameterEnum
    This enum type holds whether to force capital letters for file names.
    \value ForceCapitalLetterForFileName Forces capital letters for file names.
*/

} // namespace Core
