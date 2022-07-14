// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-generated-file.hpp"
#include "core-global.hpp"
#include "core-wizard-factory-interface.hpp"

#include <utils/filepath.hpp>

#include <QList>

QT_BEGIN_NAMESPACE
class QWizard;
class QWizardPage;
QT_END_NAMESPACE

namespace Utils {
class Wizard;
}

namespace Orca::Plugin::Core {

class BaseFileWizard;

class CORE_EXPORT WizardDialogParameters {
public:
  using wizard_page_list = QList<QWizardPage*>;

  enum DialogParameterEnum {
    ForceCapitalLetterForFileName = 0x01
  };

  Q_DECLARE_FLAGS(DialogParameterFlags, DialogParameterEnum)

  explicit WizardDialogParameters(Utils::FilePath default_path, const Utils::Id platform, const QSet<Utils::Id> &required_features, const DialogParameterFlags flags, QVariantMap extra_values) : m_default_path(std::move(default_path)), m_selected_platform(platform), m_required_features(required_features), m_parameter_flags(flags), m_extra_values(std::move(extra_values)) {}

  auto defaultPath() const -> Utils::FilePath { return m_default_path; }
  auto selectedPlatform() const -> Utils::Id { return m_selected_platform; }
  auto requiredFeatures() const -> QSet<Utils::Id> { return m_required_features; }
  auto flags() const -> DialogParameterFlags { return m_parameter_flags; }
  auto extraValues() const -> QVariantMap { return m_extra_values; }

private:
  Utils::FilePath m_default_path;
  Utils::Id m_selected_platform;
  QSet<Utils::Id> m_required_features;
  DialogParameterFlags m_parameter_flags;
  QVariantMap m_extra_values;
};

class CORE_EXPORT BaseFileWizardFactory : public IWizardFactory {
  Q_OBJECT
  friend class BaseFileWizard;

public:
  static auto buildFileName(const Utils::FilePath &path, const QString &base_name, const QString &extension) -> Utils::FilePath;

protected:
  virtual auto create(QWidget *parent, const WizardDialogParameters &parameters) const -> BaseFileWizard* = 0;
  virtual auto generateFiles(const QWizard *w, QString *error_message) const -> GeneratedFiles = 0;
  virtual auto writeFiles(const GeneratedFiles &files, QString *error_message) const -> bool;
  virtual auto postGenerateFiles(const QWizard *w, const GeneratedFiles &l, QString *error_message) const -> bool;
  static auto preferredSuffix(const QString &mime_type) -> QString;

  enum OverwriteResult {
    OverwriteOk,
    OverwriteError,
    OverwriteCanceled
  };

  static auto promptOverwrite(GeneratedFiles *files, QString *error_message) -> OverwriteResult;
  static auto postGenerateOpenEditors(const GeneratedFiles &l, QString *error_message = nullptr) -> bool;

private:
  auto runWizardImpl(const Utils::FilePath &path, QWidget *parent, Utils::Id platform, const QVariantMap &extra_values, bool show_wizard = true) -> Utils::Wizard* final;
};

} // namespace Orca::Plugin::Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Orca::Plugin::Core::GeneratedFile::Attributes)
Q_DECLARE_OPERATORS_FOR_FLAGS(Orca::Plugin::Core::WizardDialogParameters::DialogParameterFlags)
