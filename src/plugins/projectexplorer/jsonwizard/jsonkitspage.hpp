// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "jsonwizard.hpp"
#include "../targetsetuppage.hpp"

#include <QVector>

namespace ProjectExplorer {

class JsonKitsPage : public TargetSetupPage {
  Q_OBJECT

public:
  JsonKitsPage(QWidget *parent = nullptr);

  auto initializePage() -> void override;
  auto cleanupPage() -> void override;
  auto setUnexpandedProjectPath(const QString &path) -> void;
  auto unexpandedProjectPath() const -> QString;
  auto setRequiredFeatures(const QVariant &data) -> void;
  auto setPreferredFeatures(const QVariant &data) -> void;

  class ConditionalFeature {
  public:
    ConditionalFeature() = default;
    ConditionalFeature(const QString &f, const QVariant &c) : feature(f), condition(c) { }

    QString feature;
    QVariant condition;
  };

  static auto parseFeatures(const QVariant &data, QString *errorMessage = nullptr) -> QVector<ConditionalFeature>;

private:
  auto setupProjectFiles(const JsonWizard::GeneratorFiles &files) -> void;

  QString m_unexpandedProjectPath;
  QVector<ConditionalFeature> m_requiredFeatures;
  QVector<ConditionalFeature> m_preferredFeatures;

  auto evaluate(const QVector<ConditionalFeature> &list, const QVariant &defaultSet, JsonWizard *wiz) -> QSet<Utils::Id>;
};

} // namespace ProjectExplorer
