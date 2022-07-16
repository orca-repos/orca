// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-welcome-page-helper.hpp>

#include <qtsupport/baseqtversion.hpp>

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStringList>
#include <QXmlStreamReader>

namespace QtSupport {
namespace Internal {

class ExamplesListModel;

class ExampleSetModel : public QStandardItemModel {
  Q_OBJECT public:
  struct ExtraExampleSet {
    QString displayName;
    QString manifestPath;
    QString examplesPath;
  };

  static auto pluginRegisteredExampleSets() -> QVector<ExtraExampleSet>;

  ExampleSetModel();

  auto selectedExampleSet() const -> int { return m_selectedExampleSetIndex; }
  auto selectExampleSet(int index) -> void;
  auto exampleSources(QString *examplesInstallPath, QString *demosInstallPath) -> QStringList;
  auto selectedQtSupports(const Utils::Id &target) const -> bool;

signals:
  auto selectedExampleSetChanged(int) -> void;

private:
  enum ExampleSetType {
    InvalidExampleSet,
    QtExampleSet,
    ExtraExampleSetType
  };

  auto writeCurrentIdToSettings(int currentIndex) const -> void;
  auto readCurrentIndexFromSettings() const -> int;
  auto getDisplayName(int index) const -> QVariant;
  auto getId(int index) const -> QVariant;
  auto getType(int i) const -> ExampleSetType;
  auto getQtId(int index) const -> int;
  auto getExtraExampleSetIndex(int index) const -> int;
  auto findHighestQtVersion(const QtVersions &versions) const -> QtVersion*;
  auto indexForQtVersion(QtVersion *qtVersion) const -> int;
  auto recreateModel(const QtVersions &qtVersions) -> void;
  auto updateQtVersionList() -> void;
  auto qtVersionManagerLoaded() -> void;
  auto helpManagerInitialized() -> void;
  auto tryToInitialize() -> void;

  QVector<ExtraExampleSet> m_extraExampleSets;
  int m_selectedExampleSetIndex = -1;
  QSet<Utils::Id> m_selectedQtTypes;
  bool m_qtVersionManagerInitialized = false;
  bool m_helpManagerInitialized = false;
  bool m_initalized = false;
};

enum InstructionalType {
  Example = 0,
  Demo,
  Tutorial
};

class ExampleItem : public Orca::Plugin::Core::ListItem {
public:
  QString projectPath;
  QString docUrl;
  QStringList filesToOpen;
  QString mainFile; /* file to be visible after opening filesToOpen */
  QStringList dependencies;
  InstructionalType type;
  int difficulty = 0;
  bool hasSourceCode = false;
  bool isVideo = false;
  bool isHighlighted = false;
  QString videoUrl;
  QString videoLength;
  QStringList platforms;
};

class ExamplesListModel : public Orca::Plugin::Core::ListModel {
  Q_OBJECT

public:
  explicit ExamplesListModel(QObject *parent);

  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant final;
  auto updateExamples() -> void;
  auto exampleSets() const -> QStringList;
  auto exampleSetModel() -> ExampleSetModel* { return &m_exampleSetModel; }
  auto fetchPixmapAndUpdatePixmapCache(const QString &url) const -> QPixmap override;

signals:
  auto selectedExampleSetChanged(int) -> void;

private:
  auto updateSelectedQtVersion() -> void;
  auto parseExamples(QXmlStreamReader *reader, const QString &projectsOffset, const QString &examplesInstallPath) -> void;
  auto parseDemos(QXmlStreamReader *reader, const QString &projectsOffset, const QString &demosInstallPath) -> void;
  auto parseTutorials(QXmlStreamReader *reader, const QString &projectsOffset) -> void;

  ExampleSetModel m_exampleSetModel;
};

class ExamplesListModelFilter : public Orca::Plugin::Core::ListModelFilter {
public:
  ExamplesListModelFilter(ExamplesListModel *sourceModel, bool showTutorialsOnly, QObject *parent);

protected:
  auto leaveFilterAcceptsRowBeforeFiltering(const Orca::Plugin::Core::ListItem *item, bool *earlyExitResult) const -> bool override;

private:
  const bool m_showTutorialsOnly;
  ExamplesListModel *m_examplesListModel = nullptr;
};

} // namespace Internal
} // namespace QtSupport

Q_DECLARE_METATYPE(QtSupport::Internal::ExampleItem *)
