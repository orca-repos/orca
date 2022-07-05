// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filenamevalidatinglineedit.h"
#include "qtcassert.h"

#include <QRegularExpression>
#include <QDebug>

/*!
  \class Utils::FileNameValidatingLineEdit

  \brief The FileNameValidatingLineEdit class is a control that lets the user
  choose a (base) file name, based on a QLineEdit.

  The class has
   some validation logic for embedding into QWizardPage.
*/

namespace Utils {

#define WINDOWS_DEVICES_PATTERN "(CON|AUX|PRN|NUL|COM[1-9]|LPT[1-9])(\\..*)?"

// Naming a file like a device name will break on Windows, even if it is
// "com1.txt". Since we are cross-platform, we generally disallow such file
//  names.
static auto windowsDeviceNoSubDirPattern() -> const QRegularExpression&
{
  static const QRegularExpression rc(QString("^" WINDOWS_DEVICES_PATTERN "$"), QRegularExpression::CaseInsensitiveOption);
  QTC_ASSERT(rc.isValid(), return rc);
  return rc;
}

static auto windowsDeviceSubDirPattern() -> const QRegularExpression&
{
  static const QRegularExpression rc(QString("^.*[/\\\\]" WINDOWS_DEVICES_PATTERN "$"), QRegularExpression::CaseInsensitiveOption);
  QTC_ASSERT(rc.isValid(), return rc);
  return rc;
}

FileNameValidatingLineEdit::FileNameValidatingLineEdit(QWidget *parent) : FancyLineEdit(parent), m_allowDirectories(false), m_forceFirstCapitalLetter(false)
{
  setValidationFunction([this](FancyLineEdit *edit, QString *errorMessage) {
    return validateFileNameExtension(edit->text(), requiredExtensions(), errorMessage) && validateFileName(edit->text(), allowDirectories(), errorMessage);
  });
}

auto FileNameValidatingLineEdit::allowDirectories() const -> bool
{
  return m_allowDirectories;
}

auto FileNameValidatingLineEdit::setAllowDirectories(bool v) -> void
{
  m_allowDirectories = v;
}

auto FileNameValidatingLineEdit::forceFirstCapitalLetter() const -> bool
{
  return m_forceFirstCapitalLetter;
}

auto FileNameValidatingLineEdit::setForceFirstCapitalLetter(bool b) -> void
{
  m_forceFirstCapitalLetter = b;
}

/* Validate a file base name, check for forbidden characters/strings. */

#define SLASHES "/\\"

static constexpr char notAllowedCharsSubDir[] = ",^@={}[]~!?:&*\"|#%<>$\"'();`' ";
static constexpr char notAllowedCharsNoSubDir[] = ",^@={}[]~!?:&*\"|#%<>$\"'();`' " SLASHES;

static const char *notAllowedSubStrings[] = {".."};

auto FileNameValidatingLineEdit::validateFileName(const QString &name, bool allowDirectories, QString *errorMessage /* = 0*/) -> bool
{
  if (name.isEmpty()) {
    if (errorMessage)
      *errorMessage = tr("Name is empty.");
    return false;
  }
  // Characters
  const char *notAllowedChars = allowDirectories ? notAllowedCharsSubDir : notAllowedCharsNoSubDir;
  for (const char *c = notAllowedChars; *c; c++)
    if (name.contains(QLatin1Char(*c))) {
      if (errorMessage) {
        const QChar qc = QLatin1Char(*c);
        if (qc.isSpace())
          *errorMessage = tr("Name contains white space.");
        else
          *errorMessage = tr("Invalid character \"%1\".").arg(qc);
      }
      return false;
    }
  // Substrings
  const int notAllowedSubStringCount = sizeof(notAllowedSubStrings) / sizeof(const char*);
  for (int s = 0; s < notAllowedSubStringCount; s++) {
    const QLatin1String notAllowedSubString(notAllowedSubStrings[s]);
    if (name.contains(notAllowedSubString)) {
      if (errorMessage)
        *errorMessage = tr("Invalid characters \"%1\".").arg(QString(notAllowedSubString));
      return false;
    }
  }
  // Windows devices
  bool matchesWinDevice = name.contains(windowsDeviceNoSubDirPattern());
  if (!matchesWinDevice && allowDirectories)
    matchesWinDevice = name.contains(windowsDeviceSubDirPattern());
  if (matchesWinDevice) {
    if (errorMessage)
      *errorMessage = tr("Name matches MS Windows device" " (CON, AUX, PRN, NUL," " COM1, COM2, ..., COM9," " LPT1, LPT2, ..., LPT9)");
    return false;
  }
  return true;
}

auto FileNameValidatingLineEdit::fixInputString(const QString &string) -> QString
{
  if (!forceFirstCapitalLetter())
    return string;

  QString fixedString = string;
  if (!string.isEmpty() && string.at(0).isLower())
    fixedString[0] = string.at(0).toUpper();

  return fixedString;
}

auto FileNameValidatingLineEdit::validateFileNameExtension(const QString &fileName, const QStringList &requiredExtensions, QString *errorMessage) -> bool
{
  // file extension
  if (!requiredExtensions.isEmpty()) {
    for (const QString &requiredExtension : requiredExtensions) {
      QString extension = QLatin1Char('.') + requiredExtension;
      if (fileName.endsWith(extension, Qt::CaseSensitive) && extension.count() < fileName.count())
        return true;
    }

    if (errorMessage) {
      if (requiredExtensions.count() == 1)
        *errorMessage = tr("File extension %1 is required:").arg(requiredExtensions.first());
      else
        *errorMessage = tr("File extensions %1 are required:").arg(requiredExtensions.join(QLatin1String(", ")));
    }

    return false;
  }

  return true;
}

auto FileNameValidatingLineEdit::requiredExtensions() const -> QStringList
{
  return m_requiredExtensionList;
}

auto FileNameValidatingLineEdit::setRequiredExtensions(const QStringList &extensions) -> void
{
  m_requiredExtensionList = extensions;
}

} // namespace Utils
