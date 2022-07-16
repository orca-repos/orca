// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <cplusplus/Overview.h>

QT_FORWARD_DECLARE_CLASS(QTextCursor)

namespace CPlusPlus {
class DeclarationAST;
}

namespace CPlusPlus {
class Snapshot;
}

namespace Utils {
class FilePath;
}

namespace CppEditor::Internal {

class DoxygenGenerator {
public:
  DoxygenGenerator();

  enum DocumentationStyle {
    JavaStyle,
    ///< JavaStyle comment: /**
    QtStyle,
    ///< QtStyle comment: /*!
    CppStyleA,
    ///< CppStyle comment variant A: ///
    CppStyleB ///< CppStyle comment variant B: //!
  };

  auto setStyle(DocumentationStyle style) -> void;
  auto setStartComment(bool start) -> void;
  auto setGenerateBrief(bool gen) -> void;
  auto setAddLeadingAsterisks(bool add) -> void;
  auto generate(QTextCursor cursor, const CPlusPlus::Snapshot &snapshot, const Utils::FilePath &documentFilePath) -> QString;
  auto generate(QTextCursor cursor, CPlusPlus::DeclarationAST *decl) -> QString;

private:
  auto startMark() const -> QChar;
  auto styleMark() const -> QChar;

  enum Command {
    BriefCommand,
    ParamCommand,
    ReturnCommand
  };

  static auto commandSpelling(Command command) -> QString;
  auto writeStart(QString *comment) const -> void;
  auto writeEnd(QString *comment) const -> void;
  auto writeContinuation(QString *comment) const -> void;
  auto writeNewLine(QString *comment) const -> void;
  auto writeCommand(QString *comment, Command command, const QString &commandContent = QString()) const -> void;
  auto writeBrief(QString *comment, const QString &brief, const QString &prefix = QString(), const QString &suffix = QString()) -> void;
  auto assignCommentOffset(QTextCursor cursor) -> void;
  auto offsetString() const -> QString;

  bool m_addLeadingAsterisks = true;
  bool m_generateBrief = true;
  bool m_startComment = true;
  DocumentationStyle m_style = QtStyle;
  CPlusPlus::Overview m_printer;
  QString m_commentOffset;
};

} // namespace CppEditor::Internal
