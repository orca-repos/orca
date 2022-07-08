// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/basefilefind.hpp>

#include <QPointer>

namespace ProjectExplorer {

class Project;

namespace Internal {

class AllProjectsFind : public TextEditor::BaseFileFind {
  Q_OBJECT

public:
  AllProjectsFind();

  auto id() const -> QString override;
  auto displayName() const -> QString override;
  auto isEnabled() const -> bool override;
  auto createConfigWidget() -> QWidget* override;
  auto writeSettings(QSettings *settings) -> void override;
  auto readSettings(QSettings *settings) -> void override;

protected:
  auto files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator* override;
  auto filesForProjects(const QStringList &nameFilters, const QStringList &exclusionFilters, const QList<Project*> &projects) const -> Utils::FileIterator*;
  auto additionalParameters() const -> QVariant override;
  auto label() const -> QString override;
  auto toolTip() const -> QString override;

private:
  auto handleFileListChanged() -> void;

  QPointer<QWidget> m_configWidget;
};

} // namespace Internal
} // namespace ProjectExplorer
