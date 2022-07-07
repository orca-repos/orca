// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idocument.hpp"

#include <utils/textfileformat.hpp>

namespace Core {
namespace Internal {
class TextDocumentPrivate;
}

class CORE_EXPORT BaseTextDocument : public IDocument {
  Q_OBJECT

public:
  using ReadResult = Utils::TextFileFormat::ReadResult;

  explicit BaseTextDocument(QObject *parent = nullptr);
  ~BaseTextDocument() override;

  auto format() const -> Utils::TextFileFormat;
  auto codec() const -> const QTextCodec*;
  auto setCodec(const QTextCodec *) const -> void;
  virtual auto supportsCodec(const QTextCodec *) const -> bool;
  auto switchUtf8Bom() const -> void;
  auto supportsUtf8Bom() const -> bool;
  auto lineTerminationMode() const -> Utils::TextFileFormat::LineTerminationMode;
  auto read(const Utils::FilePath &file_path, QStringList *plain_text_list, QString *error_string) const -> ReadResult;
  auto read(const Utils::FilePath &file_path, QString *plain_text, QString *error_string) const -> ReadResult;
  auto hasDecodingError() const -> bool;
  auto decodingErrorSample() const -> QByteArray;
  auto write(const Utils::FilePath &file_path, const QString &data, QString *error_message) const -> bool;
  auto write(const Utils::FilePath &file_path, const Utils::TextFileFormat &format, const QString &data, QString *error_message) const -> bool;
  auto setSupportsUtf8Bom(bool value) const -> void;
  auto setLineTerminationMode(Utils::TextFileFormat::LineTerminationMode mode) const -> void;

private:
  Internal::TextDocumentPrivate *d;
};

} // namespace Core
