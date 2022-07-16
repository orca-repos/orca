// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-welcome-page-interface.hpp>

QT_BEGIN_NAMESPACE
class QFileInfo;
QT_END_NAMESPACE

namespace QtSupport {
namespace Internal {

class ExampleItem;

class ExamplesWelcomePage : public Orca::Plugin::Core::IWelcomePage {
  Q_OBJECT

public:
  explicit ExamplesWelcomePage(bool showExamples);

  auto title() const -> QString final;
  auto priority() const -> int final;
  auto id() const -> Utils::Id final;
  auto createWidget() const -> QWidget* final;
  static auto openProject(const ExampleItem *item) -> void;

private:
  static auto copyToAlternativeLocation(const QFileInfo &fileInfo, QStringList &filesToOpen, const QStringList &dependencies) -> QString;
  const bool m_showExamples;
};

} // namespace Internal
} // namespace QtSupport
