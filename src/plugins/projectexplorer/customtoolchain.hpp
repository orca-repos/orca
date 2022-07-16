// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "abi.hpp"
#include "customparser.hpp"
#include "headerpath.hpp"
#include "toolchain.hpp"
#include "toolchainconfigwidget.hpp"

#include <utils/fileutils.hpp>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QTextEdit;
class QComboBox;
QT_END_NAMESPACE

namespace Utils {
class PathChooser;
}

namespace ProjectExplorer {

class AbiWidget;

namespace Internal {
class CustomToolChainFactory;
}

// --------------------------------------------------------------------------
// CustomToolChain
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT CustomToolChain : public ToolChain {
  Q_DECLARE_TR_FUNCTIONS(CustomToolChain)
public:
  class Parser {
  public:
    Utils::Id parserId;  ///< A unique id identifying a parser
    QString displayName; ///< A translateable name to Show in the user interface
  };

  auto isValid() const -> bool override;

  auto createMacroInspectionRunner() const -> MacroInspectionRunner override;
  auto languageExtensions(const QStringList &cxxflags) const -> Utils::LanguageExtensions override;
  auto warningFlags(const QStringList &cxxflags) const -> Utils::WarningFlags override;
  auto rawPredefinedMacros() const -> const Macros&;
  auto setPredefinedMacros(const Macros &macros) -> void;
  auto createBuiltInHeaderPathsRunner(const Utils::Environment &) const -> BuiltInHeaderPathsRunner override;
  auto addToEnvironment(Utils::Environment &env) const -> void override;
  auto suggestedMkspecList() const -> QStringList override;
  auto createOutputParsers() const -> QList<Utils::OutputLineParser*> override;
  auto headerPathsList() const -> QStringList;
  auto setHeaderPaths(const QStringList &list) -> void;
  auto toMap() const -> QVariantMap override;
  auto fromMap(const QVariantMap &data) -> bool override;
  auto createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget> override;
  auto operator ==(const ToolChain &) const -> bool override;
  auto setMakeCommand(const Utils::FilePath &) -> void;
  auto makeCommand(const Utils::Environment &environment) const -> Utils::FilePath override;
  auto setCxx11Flags(const QStringList &) -> void;
  auto cxx11Flags() const -> const QStringList&;
  auto setMkspecs(const QString &) -> void;
  auto mkspecs() const -> QString;
  auto outputParserId() const -> Utils::Id;
  auto setOutputParserId(Utils::Id parserId) -> void;
  static auto parsers() -> QList<Parser>;

private:
  CustomToolChain();

  auto customParserSettings() const -> CustomParserSettings;

  Utils::FilePath m_compilerCommand;
  Utils::FilePath m_makeCommand;
  Macros m_predefinedMacros;
  HeaderPaths m_builtInHeaderPaths;
  QStringList m_cxx11Flags;
  QStringList m_mkspecs;
  Utils::Id m_outputParserId;

  friend class Internal::CustomToolChainFactory;
  friend class ToolChainFactory;
};

namespace Internal {

class CustomToolChainFactory : public ToolChainFactory {
public:
  CustomToolChainFactory();
};

// --------------------------------------------------------------------------
// CustomToolChainConfigWidget
// --------------------------------------------------------------------------

class TextEditDetailsWidget;

class CustomToolChainConfigWidget : public ToolChainConfigWidget {
  Q_OBJECT public:
  CustomToolChainConfigWidget(CustomToolChain *);

private:
  auto updateSummaries() -> void;
  auto errorParserChanged(int index = -1) -> void;

protected:
  auto applyImpl() -> void override;
  auto discardImpl() -> void override { setFromToolchain(); }
  auto isDirtyImpl() const -> bool override;
  auto makeReadOnlyImpl() -> void override;
  auto setFromToolchain() -> void;

  Utils::PathChooser *m_compilerCommand;
  Utils::PathChooser *m_makeCommand;
  AbiWidget *m_abiWidget;
  QPlainTextEdit *m_predefinedMacros;
  QPlainTextEdit *m_headerPaths;
  TextEditDetailsWidget *m_predefinedDetails;
  TextEditDetailsWidget *m_headerDetails;
  QLineEdit *m_cxx11Flags;
  QLineEdit *m_mkspecs;
  QComboBox *m_errorParserComboBox;
};

} // namespace Internal
} // namespace ProjectExplorer
