// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QString>

QT_BEGIN_NAMESPACE
template <class K, class T>
class QMap;
class QFutureInterfaceBase;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT Diff {
public:
  enum Command {
    Delete,
    Insert,
    Equal
  };

  Command command = Equal;
  QString text;
  Diff() = default;
  Diff(Command com, const QString &txt = {});
  auto operator==(const Diff &other) const -> bool;
  auto operator!=(const Diff &other) const -> bool;
  auto toString() const -> QString;
  static auto commandString(Command com) -> QString;
};

class ORCA_UTILS_EXPORT Differ {
public:
  enum DiffMode {
    CharMode,
    WordMode,
    LineMode
  };

  Differ(QFutureInterfaceBase *jobController = nullptr);
  auto diff(const QString &text1, const QString &text2) -> QList<Diff>;
  auto unifiedDiff(const QString &text1, const QString &text2) -> QList<Diff>;
  auto setDiffMode(DiffMode mode) -> void;
  auto diffMode() const -> DiffMode;
  static auto merge(const QList<Diff> &diffList) -> QList<Diff>;
  static auto cleanupSemantics(const QList<Diff> &diffList) -> QList<Diff>;
  static auto cleanupSemanticsLossless(const QList<Diff> &diffList) -> QList<Diff>;
  static auto splitDiffList(const QList<Diff> &diffList, QList<Diff> *leftDiffList, QList<Diff> *rightDiffList) -> void;
  static auto moveWhitespaceIntoEqualities(const QList<Diff> &input) -> QList<Diff>;
  static auto diffWithWhitespaceReduced(const QString &leftInput, const QString &rightInput, QList<Diff> *leftOutput, QList<Diff> *rightOutput) -> void;
  static auto unifiedDiffWithWhitespaceReduced(const QString &leftInput, const QString &rightInput, QList<Diff> *leftOutput, QList<Diff> *rightOutput) -> void;
  static auto ignoreWhitespaceBetweenEqualities(const QList<Diff> &leftInput, const QList<Diff> &rightInput, QList<Diff> *leftOutput, QList<Diff> *rightOutput) -> void;
  static auto diffBetweenEqualities(const QList<Diff> &leftInput, const QList<Diff> &rightInput, QList<Diff> *leftOutput, QList<Diff> *rightOutput) -> void;

private:
  auto preprocess1AndDiff(const QString &text1, const QString &text2) -> QList<Diff>;
  auto preprocess2AndDiff(const QString &text1, const QString &text2) -> QList<Diff>;
  auto diffMyers(const QString &text1, const QString &text2) -> QList<Diff>;
  auto diffMyersSplit(const QString &text1, int x, const QString &text2, int y) -> QList<Diff>;
  auto diffNonCharMode(const QString &text1, const QString &text2) -> QList<Diff>;
  auto encode(const QString &text1, const QString &text2, QString *encodedText1, QString *encodedText2) -> QStringList;
  auto encode(const QString &text, QStringList *lines, QMap<QString, int> *lineToCode) -> QString;
  auto findSubtextEnd(const QString &text, int subTextStart) -> int;
  DiffMode m_diffMode = Differ::LineMode;
  DiffMode m_currentDiffMode = Differ::LineMode;
  QFutureInterfaceBase *m_jobController = nullptr;
};

} // namespace Utils
