// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "abstracteditorsupport.hpp"

#include "cppeditorplugin.hpp"
#include "cppfilesettingspage.hpp"
#include "cppmodelmanager.hpp"

#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>
#include <utils/templateengine.hpp>

namespace CppEditor {

AbstractEditorSupport::AbstractEditorSupport(CppModelManager *modelmanager, QObject *parent) : QObject(parent), m_modelmanager(modelmanager), m_revision(1)
{
  modelmanager->addExtraEditorSupport(this);
}

AbstractEditorSupport::~AbstractEditorSupport()
{
  m_modelmanager->removeExtraEditorSupport(this);
}

auto AbstractEditorSupport::updateDocument() -> void
{
  ++m_revision;
  m_modelmanager->updateSourceFiles(QSet<QString>() << fileName());
}

auto AbstractEditorSupport::notifyAboutUpdatedContents() const -> void
{
  m_modelmanager->emitAbstractEditorSupportContentsUpdated(fileName(), sourceFileName(), contents());
}

auto AbstractEditorSupport::licenseTemplate(const QString &file, const QString &className) -> QString
{
  const auto license = Internal::CppFileSettings::licenseTemplate();
  Utils::MacroExpander expander;
  expander.registerVariable("Cpp:License:FileName", tr("The file name."), [file]() { return Utils::FilePath::fromString(file).fileName(); });
  expander.registerVariable("Cpp:License:ClassName", tr("The class name."), [className]() { return className; });

  return Utils::TemplateEngine::processText(&expander, license, nullptr);
}

auto AbstractEditorSupport::usePragmaOnce() -> bool
{
  return Internal::CppEditorPlugin::usePragmaOnce();
}

} // namespace CppEditor

