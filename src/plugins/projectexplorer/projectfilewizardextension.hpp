/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "projectexplorer_export.hpp"

#include <core/core-file-wizard-extension-interface.hpp>

namespace ProjectExplorer {
class FolderNode;
class Node;
class Project;

namespace Internal {

class ProjectWizardContext;

class PROJECTEXPLORER_EXPORT ProjectFileWizardExtension : public Orca::Plugin::Core::IFileWizardExtension {
  Q_OBJECT

public:
  ~ProjectFileWizardExtension() override;

  auto extensionPages(const Orca::Plugin::Core::IWizardFactory *wizard) -> QList<QWizardPage*> override;
  auto processFiles(const QList<Orca::Plugin::Core::GeneratedFile> &files, bool *removeOpenProjectAttribute, QString *errorMessage) -> bool override;
  auto applyCodeStyle(Orca::Plugin::Core::GeneratedFile *file) const -> void override;

public slots:
  void firstExtensionPageShown(const QList<Orca::Plugin::Core::GeneratedFile> &files, const QVariantMap &extraValues) override;

private:
  auto findWizardContextNode(Node *contextNode, Project *project, const Utils::FilePath &path) -> Node*;
  auto processProject(const QList<Orca::Plugin::Core::GeneratedFile> &files, bool *removeOpenProjectAttribute, QString *errorMessage) -> bool;

  ProjectWizardContext *m_context = nullptr;
};

} // namespace Internal
} // namespace ProjectExplorer
