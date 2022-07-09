// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "baseeditordocumentparser.hpp"
#include "baseeditordocumentprocessor.hpp"

#include "cppmodelmanager.hpp"
#include "cppprojectpartchooser.hpp"
#include "editordocumenthandle.hpp"

namespace CppEditor {

/*!
    \class CppEditor::BaseEditorDocumentParser

    \brief The BaseEditorDocumentParser class parses a source text as
           precisely as possible.

    It's meant to be used in the C++ editor to get precise results by using
    the "best" project part for a file.

    Derived classes are expected to implement updateImpl() this way:

    \list
        \li Get a copy of the configuration and the last state.
        \li Work on the data and do whatever is necessary. At least, update
            the project part with the help of determineProjectPart().
        \li Ensure the new state is set before updateImpl() returns.
    \endlist
*/

BaseEditorDocumentParser::BaseEditorDocumentParser(const QString &filePath) : m_filePath(filePath)
{
  static auto meta = qRegisterMetaType<ProjectPartInfo>("ProjectPartInfo");
  Q_UNUSED(meta)
}

BaseEditorDocumentParser::~BaseEditorDocumentParser() = default;

auto BaseEditorDocumentParser::filePath() const -> QString
{
  return m_filePath;
}

auto BaseEditorDocumentParser::configuration() const -> BaseEditorDocumentParser::Configuration
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  return m_configuration;
}

auto BaseEditorDocumentParser::setConfiguration(const Configuration &configuration) -> void
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  m_configuration = configuration;
}

auto BaseEditorDocumentParser::update(const UpdateParams &updateParams) -> void
{
  QFutureInterface<void> dummy;
  update(dummy, updateParams);
}

auto BaseEditorDocumentParser::update(const QFutureInterface<void> &future, const UpdateParams &updateParams) -> void
{
  QMutexLocker locker(&m_updateIsRunning);
  updateImpl(future, updateParams);
}

auto BaseEditorDocumentParser::state() const -> BaseEditorDocumentParser::State
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  return m_state;
}

auto BaseEditorDocumentParser::setState(const State &state) -> void
{
  QMutexLocker locker(&m_stateAndConfigurationMutex);
  m_state = state;
}

auto BaseEditorDocumentParser::projectPartInfo() const -> ProjectPartInfo
{
  return state().projectPartInfo;
}

auto BaseEditorDocumentParser::get(const QString &filePath) -> BaseEditorDocumentParser::Ptr
{
  auto cmmi = CppModelManager::instance();
  if (auto cppEditorDocument = cmmi->cppEditorDocument(filePath)) {
    if (auto processor = cppEditorDocument->processor())
      return processor->parser();
  }
  return BaseEditorDocumentParser::Ptr();
}

auto BaseEditorDocumentParser::determineProjectPart(const QString &filePath, const QString &preferredProjectPartId, const ProjectPartInfo &currentProjectPartInfo, const Utils::FilePath &activeProject, Utils::Language languagePreference, bool projectsUpdated) -> ProjectPartInfo
{
  Internal::ProjectPartChooser chooser;
  chooser.setFallbackProjectPart([]() {
    return CppModelManager::instance()->fallbackProjectPart();
  });
  chooser.setProjectPartsForFile([](const QString &filePath) {
    return CppModelManager::instance()->projectPart(filePath);
  });
  chooser.setProjectPartsFromDependenciesForFile([&](const QString &filePath) {
    const auto fileName = Utils::FilePath::fromString(filePath);
    return CppModelManager::instance()->projectPartFromDependencies(fileName);
  });

  const auto chooserResult = chooser.choose(filePath, currentProjectPartInfo, preferredProjectPartId, activeProject, languagePreference, projectsUpdated);

  return chooserResult;
}

} // namespace CppEditor
