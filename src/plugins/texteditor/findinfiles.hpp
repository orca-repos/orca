// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "basefilefind.hpp"

#include <utils/fileutils.hpp>

#include <QPointer>

QT_BEGIN_NAMESPACE
class QComboBox;
class QStackedWidget;
QT_END_NAMESPACE

namespace Utils {
class PathChooser;
}

namespace TextEditor {

class TEXTEDITOR_EXPORT FindInFiles : public BaseFileFind {
  Q_OBJECT

public:
  FindInFiles();
  ~FindInFiles() override;

  auto id() const -> QString override;
  auto displayName() const -> QString override;
  auto createConfigWidget() -> QWidget* override;
  auto writeSettings(QSettings *settings) -> void override;
  auto readSettings(QSettings *settings) -> void override;
  auto isValid() const -> bool override;
  auto setDirectory(const Utils::FilePath &directory) -> void;
  auto setBaseDirectory(const Utils::FilePath &directory) -> void;
  auto directory() const -> Utils::FilePath;

  static auto findOnFileSystem(const QString &path) -> void;
  static auto instance() -> FindInFiles*;

signals:
  auto pathChanged(const Utils::FilePath &directory) -> void;

protected:
  auto files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator* override;
  auto additionalParameters() const -> QVariant override;
  auto label() const -> QString override;
  auto toolTip() const -> QString override;
  auto syncSearchEngineCombo(int selectedSearchEngineIndex) -> void override;

private:
  auto setValid(bool valid) -> void;
  auto searchEnginesSelectionChanged(int index) -> void;
  auto path() const -> Utils::FilePath;

  QPointer<QWidget> m_configWidget;
  QPointer<Utils::PathChooser> m_directory;
  QStackedWidget *m_searchEngineWidget = nullptr;
  QComboBox *m_searchEngineCombo = nullptr;
  bool m_isValid = false;
};

} // namespace TextEditor
