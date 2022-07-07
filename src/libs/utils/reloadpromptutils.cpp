// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fileutils.hpp"
#include "reloadpromptutils.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>

namespace Utils {

ORCA_UTILS_EXPORT auto reloadPrompt(const FilePath &fileName, bool modified, bool enableDiffOption, QWidget *parent) -> ReloadPromptAnswer
{
  const QString title = QCoreApplication::translate("Utils::reloadPrompt", "File Changed");
  QString msg;

  if (modified) {
    msg = QCoreApplication::translate("Utils::reloadPrompt", "The unsaved file <i>%1</i> has been changed on disk. " "Do you want to reload it and discard your changes?");
  } else {
    msg = QCoreApplication::translate("Utils::reloadPrompt", "The file <i>%1</i> has been changed on disk. Do you want to reload it?");
  }
  msg = "<p>" + msg.arg(fileName.fileName()) + "</p><p>" + QCoreApplication::translate("Utils::reloadPrompt", "The default behavior can be set in Edit > Preferences > Environment > System..") + "</p>";
  return reloadPrompt(title, msg, fileName.toUserOutput(), enableDiffOption, parent);
}

ORCA_UTILS_EXPORT auto reloadPrompt(const QString &title, const QString &prompt, const QString &details, bool enableDiffOption, QWidget *parent) -> ReloadPromptAnswer
{
  QMessageBox msg(parent);
  msg.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::Close | QMessageBox::No | QMessageBox::NoToAll);
  msg.setDefaultButton(QMessageBox::YesToAll);
  msg.setWindowTitle(title);
  msg.setText(prompt);
  msg.setDetailedText(details);

  msg.button(QMessageBox::Close)->setText(QCoreApplication::translate("Utils::reloadPrompt", "&Close"));

  QPushButton *diffButton = nullptr;
  if (enableDiffOption) {
    diffButton = msg.addButton(QCoreApplication::translate("Utils::reloadPrompt", "No to All && &Diff"), QMessageBox::NoRole);
  }

  const int result = msg.exec();

  if (msg.clickedButton() == diffButton)
    return ReloadNoneAndDiff;

  switch (result) {
  case QMessageBox::Yes:
    return ReloadCurrent;
  case QMessageBox::YesToAll:
    return ReloadAll;
  case QMessageBox::No:
    return ReloadSkipCurrent;
  case QMessageBox::Close:
    return CloseCurrent;
  default:
    break;
  }
  return ReloadNone;
}

ORCA_UTILS_EXPORT auto fileDeletedPrompt(const QString &fileName, QWidget *parent) -> FileDeletedPromptAnswer
{
  const QString title = QCoreApplication::translate("Utils::fileDeletedPrompt", "File Has Been Removed");
  QString msg = QCoreApplication::translate("Utils::fileDeletedPrompt", "The file %1 has been removed from disk. " "Do you want to save it under a different name, or close " "the editor?").arg(QDir::toNativeSeparators(fileName));
  QMessageBox box(QMessageBox::Question, title, msg, QMessageBox::NoButton, parent);
  QPushButton *close = box.addButton(QCoreApplication::translate("Utils::fileDeletedPrompt", "&Close"), QMessageBox::RejectRole);
  QPushButton *closeAll = box.addButton(QCoreApplication::translate("Utils::fileDeletedPrompt", "C&lose All"), QMessageBox::RejectRole);
  QPushButton *saveas = box.addButton(QCoreApplication::translate("Utils::fileDeletedPrompt", "Save &as..."), QMessageBox::ActionRole);
  QPushButton *save = box.addButton(QCoreApplication::translate("Utils::fileDeletedPrompt", "&Save"), QMessageBox::AcceptRole);
  box.setDefaultButton(saveas);
  box.exec();
  QAbstractButton *clickedbutton = box.clickedButton();
  if (clickedbutton == close)
    return FileDeletedClose;
  if (clickedbutton == closeAll)
    return FileDeletedCloseAll;
  if (clickedbutton == saveas)
    return FileDeletedSaveAs;
  if (clickedbutton == save)
    return FileDeletedSave;
  return FileDeletedClose;
}

} // namespace Utils
