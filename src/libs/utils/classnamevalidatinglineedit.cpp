// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classnamevalidatinglineedit.h"

#include <utils/qtcassert.h>

#include <QDebug>
#include <QRegularExpression>

/*!
    \class Utils::ClassNameValidatingLineEdit

    \brief The ClassNameValidatingLineEdit class implements a line edit that
    validates a C++ class name and emits a signal
    to derive suggested file names from it.
*/

namespace Utils {

// Match something like "Namespace1::Namespace2::ClassName".

struct ClassNameValidatingLineEditPrivate {
  QRegularExpression m_nameRegexp;
  QString m_namespaceDelimiter{"::"};
  bool m_namespacesEnabled = false;
  bool m_lowerCaseFileName = true;
  bool m_forceFirstCapitalLetter = false;
};

// --------------------- ClassNameValidatingLineEdit
ClassNameValidatingLineEdit::ClassNameValidatingLineEdit(QWidget *parent) : FancyLineEdit(parent), d(new ClassNameValidatingLineEditPrivate)
{
  setValidationFunction([this](FancyLineEdit *edit, QString *errorMessage) {
    return validateClassName(edit, errorMessage);
  });
  updateRegExp();
}

ClassNameValidatingLineEdit::~ClassNameValidatingLineEdit()
{
  delete d;
}

auto ClassNameValidatingLineEdit::namespacesEnabled() const -> bool
{
  return d->m_namespacesEnabled;
}

auto ClassNameValidatingLineEdit::setNamespacesEnabled(bool b) -> void
{
  d->m_namespacesEnabled = b;
}

/**
 * @return Language-specific namespace delimiter, e.g. '::' or '.'
 */
auto ClassNameValidatingLineEdit::namespaceDelimiter() -> QString
{
  return d->m_namespaceDelimiter;
}

/**
 * @brief Sets language-specific namespace delimiter, e.g. '::' or '.'
 * Do not use identifier characters in delimiter
 */
auto ClassNameValidatingLineEdit::setNamespaceDelimiter(const QString &delimiter) -> void
{
  d->m_namespaceDelimiter = delimiter;
}

auto ClassNameValidatingLineEdit::validateClassName(FancyLineEdit *edit, QString *errorMessage) const -> bool
{
  QTC_ASSERT(d->m_nameRegexp.isValid(), return false);

  const QString value = edit->text();
  if (!d->m_namespacesEnabled && value.contains(d->m_namespaceDelimiter)) {
    if (errorMessage)
      *errorMessage = tr("The class name must not contain namespace delimiters.");
    return false;
  } else if (value.isEmpty()) {
    if (errorMessage)
      *errorMessage = tr("Please enter a class name.");
    return false;
  } else if (!d->m_nameRegexp.match(value).hasMatch()) {
    if (errorMessage)
      *errorMessage = tr("The class name contains invalid characters.");
    return false;
  }
  return true;
}

auto ClassNameValidatingLineEdit::handleChanged(const QString &t) -> void
{
  if (isValid()) {
    // Suggest file names, strip namespaces
    QString fileName = d->m_lowerCaseFileName ? t.toLower() : t;
    if (d->m_namespacesEnabled) {
      const int namespaceIndex = fileName.lastIndexOf(d->m_namespaceDelimiter);
      if (namespaceIndex != -1)
        fileName.remove(0, namespaceIndex + d->m_namespaceDelimiter.size());
    }
    emit updateFileName(fileName);
  }
}

auto ClassNameValidatingLineEdit::fixInputString(const QString &string) -> QString
{
  if (!forceFirstCapitalLetter())
    return string;

  QString fixedString = string;
  if (!string.isEmpty() && string.at(0).isLower())
    fixedString[0] = string.at(0).toUpper();

  return fixedString;
}

auto ClassNameValidatingLineEdit::updateRegExp() const -> void
{
  const QString pattern = "^%1(%2%1)*$";
  d->m_nameRegexp.setPattern(pattern.arg("[a-zA-Z_][a-zA-Z0-9_]*").arg(QRegularExpression::escape(d->m_namespaceDelimiter)));
}

auto ClassNameValidatingLineEdit::createClassName(const QString &name) -> QString
{
  // Remove spaces and convert the adjacent characters to uppercase
  QString className = name;
  const QRegularExpression spaceMatcher(" +(\\w)");
  QTC_CHECK(spaceMatcher.isValid());
  while (true) {
    const QRegularExpressionMatch match = spaceMatcher.match(className);
    if (!match.hasMatch())
      break;
    className.replace(match.capturedStart(), match.capturedLength(), match.captured(1).toUpper());
  }

  // Filter out any remaining invalid characters
  className.remove(QRegularExpression("[^a-zA-Z0-9_]"));

  // If the first character is numeric, prefix the name with a "_"
  if (className.at(0).isNumber()) {
    className.prepend(QLatin1Char('_'));
  } else {
    // Convert the first character to uppercase
    className.replace(0, 1, className.left(1).toUpper());
  }

  return className;
}

auto ClassNameValidatingLineEdit::lowerCaseFileName() const -> bool
{
  return d->m_lowerCaseFileName;
}

auto ClassNameValidatingLineEdit::setLowerCaseFileName(bool v) -> void
{
  d->m_lowerCaseFileName = v;
}

auto ClassNameValidatingLineEdit::forceFirstCapitalLetter() const -> bool
{
  return d->m_forceFirstCapitalLetter;
}

auto ClassNameValidatingLineEdit::setForceFirstCapitalLetter(bool b) -> void
{
  d->m_forceFirstCapitalLetter = b;
}

} // namespace Utils
