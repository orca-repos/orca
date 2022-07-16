// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"
#include "projectconfiguration.hpp"

#include <projectexplorer/task.hpp>
#include <utils/detailswidget.hpp>

#include <QRegularExpression>
#include <QVariantMap>

namespace ProjectExplorer {
class Target;

class PROJECTEXPLORER_EXPORT CustomParserExpression {
public:
  enum CustomParserChannel {
    ParseNoChannel = 0,
    ParseStdErrChannel = 1,
    ParseStdOutChannel = 2,
    ParseBothChannels = 3
  };

  auto operator ==(const CustomParserExpression &other) const -> bool;
  auto pattern() const -> QString;
  auto setPattern(const QString &pattern) -> void;
  auto match(const QString &line) const -> QRegularExpressionMatch { return m_regExp.match(line); }
  auto channel() const -> CustomParserChannel;
  auto setChannel(CustomParserChannel channel) -> void;
  auto example() const -> QString;
  auto setExample(const QString &example) -> void;
  auto fileNameCap() const -> int;
  auto setFileNameCap(int fileNameCap) -> void;
  auto lineNumberCap() const -> int;
  auto setLineNumberCap(int lineNumberCap) -> void;
  auto messageCap() const -> int;
  auto setMessageCap(int messageCap) -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;

private:
  QRegularExpression m_regExp;
  CustomParserChannel m_channel = ParseBothChannels;
  QString m_example;
  int m_fileNameCap = 1;
  int m_lineNumberCap = 2;
  int m_messageCap = 3;
};

class PROJECTEXPLORER_EXPORT CustomParserSettings {
public:
  auto operator ==(const CustomParserSettings &other) const -> bool;
  auto operator !=(const CustomParserSettings &other) const -> bool { return !operator==(other); }
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;

  Utils::Id id;
  QString displayName;
  CustomParserExpression error;
  CustomParserExpression warning;
};

class PROJECTEXPLORER_EXPORT CustomParsersAspect : public Utils::BaseAspect {
  Q_OBJECT

public:
  CustomParsersAspect(Target *target);

  auto setParsers(const QList<Utils::Id> &parsers) -> void { m_parsers = parsers; }
  auto parsers() const -> const QList<Utils::Id> { return m_parsers; }

private:
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &map) const -> void override;

  QList<Utils::Id> m_parsers;
};

namespace Internal {

class CustomParser : public OutputTaskParser {
public:
  CustomParser(const CustomParserSettings &settings = CustomParserSettings());

  auto setSettings(const CustomParserSettings &settings) -> void;

  static auto createFromId(Utils::Id id) -> CustomParser*;
  static auto id() -> Utils::Id;

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto hasMatch(const QString &line, CustomParserExpression::CustomParserChannel channel, const CustomParserExpression &expression, Task::TaskType taskType) -> Result;
  auto parseLine(const QString &rawLine, CustomParserExpression::CustomParserChannel channel) -> Result;

  CustomParserExpression m_error;
  CustomParserExpression m_warning;
};

class CustomParsersSelectionWidget : public Utils::DetailsWidget {
  Q_OBJECT

public:
  CustomParsersSelectionWidget(QWidget *parent = nullptr);

  auto setSelectedParsers(const QList<Utils::Id> &parsers) -> void;
  auto selectedParsers() const -> QList<Utils::Id>;

signals:
  auto selectionChanged() -> void;

private:
  auto updateSummary() -> void;
};

} // namespace Internal
} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::CustomParserExpression::CustomParserChannel);
