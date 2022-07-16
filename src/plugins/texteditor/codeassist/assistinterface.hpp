// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "assistenums.hpp"

#include <texteditor/texteditor_global.hpp>

#include <utils/fileutils.hpp>

#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT AssistInterface {
public:
  AssistInterface(QTextDocument *textDocument, int position, const Utils::FilePath &filePath, AssistReason reason);
  virtual ~AssistInterface();

  virtual auto position() const -> int { return m_position; }
  virtual auto characterAt(int position) const -> QChar;
  virtual auto textAt(int position, int length) const -> QString;
  virtual auto filePath() const -> Utils::FilePath { return m_filePath; }
  virtual auto textDocument() const -> QTextDocument* { return m_textDocument; }
  virtual auto prepareForAsyncUse() -> void;
  virtual auto recreateTextDocument() -> void;
  virtual auto reason() const -> AssistReason;

private:
  QTextDocument *m_textDocument;
  bool m_isAsync;
  int m_position;
  Utils::FilePath m_filePath;
  AssistReason m_reason;
  QString m_text;
  QVector<int> m_userStates;
};

} // namespace TextEditor
