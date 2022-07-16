// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include <core/core-base-file-wizard-factory.hpp>

#include <QSharedPointer>
#include <QList>
#include <QMap>

QT_BEGIN_NAMESPACE
class QDir;
QT_END_NAMESPACE

namespace Utils {
class Wizard;
}

namespace ProjectExplorer {
class CustomWizard;
class BaseProjectWizardDialog;

namespace Internal {
class CustomWizardPrivate;
class CustomWizardContext;
class CustomWizardParameters;
}

class PROJECTEXPLORER_EXPORT ICustomWizardMetaFactory : public QObject {
  Q_OBJECT

public:
  ICustomWizardMetaFactory(const QString &klass, Orca::Plugin::Core::IWizardFactory::WizardKind kind);
  ~ICustomWizardMetaFactory() override;

  virtual auto create() const -> CustomWizard* = 0;
  auto klass() const -> QString { return m_klass; }
  auto kind() const -> int { return m_kind; }

private:
  QString m_klass;
  Orca::Plugin::Core::IWizardFactory::WizardKind m_kind;
};

// Convenience template to create wizard factory classes.
template <class Wizard>
class CustomWizardMetaFactory : public ICustomWizardMetaFactory {
public:
  CustomWizardMetaFactory(const QString &klass, Orca::Plugin::Core::IWizardFactory::WizardKind kind) : ICustomWizardMetaFactory(klass, kind) { }
  CustomWizardMetaFactory(Orca::Plugin::Core::IWizardFactory::WizardKind kind) : ICustomWizardMetaFactory(QString(), kind) { }

  auto create() const -> CustomWizard* override { return new Wizard; }
};

// Documentation inside.
class PROJECTEXPLORER_EXPORT CustomWizard : public Orca::Plugin::Core::BaseFileWizardFactory {
  Q_OBJECT

public:
  using FieldReplacementMap = QMap<QString, QString>;

  CustomWizard();
  ~CustomWizard() override;

  // Can be reimplemented to create custom wizards. initWizardDialog() needs to be
  // called.
  auto create(QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) const -> Orca::Plugin::Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *w, QString *errorMessage) const -> Orca::Plugin::Core::GeneratedFiles override;

  // Create all wizards. As other plugins might register factories for derived
  // classes, call it in extensionsInitialized().
  static auto createWizards() -> QList<IWizardFactory*>;
  static auto setVerbose(int) -> void;
  static auto verbose() -> int;

protected:
  using CustomWizardParametersPtr = QSharedPointer<Internal::CustomWizardParameters>;
  using CustomWizardContextPtr = QSharedPointer<Internal::CustomWizardContext>;

  // generate files in path
  auto generateWizardFiles(QString *errorMessage) const -> Orca::Plugin::Core::GeneratedFiles;
  // Create replacement map as static base fields + QWizard fields
  auto replacementMap(const QWizard *w) const -> FieldReplacementMap;
  auto writeFiles(const Orca::Plugin::Core::GeneratedFiles &files, QString *errorMessage) const -> bool override;
  auto parameters() const -> CustomWizardParametersPtr;
  auto context() const -> CustomWizardContextPtr;
  static auto createWizard(const CustomWizardParametersPtr &p) -> CustomWizard*;

private:
  auto setParameters(const CustomWizardParametersPtr &p) -> void;

  Internal::CustomWizardPrivate *d;
};

// Documentation inside.
class PROJECTEXPLORER_EXPORT CustomProjectWizard : public CustomWizard {
  Q_OBJECT

public:
  CustomProjectWizard();

  static auto postGenerateOpen(const Orca::Plugin::Core::GeneratedFiles &l, QString *errorMessage = nullptr) -> bool;

signals:
  auto projectLocationChanged(const QString &path) -> void;

protected:
  auto create(QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) const -> Orca::Plugin::Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *w, QString *errorMessage) const -> Orca::Plugin::Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *w, const Orca::Plugin::Core::GeneratedFiles &l, QString *errorMessage) const -> bool override;
  auto initProjectWizardDialog(BaseProjectWizardDialog *w, const Utils::FilePath &defaultPath, const QList<QWizardPage*> &extensionPages) const -> void;

private:
  auto projectParametersChanged(const QString &project, const QString &path) -> void;
};

} // namespace ProjectExplorer
