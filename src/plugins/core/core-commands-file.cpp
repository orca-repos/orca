// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-commands-file.hpp"

#include "core-command.hpp"
#include "core-interface.hpp"
#include "core-shortcut-settings.hpp"

#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>

#include <QDateTime>
#include <QFile>
#include <QKeySequence>
#include <QXmlStreamAttributes>

namespace Orca::Plugin::Core {
namespace Commands {

struct Context {
  const QString mapping_element = "mapping";
  const QString short_cut_element = "shortcut";
  const QString id_attribute = "id";
  const QString key_element = "key";
  const QString value_attribute = "value";
};

} // namespace Commands

using namespace Utils;

// XML parsing context with strings.


/*!
    \class Orca::Plugin::Core::CommandsFile
    \internal
    \inmodule Orca
    \brief The CommandsFile class provides a collection of import and export commands.
    \inheaderfile commandsfile.h
*/

/*!
    \internal
*/
CommandsFile::CommandsFile(FilePath filename) : m_file_path(std::move(filename))
{
}

/*!
    \internal
*/
auto CommandsFile::importCommands() const -> QMap<QString, QList<QKeySequence>>
{
  QMap<QString, QList<QKeySequence>> result;
  QFile file(m_file_path.toString());

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return result;

  QXmlStreamReader r(&file);
  QString current_id;

  while (!r.atEnd()) {
    switch (r.readNext()) {
    case QXmlStreamReader::StartElement: {
      const auto name = r.name();
      if (const Commands::Context ctx; name == ctx.short_cut_element) {
        current_id = r.attributes().value(ctx.id_attribute).toString();
        if (!result.contains(current_id))
          result.insert(current_id, {});
      } else if (name == ctx.key_element) {
        QTC_ASSERT(!current_id.isEmpty(), continue);
        if (const auto attributes = r.attributes(); attributes.hasAttribute(ctx.value_attribute)) {
          const auto key_string = attributes.value(ctx.value_attribute).toString();
          auto keys = result.value(current_id);
          result.insert(current_id, keys << QKeySequence(key_string));
        }
      } // if key element
    }   // case QXmlStreamReader::StartElement
    [[fallthrough]];
    default:
      break;
    } // switch
  }   // while !atEnd
  file.close();
  return result;
}

/*!
    \internal
*/

auto CommandsFile::exportCommands(const QList<ShortcutItem*> &items) const -> bool
{
  FileSaver saver(m_file_path, QIODevice::Text);
  if (!saver.hasError()) {
    const Commands::Context ctx;

    QXmlStreamWriter w(saver.file());
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(1); // Historical, used to be QDom.
    w.writeStartDocument();
    w.writeDTD(QLatin1String("<!DOCTYPE KeyboardMappingScheme>"));
    w.writeComment(QString::fromLatin1(" Written by %1, %2. ").arg(ICore::versionString(), QDateTime::currentDateTime().toString(Qt::ISODate)));
    w.writeStartElement(ctx.mapping_element);

    for (const auto item : qAsConst(items)) {
      const auto id = item->m_cmd->id();
      if (item->m_keys.isEmpty() || item->m_keys.first().isEmpty()) {
        w.writeEmptyElement(ctx.short_cut_element);
        w.writeAttribute(ctx.id_attribute, id.toString());
      } else {
        w.writeStartElement(ctx.short_cut_element);
        w.writeAttribute(ctx.id_attribute, id.toString());
        for (const auto &k : item->m_keys) {
          w.writeEmptyElement(ctx.key_element);
          w.writeAttribute(ctx.value_attribute, k.toString());
        }
        w.writeEndElement(); // Shortcut
      }
    }

    w.writeEndElement();
    w.writeEndDocument();
    saver.setResult(&w);
  }
  return saver.finalize();
}

} // namespace Orca::Plugin::Core
