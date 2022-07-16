// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "findincurrentfile.hpp"
#include "texteditor.hpp"
#include "textdocument.hpp"

#include <utils/filesearch.hpp>
#include <utils/fileutils.hpp>

#include <core/core-interface.hpp>
#include <core/core-editor-interface.hpp>
#include <core/core-editor-manager.hpp>

#include <QSettings>

using namespace TextEditor;
using namespace Internal;

FindInCurrentFile::FindInCurrentFile()
{
  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::currentEditorChanged, this, &FindInCurrentFile::handleFileChange);
  handleFileChange(Orca::Plugin::Core::EditorManager::currentEditor());
}

auto FindInCurrentFile::id() const -> QString
{
  return QLatin1String("Current File");
}

auto FindInCurrentFile::displayName() const -> QString
{
  return tr("Current File");
}

auto FindInCurrentFile::files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator*
{
  Q_UNUSED(nameFilters)
  Q_UNUSED(exclusionFilters)
  auto fileName = additionalParameters.toString();
  const auto openEditorEncodings = TextDocument::openedTextDocumentEncodings();
  auto codec = openEditorEncodings.value(fileName);
  if (!codec)
    codec = Orca::Plugin::Core::EditorManager::defaultTextCodec();
  return new Utils::FileListIterator({fileName}, {codec});
}

auto FindInCurrentFile::additionalParameters() const -> QVariant
{
  return QVariant::fromValue(m_currentDocument->filePath().toString());
}

auto FindInCurrentFile::label() const -> QString
{
  return tr("File \"%1\":").arg(m_currentDocument->filePath().fileName());
}

auto FindInCurrentFile::toolTip() const -> QString
{
  // %2 is filled by BaseFileFind::runNewSearch
  return tr("File path: %1\n%2").arg(m_currentDocument->filePath().toUserOutput());
}

auto FindInCurrentFile::isEnabled() const -> bool
{
  return m_currentDocument && !m_currentDocument->filePath().isEmpty();
}

auto FindInCurrentFile::handleFileChange(Orca::Plugin::Core::IEditor *editor) -> void
{
  if (!editor) {
    m_currentDocument = nullptr;
    emit enabledChanged(isEnabled());
  } else {
    Orca::Plugin::Core::IDocument *document = editor->document();
    if (document != m_currentDocument) {
      m_currentDocument = document;
      emit enabledChanged(isEnabled());
    }
  }
}

auto FindInCurrentFile::writeSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("FindInCurrentFile"));
  writeCommonSettings(settings);
  settings->endGroup();
}

auto FindInCurrentFile::readSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("FindInCurrentFile"));
  readCommonSettings(settings, "*", "");
  settings->endGroup();
}
