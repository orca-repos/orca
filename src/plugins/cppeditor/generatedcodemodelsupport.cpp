// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "generatedcodemodelsupport.hpp"
#include "cppmodelmanager.hpp"

#include <core/core-editor-manager.hpp>
#include <core/core-editor-interface.hpp>
#include <core/core-document-interface.hpp>
#include <projectexplorer/buildconfiguration.hpp>
#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>
#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace CPlusPlus;

namespace CppEditor {

class QObjectCache {
public:
  auto contains(QObject *object) const -> bool
  {
    return m_cache.contains(object);
  }

  auto insert(QObject *object) -> void
  {
    QObject::connect(object, &QObject::destroyed, [this](QObject *dead) {
      m_cache.remove(dead);
    });
    m_cache.insert(object);
  }

private:
  QSet<QObject*> m_cache;
};

GeneratedCodeModelSupport::GeneratedCodeModelSupport(CppModelManager *modelmanager, ProjectExplorer::ExtraCompiler *generator, const Utils::FilePath &generatedFile) : AbstractEditorSupport(modelmanager, generator), m_generatedFileName(generatedFile), m_generator(generator)
{
  QLoggingCategory log("qtc.cppeditor.generatedcodemodelsupport", QtWarningMsg);
  qCDebug(log) << "ctor GeneratedCodeModelSupport for" << m_generator->source() << generatedFile;

  connect(m_generator, &ProjectExplorer::ExtraCompiler::contentsChanged, this, &GeneratedCodeModelSupport::onContentsChanged, Qt::QueuedConnection);
  onContentsChanged(generatedFile);
}

GeneratedCodeModelSupport::~GeneratedCodeModelSupport()
{
  CppModelManager::instance()->emitAbstractEditorSupportRemoved(m_generatedFileName.toString());
  QLoggingCategory log("qtc.cppeditor.generatedcodemodelsupport", QtWarningMsg);
  qCDebug(log) << "dtor ~generatedcodemodelsupport for" << m_generatedFileName;
}

auto GeneratedCodeModelSupport::onContentsChanged(const Utils::FilePath &file) -> void
{
  if (file == m_generatedFileName) {
    notifyAboutUpdatedContents();
    updateDocument();
  }
}

auto GeneratedCodeModelSupport::contents() const -> QByteArray
{
  return m_generator->content(m_generatedFileName);
}

auto GeneratedCodeModelSupport::fileName() const -> QString
{
  return m_generatedFileName.toString();
}

auto GeneratedCodeModelSupport::sourceFileName() const -> QString
{
  return m_generator->source().toString();
}

auto GeneratedCodeModelSupport::update(const QList<ProjectExplorer::ExtraCompiler*> &generators) -> void
{
  static QObjectCache extraCompilerCache;

  const auto mm = CppModelManager::instance();

  foreach(ExtraCompiler *generator, generators) {
    if (extraCompilerCache.contains(generator))
      continue;

    extraCompilerCache.insert(generator);
    generator->forEachTarget([mm, generator](const Utils::FilePath &generatedFile) {
      new GeneratedCodeModelSupport(mm, generator, generatedFile);
    });
  }
}

} // namespace CppEditor
