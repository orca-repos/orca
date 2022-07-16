// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "abstracteditorsupport.hpp"

#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/extracompiler.hpp>

#include <QDateTime>
#include <QHash>
#include <QSet>

namespace Orca::Plugin::Core {
class IEditor;
}

namespace ProjectExplorer {
class Project;
}

namespace CppEditor {

class CPPEDITOR_EXPORT GeneratedCodeModelSupport : public AbstractEditorSupport {
  Q_OBJECT

public:
  GeneratedCodeModelSupport(CppModelManager *modelmanager, ProjectExplorer::ExtraCompiler *generator, const Utils::FilePath &generatedFile);
  ~GeneratedCodeModelSupport() override;

  /// \returns the contents encoded in UTF-8.
  auto contents() const -> QByteArray override;
  auto fileName() const -> QString override; // The generated file
  auto sourceFileName() const -> QString override;

  static auto update(const QList<ProjectExplorer::ExtraCompiler*> &generators) -> void;

private:
  auto onContentsChanged(const Utils::FilePath &file) -> void;
  Utils::FilePath m_generatedFileName;
  ProjectExplorer::ExtraCompiler *m_generator;
};

} // CppEditor
