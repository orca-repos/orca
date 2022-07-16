// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customtoolchain.hpp"
#include "abiwidget.hpp"
#include "gccparser.hpp"
#include "clangparser.hpp"
#include "linuxiccparser.hpp"
#include "msvcparser.hpp"
#include "customparser.hpp"
#include "projectexplorer.hpp"
#include "projectexplorerconstants.hpp"
#include "projectmacro.hpp"
#include "toolchainmanager.hpp"

#include <utils/algorithm.hpp>
#include <utils/detailswidget.hpp>
#include <utils/environment.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QUuid>

using namespace Utils;

namespace ProjectExplorer {

// --------------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------------

static constexpr char makeCommandKeyC[] = "ProjectExplorer.CustomToolChain.MakePath";
static constexpr char predefinedMacrosKeyC[] = "ProjectExplorer.CustomToolChain.PredefinedMacros";
static constexpr char headerPathsKeyC[] = "ProjectExplorer.CustomToolChain.HeaderPaths";
static constexpr char cxx11FlagsKeyC[] = "ProjectExplorer.CustomToolChain.Cxx11Flags";
static constexpr char mkspecsKeyC[] = "ProjectExplorer.CustomToolChain.Mkspecs";
static constexpr char outputParserKeyC[] = "ProjectExplorer.CustomToolChain.OutputParser";

// --------------------------------------------------------------------------
// CustomToolChain
// --------------------------------------------------------------------------

CustomToolChain::CustomToolChain() : ToolChain(Constants::CUSTOM_TOOLCHAIN_TYPEID), m_outputParserId(GccParser::id())
{
  setTypeDisplayName(tr("Custom"));
  setTargetAbiKey("ProjectExplorer.CustomToolChain.TargetAbi");
  setCompilerCommandKey("ProjectExplorer.CustomToolChain.CompilerPath");
}

auto CustomToolChain::customParserSettings() const -> CustomParserSettings
{
  return findOrDefault(ProjectExplorerPlugin::customParsers(), [this](const CustomParserSettings &s) {
    return s.id == outputParserId();
  });
}

auto CustomToolChain::isValid() const -> bool
{
  return true;
}

auto CustomToolChain::createMacroInspectionRunner() const -> MacroInspectionRunner
{
  const auto theMacros = m_predefinedMacros;
  const auto lang = language();

  // This runner must be thread-safe!
  return [theMacros, lang](const QStringList &cxxflags) {
    auto macros = theMacros;
    for (const auto &cxxFlag : cxxflags) {
      if (cxxFlag.startsWith(QLatin1String("-D")))
        macros.append(Macro::fromKeyValue(cxxFlag.mid(2).trimmed()));
      else if (cxxFlag.startsWith(QLatin1String("-U")) && !cxxFlag.contains('='))
        macros.append({cxxFlag.mid(2).trimmed().toUtf8(), MacroType::Undefine});

    }
    return MacroInspectionReport{macros, languageVersion(lang, macros)};
  };
}

auto CustomToolChain::languageExtensions(const QStringList &) const -> LanguageExtensions
{
  return LanguageExtension::None;
}

auto CustomToolChain::warningFlags(const QStringList &cxxflags) const -> WarningFlags
{
  Q_UNUSED(cxxflags)
  return WarningFlags::Default;
}

auto CustomToolChain::rawPredefinedMacros() const -> const Macros&
{
  return m_predefinedMacros;
}

auto CustomToolChain::setPredefinedMacros(const Macros &macros) -> void
{
  if (m_predefinedMacros == macros)
    return;
  m_predefinedMacros = macros;
  toolChainUpdated();
}

auto CustomToolChain::createBuiltInHeaderPathsRunner(const Environment &) const -> BuiltInHeaderPathsRunner
{
  const auto builtInHeaderPaths = m_builtInHeaderPaths;

  // This runner must be thread-safe!
  return [builtInHeaderPaths](const QStringList &cxxFlags, const QString &, const QString &) {
    HeaderPaths flagHeaderPaths;
    for (const auto &cxxFlag : cxxFlags) {
      if (cxxFlag.startsWith(QLatin1String("-I")))
        flagHeaderPaths.push_back(HeaderPath::makeBuiltIn(cxxFlag.mid(2).trimmed()));
    }

    return builtInHeaderPaths + flagHeaderPaths;
  };
}

auto CustomToolChain::addToEnvironment(Environment &env) const -> void
{
  if (!m_compilerCommand.isEmpty()) {
    const auto path = m_compilerCommand.parentDir();
    env.prependOrSetPath(path);
    const auto makePath = m_makeCommand.parentDir();
    if (makePath != path)
      env.prependOrSetPath(makePath);
  }
}

auto CustomToolChain::suggestedMkspecList() const -> QStringList
{
  return m_mkspecs;
}

auto CustomToolChain::createOutputParsers() const -> QList<OutputLineParser*>
{
  if (m_outputParserId == GccParser::id())
    return GccParser::gccParserSuite();
  if (m_outputParserId == ClangParser::id())
    return ClangParser::clangParserSuite();
  if (m_outputParserId == LinuxIccParser::id())
    return LinuxIccParser::iccParserSuite();
  if (m_outputParserId == MsvcParser::id())
    return {new MsvcParser};
  return {new Internal::CustomParser(customParserSettings())};
  return {};
}

auto CustomToolChain::headerPathsList() const -> QStringList
{
  return Utils::transform<QList>(m_builtInHeaderPaths, &HeaderPath::path);
}

auto CustomToolChain::setHeaderPaths(const QStringList &list) -> void
{
  const auto tmp = Utils::transform<QVector>(list, [](const QString &headerPath) {
    return HeaderPath::makeBuiltIn(headerPath.trimmed());
  });

  if (m_builtInHeaderPaths == tmp)
    return;
  m_builtInHeaderPaths = tmp;
  toolChainUpdated();
}

auto CustomToolChain::setMakeCommand(const FilePath &path) -> void
{
  if (path == m_makeCommand)
    return;
  m_makeCommand = path;
  toolChainUpdated();
}

auto CustomToolChain::makeCommand(const Environment &) const -> FilePath
{
  return m_makeCommand;
}

auto CustomToolChain::setCxx11Flags(const QStringList &flags) -> void
{
  if (flags == m_cxx11Flags)
    return;
  m_cxx11Flags = flags;
  toolChainUpdated();
}

auto CustomToolChain::cxx11Flags() const -> const QStringList&
{
  return m_cxx11Flags;
}

auto CustomToolChain::setMkspecs(const QString &specs) -> void
{
  const auto tmp = specs.split(',');
  if (tmp == m_mkspecs)
    return;
  m_mkspecs = tmp;
  toolChainUpdated();
}

auto CustomToolChain::mkspecs() const -> QString
{
  return m_mkspecs.join(',');
}

auto CustomToolChain::toMap() const -> QVariantMap
{
  auto data = ToolChain::toMap();
  data.insert(QLatin1String(makeCommandKeyC), m_makeCommand.toString());
  const auto macros = Utils::transform<QList>(m_predefinedMacros, [](const Macro &m) { return QString::fromUtf8(m.toByteArray()); });
  data.insert(QLatin1String(predefinedMacrosKeyC), macros);
  data.insert(QLatin1String(headerPathsKeyC), headerPathsList());
  data.insert(QLatin1String(cxx11FlagsKeyC), m_cxx11Flags);
  data.insert(QLatin1String(mkspecsKeyC), mkspecs());
  data.insert(QLatin1String(outputParserKeyC), m_outputParserId.toSetting());

  return data;
}

auto CustomToolChain::fromMap(const QVariantMap &data) -> bool
{
  if (!ToolChain::fromMap(data))
    return false;

  m_makeCommand = FilePath::fromString(data.value(QLatin1String(makeCommandKeyC)).toString());
  const auto macros = data.value(QLatin1String(predefinedMacrosKeyC)).toStringList();
  m_predefinedMacros = Macro::toMacros(macros.join('\n').toUtf8());
  setHeaderPaths(data.value(QLatin1String(headerPathsKeyC)).toStringList());
  m_cxx11Flags = data.value(QLatin1String(cxx11FlagsKeyC)).toStringList();
  setMkspecs(data.value(QLatin1String(mkspecsKeyC)).toString());
  setOutputParserId(Id::fromSetting(data.value(QLatin1String(outputParserKeyC))));

  // Restore Pre-4.13 settings.
  if (outputParserId() == Internal::CustomParser::id()) {
    CustomParserSettings customParserSettings;
    customParserSettings.error.setPattern(data.value("ProjectExplorer.CustomToolChain.ErrorPattern").toString());
    customParserSettings.error.setFileNameCap(data.value("ProjectExplorer.CustomToolChain.ErrorLineNumberCap").toInt());
    customParserSettings.error.setLineNumberCap(data.value("ProjectExplorer.CustomToolChain.ErrorFileNameCap").toInt());
    customParserSettings.error.setMessageCap(data.value("ProjectExplorer.CustomToolChain.ErrorMessageCap").toInt());
    customParserSettings.error.setChannel(static_cast<CustomParserExpression::CustomParserChannel>(data.value("ProjectExplorer.CustomToolChain.ErrorChannel").toInt()));
    customParserSettings.error.setExample(data.value("ProjectExplorer.CustomToolChain.ErrorExample").toString());
    customParserSettings.warning.setPattern(data.value("ProjectExplorer.CustomToolChain.WarningPattern").toString());
    customParserSettings.warning.setFileNameCap(data.value("ProjectExplorer.CustomToolChain.WarningLineNumberCap").toInt());
    customParserSettings.warning.setLineNumberCap(data.value("ProjectExplorer.CustomToolChain.WarningFileNameCap").toInt());
    customParserSettings.warning.setMessageCap(data.value("ProjectExplorer.CustomToolChain.WarningMessageCap").toInt());
    customParserSettings.warning.setChannel(static_cast<CustomParserExpression::CustomParserChannel>(data.value("ProjectExplorer.CustomToolChain.WarningChannel").toInt()));
    customParserSettings.warning.setExample(data.value("ProjectExplorer.CustomToolChain.WarningExample").toString());
    if (!customParserSettings.error.pattern().isEmpty() || !customParserSettings.error.pattern().isEmpty()) {
      // Found custom parser in old settings, move to new place.
      customParserSettings.id = Id::fromString(QUuid::createUuid().toString());
      setOutputParserId(customParserSettings.id);
      customParserSettings.displayName = tr("Parser for toolchain %1").arg(displayName());
      auto settings = ProjectExplorerPlugin::customParsers();
      settings << customParserSettings;
      ProjectExplorerPlugin::setCustomParsers(settings);
    }
  }

  return true;
}

auto CustomToolChain::operator ==(const ToolChain &other) const -> bool
{
  if (!ToolChain::operator ==(other))
    return false;

  const auto customTc = static_cast<const CustomToolChain*>(&other);
  return m_compilerCommand == customTc->m_compilerCommand && m_makeCommand == customTc->m_makeCommand && targetAbi() == customTc->targetAbi() && m_predefinedMacros == customTc->m_predefinedMacros && m_builtInHeaderPaths == customTc->m_builtInHeaderPaths;
}

auto CustomToolChain::outputParserId() const -> Id
{
  return m_outputParserId;
}

auto CustomToolChain::setOutputParserId(Id parserId) -> void
{
  if (m_outputParserId == parserId)
    return;
  m_outputParserId = parserId;
  toolChainUpdated();
}

auto CustomToolChain::parsers() -> QList<Parser>
{
  QList<Parser> result;
  result.append({GccParser::id(), tr("GCC")});
  result.append({ClangParser::id(), tr("Clang")});
  result.append({LinuxIccParser::id(), tr("ICC")});
  result.append({MsvcParser::id(), tr("MSVC")});
  return result;
}

auto CustomToolChain::createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget>
{
  return std::make_unique<Internal::CustomToolChainConfigWidget>(this);
}

namespace Internal {

// --------------------------------------------------------------------------
// CustomToolChainFactory
// --------------------------------------------------------------------------

CustomToolChainFactory::CustomToolChainFactory()
{
  setDisplayName(CustomToolChain::tr("Custom"));
  setSupportedToolChainType(Constants::CUSTOM_TOOLCHAIN_TYPEID);
  setSupportsAllLanguages(true);
  setToolchainConstructor([] { return new CustomToolChain; });
  setUserCreatable(true);
}

// --------------------------------------------------------------------------
// Helper for ConfigWidget
// --------------------------------------------------------------------------

class TextEditDetailsWidget : public DetailsWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::TextEditDetailsWidget)
public:
  TextEditDetailsWidget(QPlainTextEdit *textEdit)
  {
    setWidget(textEdit);
  }

  inline auto textEditWidget() const -> QPlainTextEdit*
  {
    return static_cast<QPlainTextEdit*>(widget());
  }

  auto entries() const -> QStringList
  {
    return textEditWidget()->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  }

  auto text() const -> QString
  {
    return textEditWidget()->toPlainText();
  }

  // not accurate, counts empty lines (except last)
  auto entryCount() const -> int
  {
    auto count = textEditWidget()->blockCount();
    const auto text = textEditWidget()->toPlainText();
    if (text.isEmpty() || text.endsWith(QLatin1Char('\n')))
      --count;
    return count;
  }

  auto updateSummaryText() -> void
  {
    const auto count = entryCount();
    setSummaryText(count ? tr("%n entries", "", count) : tr("Empty"));
  }
};

// --------------------------------------------------------------------------
// CustomToolChainConfigWidget
// --------------------------------------------------------------------------

CustomToolChainConfigWidget::CustomToolChainConfigWidget(CustomToolChain *tc) : ToolChainConfigWidget(tc), m_compilerCommand(new PathChooser), m_makeCommand(new PathChooser), m_abiWidget(new AbiWidget), m_predefinedMacros(new QPlainTextEdit), m_headerPaths(new QPlainTextEdit), m_predefinedDetails(new TextEditDetailsWidget(m_predefinedMacros)), m_headerDetails(new TextEditDetailsWidget(m_headerPaths)), m_cxx11Flags(new QLineEdit), m_mkspecs(new QLineEdit), m_errorParserComboBox(new QComboBox)
{
  Q_ASSERT(tc);

  const auto parsers = CustomToolChain::parsers();
  for (const auto &parser : parsers)
    m_errorParserComboBox->addItem(parser.displayName, parser.parserId.toString());
  for (const auto &s : ProjectExplorerPlugin::customParsers())
    m_errorParserComboBox->addItem(s.displayName, s.id.toString());

  const auto parserLayoutWidget = new QWidget;
  const auto parserLayout = new QHBoxLayout(parserLayoutWidget);
  parserLayout->setContentsMargins(0, 0, 0, 0);
  m_predefinedMacros->setPlaceholderText(tr("MACRO[=VALUE]"));
  m_predefinedMacros->setTabChangesFocus(true);
  m_predefinedMacros->setToolTip(tr("Each line defines a macro. Format is MACRO[=VALUE]."));
  m_headerPaths->setTabChangesFocus(true);
  m_headerPaths->setToolTip(tr("Each line adds a global header lookup path."));
  m_cxx11Flags->setToolTip(tr("Comma-separated list of flags that turn on C++11 support."));
  m_mkspecs->setToolTip(tr("Comma-separated list of mkspecs."));
  m_compilerCommand->setExpectedKind(PathChooser::ExistingCommand);
  m_compilerCommand->setHistoryCompleter(QLatin1String("PE.ToolChainCommand.History"));
  m_makeCommand->setExpectedKind(PathChooser::ExistingCommand);
  m_makeCommand->setHistoryCompleter(QLatin1String("PE.MakeCommand.History"));
  m_mainLayout->addRow(tr("&Compiler path:"), m_compilerCommand);
  m_mainLayout->addRow(tr("&Make path:"), m_makeCommand);
  m_mainLayout->addRow(tr("&ABI:"), m_abiWidget);
  m_mainLayout->addRow(tr("&Predefined macros:"), m_predefinedDetails);
  m_mainLayout->addRow(tr("&Header paths:"), m_headerDetails);
  m_mainLayout->addRow(tr("C++11 &flags:"), m_cxx11Flags);
  m_mainLayout->addRow(tr("&Qt mkspecs:"), m_mkspecs);
  parserLayout->addWidget(m_errorParserComboBox);
  m_mainLayout->addRow(tr("&Error parser:"), parserLayoutWidget);
  addErrorLabel();

  setFromToolchain();
  m_predefinedDetails->updateSummaryText();
  m_headerDetails->updateSummaryText();

  connect(m_compilerCommand, &PathChooser::rawPathChanged, this, &ToolChainConfigWidget::dirty);
  connect(m_makeCommand, &PathChooser::rawPathChanged, this, &ToolChainConfigWidget::dirty);
  connect(m_abiWidget, &AbiWidget::abiChanged, this, &ToolChainConfigWidget::dirty);
  connect(m_predefinedMacros, &QPlainTextEdit::textChanged, this, &CustomToolChainConfigWidget::updateSummaries);
  connect(m_headerPaths, &QPlainTextEdit::textChanged, this, &CustomToolChainConfigWidget::updateSummaries);
  connect(m_cxx11Flags, &QLineEdit::textChanged, this, &ToolChainConfigWidget::dirty);
  connect(m_mkspecs, &QLineEdit::textChanged, this, &ToolChainConfigWidget::dirty);
  connect(m_errorParserComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CustomToolChainConfigWidget::errorParserChanged);
  errorParserChanged();
}

auto CustomToolChainConfigWidget::updateSummaries() -> void
{
  if (sender() == m_predefinedMacros)
    m_predefinedDetails->updateSummaryText();
  else
    m_headerDetails->updateSummaryText();
  emit dirty();
}

auto CustomToolChainConfigWidget::errorParserChanged(int) -> void
{
  emit dirty();
}

auto CustomToolChainConfigWidget::applyImpl() -> void
{
  if (toolChain()->isAutoDetected())
    return;

  const auto tc = static_cast<CustomToolChain*>(toolChain());
  Q_ASSERT(tc);
  const auto displayName = tc->displayName();
  tc->setCompilerCommand(m_compilerCommand->filePath());
  tc->setMakeCommand(m_makeCommand->filePath());
  tc->setTargetAbi(m_abiWidget->currentAbi());
  const auto macros = Utils::transform<QVector>(m_predefinedDetails->text().split('\n', Qt::SkipEmptyParts), [](const QString &m) {
    return Macro::fromKeyValue(m);
  });
  tc->setPredefinedMacros(macros);
  tc->setHeaderPaths(m_headerDetails->entries());
  tc->setCxx11Flags(m_cxx11Flags->text().split(QLatin1Char(',')));
  tc->setMkspecs(m_mkspecs->text());
  tc->setDisplayName(displayName); // reset display name
  tc->setOutputParserId(Id::fromSetting(m_errorParserComboBox->currentData()));

  setFromToolchain(); // Refresh with actual data from the toolchain. This shows what e.g. the
  // macro parser did with the input.
}

auto CustomToolChainConfigWidget::setFromToolchain() -> void
{
  // subwidgets are not yet connected!
  QSignalBlocker blocker(this);
  const auto tc = static_cast<CustomToolChain*>(toolChain());
  m_compilerCommand->setFilePath(tc->compilerCommand());
  m_makeCommand->setFilePath(tc->makeCommand(Environment()));
  m_abiWidget->setAbis(Abis(), tc->targetAbi());
  const auto macroLines = Utils::transform<QList>(tc->rawPredefinedMacros(), [](const Macro &m) {
    return QString::fromUtf8(m.toKeyValue(QByteArray()));
  });
  m_predefinedMacros->setPlainText(macroLines.join('\n'));
  m_headerPaths->setPlainText(tc->headerPathsList().join('\n'));
  m_cxx11Flags->setText(tc->cxx11Flags().join(QLatin1Char(',')));
  m_mkspecs->setText(tc->mkspecs());
  const auto index = m_errorParserComboBox->findData(tc->outputParserId().toSetting());
  m_errorParserComboBox->setCurrentIndex(index);
}

auto CustomToolChainConfigWidget::isDirtyImpl() const -> bool
{
  const auto tc = static_cast<CustomToolChain*>(toolChain());
  Q_ASSERT(tc);
  return m_compilerCommand->filePath() != tc->compilerCommand() || m_makeCommand->filePath().toString() != tc->makeCommand(Environment()).toString() || m_abiWidget->currentAbi() != tc->targetAbi() || Macro::toMacros(m_predefinedDetails->text().toUtf8()) != tc->rawPredefinedMacros() || m_headerDetails->entries() != tc->headerPathsList() || m_cxx11Flags->text().split(QLatin1Char(',')) != tc->cxx11Flags() || m_mkspecs->text() != tc->mkspecs() || Id::fromSetting(m_errorParserComboBox->currentData()) == tc->outputParserId();
}

auto CustomToolChainConfigWidget::makeReadOnlyImpl() -> void
{
  m_mainLayout->setEnabled(false);
}

} // namespace Internal
} // namespace ProjectExplorer
