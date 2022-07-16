// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "findinopenfiles.hpp"
#include "textdocument.hpp"
#include "texteditor.hpp"

#include <utils/filesearch.hpp>

#include <core/core-interface.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-document-model.hpp>

#include <QSettings>

using namespace TextEditor;
using namespace Internal;

FindInOpenFiles::FindInOpenFiles()
{
  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::editorOpened, this, &FindInOpenFiles::updateEnabledState);
  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::editorsClosed, this, &FindInOpenFiles::updateEnabledState);
}

auto FindInOpenFiles::id() const -> QString
{
  return QLatin1String("Open Files");
}

auto FindInOpenFiles::displayName() const -> QString
{
  return tr("Open Documents");
}

auto FindInOpenFiles::files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator*
{
  Q_UNUSED(nameFilters)
  Q_UNUSED(exclusionFilters)
  Q_UNUSED(additionalParameters)
  const auto openEditorEncodings = TextDocument::openedTextDocumentEncodings();
  QStringList fileNames;
  QList<QTextCodec*> codecs;
  foreach(Orca::Plugin::Core::DocumentModel::Entry *entry, Orca::Plugin::Core::DocumentModel::entries()) {
    QString fileName = entry->fileName().toString();
    if (!fileName.isEmpty()) {
      fileNames.append(fileName);
      QTextCodec *codec = openEditorEncodings.value(fileName);
      if (!codec)
        codec = Orca::Plugin::Core::EditorManager::defaultTextCodec();
      codecs.append(codec);
    }
  }

  return new Utils::FileListIterator(fileNames, codecs);
}

auto FindInOpenFiles::additionalParameters() const -> QVariant
{
  return QVariant();
}

auto FindInOpenFiles::label() const -> QString
{
  return tr("Open documents:");
}

auto FindInOpenFiles::toolTip() const -> QString
{
  // %1 is filled by BaseFileFind::runNewSearch
  return tr("Open Documents\n%1");
}

auto FindInOpenFiles::isEnabled() const -> bool
{
  return Orca::Plugin::Core::DocumentModel::entryCount() > 0;
}

auto FindInOpenFiles::writeSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("FindInOpenFiles"));
  writeCommonSettings(settings);
  settings->endGroup();
}

auto FindInOpenFiles::readSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("FindInOpenFiles"));
  readCommonSettings(settings, "*", "");
  settings->endGroup();
}

auto FindInOpenFiles::updateEnabledState() -> void
{
  emit enabledChanged(isEnabled());
}
