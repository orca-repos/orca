// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textdocument.hpp"

#include <core/editormanager/editormanager.hpp>

#include <QDebug>
#include <QTextCodec>

/*!
    \class Core::BaseTextDocument
    \inheaderfile coreplugin/textdocument.h
    \inmodule Orca

    \brief The BaseTextDocument class is a very general base class for
    documents that work with text.

    This class contains helper methods for saving and reading text files with encoding and
    line ending settings.

    \sa Utils::TextFileFormat
*/

enum {
  debug = 0
};

namespace Core {
namespace Internal {

class TextDocumentPrivate {
public:
  Utils::TextFileFormat m_format;
  Utils::TextFileFormat::ReadResult m_read_result = Utils::TextFileFormat::ReadSuccess;
  QByteArray m_decoding_error_sample;
  bool m_supports_utf8_bom = true;
};

} // namespace Internal

BaseTextDocument::BaseTextDocument(QObject *parent) : IDocument(parent), d(new Internal::TextDocumentPrivate)
{
  setCodec(EditorManager::defaultTextCodec());
  setLineTerminationMode(EditorManager::defaultLineEnding());
}

BaseTextDocument::~BaseTextDocument()
{
  delete d;
}

auto BaseTextDocument::hasDecodingError() const -> bool
{
  return d->m_read_result == Utils::TextFileFormat::ReadEncodingError;
}

auto BaseTextDocument::decodingErrorSample() const -> QByteArray
{
  return d->m_decoding_error_sample;
}

/*!
    Writes out the contents (\a data) of the text file \a filePath.
    Uses the format obtained from the last read() of the file.

    If an error occurs while writing the file, \a errorMessage is set to the
    error details.

    Returns whether the operation was successful.
*/

auto BaseTextDocument::write(const Utils::FilePath &file_path, const QString &data, QString *error_message) const -> bool
{
  return write(file_path, format(), data, error_message);
}

/*!
    Writes out the contents (\a data) of the text file \a filePath.
    Uses the custom format \a format.

    If an error occurs while writing the file, \a errorMessage is set to the
    error details.

    Returns whether the operation was successful.
*/

auto BaseTextDocument::write(const Utils::FilePath &file_path, const Utils::TextFileFormat &format, const QString &data, QString *error_message) const -> bool
{
  if constexpr (debug)
    qDebug() << Q_FUNC_INFO << this << file_path;
  return format.writeFile(file_path, data, error_message);
}

auto BaseTextDocument::setSupportsUtf8Bom(const bool value) const -> void
{
  d->m_supports_utf8_bom = value;
}

auto BaseTextDocument::setLineTerminationMode(const Utils::TextFileFormat::LineTerminationMode mode) const -> void
{
  d->m_format.lineTerminationMode = mode;
}

/*!
    Autodetects file format and reads the text file specified by \a filePath
    into a list of strings specified by \a plainTextList.

    If an error occurs while writing the file, \a errorString is set to the
    error details.

    Returns whether the operation was successful.
*/

auto BaseTextDocument::read(const Utils::FilePath &file_path, QStringList *plain_text_list, QString *error_string) const -> ReadResult
{
  d->m_read_result = Utils::TextFileFormat::readFile(file_path, codec(), plain_text_list, &d->m_format, error_string, &d->m_decoding_error_sample);
  return d->m_read_result;
}

/*!
    Autodetects file format and reads the text file specified by \a filePath
    into \a plainText.

    If an error occurs while writing the file, \a errorString is set to the
    error details.

    Returns whether the operation was successful.
*/

auto BaseTextDocument::read(const Utils::FilePath &file_path, QString *plain_text, QString *error_string) const -> ReadResult
{
  d->m_read_result = Utils::TextFileFormat::readFile(file_path, codec(), plain_text, &d->m_format, error_string, &d->m_decoding_error_sample);
  return d->m_read_result;
}

auto BaseTextDocument::codec() const -> const QTextCodec*
{
  return d->m_format.codec;
}

auto BaseTextDocument::setCodec(const QTextCodec *codec) const -> void
{
  if constexpr (debug)
    qDebug() << Q_FUNC_INFO << this << (codec ? codec->name() : QByteArray());
  if (supportsCodec(codec))
    d->m_format.codec = codec;
}

auto BaseTextDocument::supportsCodec(const QTextCodec *) const -> bool
{
  return true;
}

auto BaseTextDocument::switchUtf8Bom() const -> void
{
  if constexpr (debug)
    qDebug() << Q_FUNC_INFO << this << "UTF-8 BOM: " << !d->m_format.hasUtf8Bom;
  d->m_format.hasUtf8Bom = !d->m_format.hasUtf8Bom;
}

auto BaseTextDocument::supportsUtf8Bom() const -> bool
{
  return d->m_supports_utf8_bom;
}

auto BaseTextDocument::lineTerminationMode() const -> Utils::TextFileFormat::LineTerminationMode
{
  return d->m_format.lineTerminationMode;
}

/*!
    Returns the format obtained from the last call to read().
*/

auto BaseTextDocument::format() const -> Utils::TextFileFormat
{
  return d->m_format;
}

} // namespace Core
