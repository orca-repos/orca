// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-external-tool.hpp"

#include "core-document-interface.hpp"
#include "core-document-manager.hpp"
#include "core-editor-manager.hpp"
#include "core-external-tool-manager.hpp"
#include "core-interface.hpp"
#include "core-message-manager.hpp"

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>
#include <utils/qtcprocess.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QXmlStreamReader>

#include <utility>

using namespace Utils;

namespace Orca::Plugin::Core {

constexpr char kExternalTool[] = "externaltool";
constexpr char kId[] = "id";
constexpr char kDescription[] = "description";
constexpr char kDisplayName[] = "displayname";
constexpr char kCategory[] = "category";
constexpr char kOrder[] = "order";
constexpr char kExecutable[] = "executable";
constexpr char kPath[] = "path";
constexpr char kArguments[] = "arguments";
constexpr char kInput[] = "input";
constexpr char kWorkingDirectory[] = "workingdirectory";
constexpr char kBaseEnvironmentId[] = "baseEnvironmentId";
constexpr char kEnvironment[] = "environment";
constexpr char kXmlLang[] = "xml:lang";
constexpr char kOutput[] = "output";
constexpr char kError[] = "error";
constexpr char kOutputShowInPane[] = "showinpane";
constexpr char kOutputReplaceSelection[] = "replaceselection";
constexpr char kOutputIgnore[] = "ignore";
constexpr char kModifiesDocument[] = "modifiesdocument";
constexpr char kYes[] = "yes";
constexpr char kNo[] = "no";
constexpr char kTrue[] = "true";
constexpr char kFalse[] = "false";

// #pragma mark -- ExternalTool

ExternalTool::ExternalTool() : m_displayCategory("") // difference between isNull and isEmpty
{}

ExternalTool::ExternalTool(const ExternalTool *other) : m_id(other->m_id), m_description(other->m_description), m_displayName(other->m_displayName), m_displayCategory(other->m_displayCategory), m_order(other->m_order), m_executables(other->m_executables), m_arguments(other->m_arguments), m_input(other->m_input), m_workingDirectory(other->m_workingDirectory), m_baseEnvironmentProviderId(other->m_baseEnvironmentProviderId), m_environment(other->m_environment), m_outputHandling(other->m_outputHandling), m_errorHandling(other->m_errorHandling), m_modifiesCurrentDocument(other->m_modifiesCurrentDocument), m_filePath(other->m_filePath), m_presetTool(other->m_presetTool) {}

auto ExternalTool::operator=(const ExternalTool &other) -> ExternalTool&
{
  m_id = other.m_id;
  m_description = other.m_description;
  m_displayName = other.m_displayName;
  m_displayCategory = other.m_displayCategory;
  m_order = other.m_order;
  m_executables = other.m_executables;
  m_arguments = other.m_arguments;
  m_input = other.m_input;
  m_workingDirectory = other.m_workingDirectory;
  m_environment = other.m_environment;
  m_outputHandling = other.m_outputHandling;
  m_errorHandling = other.m_errorHandling;
  m_modifiesCurrentDocument = other.m_modifiesCurrentDocument;
  m_filePath = other.m_filePath;
  m_presetFileName = other.m_presetFileName;
  m_presetTool = other.m_presetTool;
  return *this;
}

ExternalTool::~ExternalTool() = default;

auto ExternalTool::id() const -> QString
{
  return m_id;
}

auto ExternalTool::description() const -> QString
{
  return m_description;
}

auto ExternalTool::displayName() const -> QString
{
  return m_displayName;
}

auto ExternalTool::displayCategory() const -> QString
{
  return m_displayCategory;
}

auto ExternalTool::order() const -> int
{
  return m_order;
}

auto ExternalTool::executables() const -> FilePaths
{
  return m_executables;
}

auto ExternalTool::arguments() const -> QString
{
  return m_arguments;
}

auto ExternalTool::input() const -> QString
{
  return m_input;
}

auto ExternalTool::workingDirectory() const -> FilePath
{
  return m_workingDirectory;
}

auto ExternalTool::baseEnvironmentProviderId() const -> Id
{
  return m_baseEnvironmentProviderId;
}

auto ExternalTool::baseEnvironment() const -> Environment
{
  if (m_baseEnvironmentProviderId.isValid()) {
    const auto provider = EnvironmentProvider::provider(m_baseEnvironmentProviderId.name());
    if (provider && provider->environment)
      return provider->environment();
  }
  return Environment::systemEnvironment();
}

auto ExternalTool::environmentUserChanges() const -> EnvironmentItems
{
  return m_environment;
}

auto ExternalTool::outputHandling() const -> ExternalTool::OutputHandling
{
  return m_outputHandling;
}

auto ExternalTool::errorHandling() const -> ExternalTool::OutputHandling
{
  return m_errorHandling;
}

auto ExternalTool::modifiesCurrentDocument() const -> bool
{
  return m_modifiesCurrentDocument;
}

auto ExternalTool::setFileName(const Utils::FilePath &fileName) -> void
{
  m_filePath = fileName;
}

auto ExternalTool::setPreset(QSharedPointer<ExternalTool> preset) -> void
{
  m_presetTool = std::move(preset);
}

auto ExternalTool::fileName() const -> Utils::FilePath
{
  return m_filePath;
}

auto ExternalTool::preset() const -> QSharedPointer<ExternalTool>
{
  return m_presetTool;
}

auto ExternalTool::setId(const QString &id) -> void
{
  m_id = id;
}

auto ExternalTool::setDisplayCategory(const QString &category) -> void
{
  m_displayCategory = category;
}

auto ExternalTool::setDisplayName(const QString &name) -> void
{
  m_displayName = name;
}

auto ExternalTool::setDescription(const QString &description) -> void
{
  m_description = description;
}

auto ExternalTool::setOutputHandling(OutputHandling handling) -> void
{
  m_outputHandling = handling;
}

auto ExternalTool::setErrorHandling(OutputHandling handling) -> void
{
  m_errorHandling = handling;
}

auto ExternalTool::setModifiesCurrentDocument(bool modifies) -> void
{
  m_modifiesCurrentDocument = modifies;
}

auto ExternalTool::setExecutables(const FilePaths &executables) -> void
{
  m_executables = executables;
}

auto ExternalTool::setArguments(const QString &arguments) -> void
{
  m_arguments = arguments;
}

auto ExternalTool::setInput(const QString &input) -> void
{
  m_input = input;
}

auto ExternalTool::setWorkingDirectory(const FilePath &workingDirectory) -> void
{
  m_workingDirectory = workingDirectory;
}

auto ExternalTool::setBaseEnvironmentProviderId(Id id) -> void
{
  m_baseEnvironmentProviderId = id;
}

auto ExternalTool::setEnvironmentUserChanges(const EnvironmentItems &items) -> void
{
  m_environment = items;
}

static auto splitLocale(const QString &locale) -> QStringList
{
  auto value = locale;
  QStringList values;
  if (!value.isEmpty())
    values << value;
  int index = value.indexOf(QLatin1Char('.'));
  if (index >= 0) {
    value = value.left(index);
    if (!value.isEmpty())
      values << value;
  }
  index = value.indexOf(QLatin1Char('_'));
  if (index >= 0) {
    value = value.left(index);
    if (!value.isEmpty())
      values << value;
  }
  return values;
}

static auto localizedText(const QStringList &locales, QXmlStreamReader *reader, int *currentLocale, QString *currentText) -> void
{
  Q_ASSERT(reader);
  Q_ASSERT(currentLocale);
  Q_ASSERT(currentText);
  if (reader->attributes().hasAttribute(kXmlLang)) {
    int index = locales.indexOf(reader->attributes().value(kXmlLang).toString());
    if (index >= 0 && (index < *currentLocale || *currentLocale < 0)) {
      *currentText = reader->readElementText();
      *currentLocale = index;
    } else {
      reader->skipCurrentElement();
    }
  } else {
    if (*currentLocale < 0 && currentText->isEmpty()) {
      *currentText = QCoreApplication::translate("Core::ExternalTool", reader->readElementText().toUtf8().constData(), "");
    } else {
      reader->skipCurrentElement();
    }
  }
  if (currentText->isNull()) // prefer isEmpty over isNull
    *currentText = "";
}

static auto parseOutputAttribute(const QString &attribute, QXmlStreamReader *reader, ExternalTool::OutputHandling *value) -> bool
{
  const auto output = reader->attributes().value(attribute);
  if (output == QLatin1String(kOutputShowInPane)) {
    *value = ExternalTool::ShowInPane;
  } else if (output == QLatin1String(kOutputReplaceSelection)) {
    *value = ExternalTool::ReplaceSelection;
  } else if (output == QLatin1String(kOutputIgnore)) {
    *value = ExternalTool::Ignore;
  } else {
    reader->raiseError("Allowed values for output attribute are 'showinpane','replaceselection','ignore'");
    return false;
  }
  return true;
}

auto ExternalTool::createFromXml(const QByteArray &xml, QString *errorMessage, const QString &locale) -> ExternalTool*
{
  auto descriptionLocale = -1;
  auto nameLocale = -1;
  auto categoryLocale = -1;
  const auto &locales = splitLocale(locale);
  auto tool = new ExternalTool;
  QXmlStreamReader reader(xml);

  if (!reader.readNextStartElement() || reader.name() != QLatin1String(kExternalTool))
    reader.raiseError("Missing start element <externaltool>");
  tool->m_id = reader.attributes().value(kId).toString();
  if (tool->m_id.isEmpty())
    reader.raiseError("Missing or empty id attribute for <externaltool>");
  while (reader.readNextStartElement()) {
    if (reader.name() == QLatin1String(kDescription)) {
      localizedText(locales, &reader, &descriptionLocale, &tool->m_description);
    } else if (reader.name() == QLatin1String(kDisplayName)) {
      localizedText(locales, &reader, &nameLocale, &tool->m_displayName);
    } else if (reader.name() == QLatin1String(kCategory)) {
      localizedText(locales, &reader, &categoryLocale, &tool->m_displayCategory);
    } else if (reader.name() == QLatin1String(kOrder)) {
      if (tool->m_order >= 0) {
        reader.raiseError("only one <order> element allowed");
        break;
      }
      bool ok;
      tool->m_order = reader.readElementText().toInt(&ok);
      if (!ok || tool->m_order < 0)
        reader.raiseError("<order> element requires non-negative integer value");
    } else if (reader.name() == QLatin1String(kExecutable)) {
      if (reader.attributes().hasAttribute(kOutput)) {
        if (!parseOutputAttribute(kOutput, &reader, &tool->m_outputHandling))
          break;
      }
      if (reader.attributes().hasAttribute(kError)) {
        if (!parseOutputAttribute(kError, &reader, &tool->m_errorHandling))
          break;
      }
      if (reader.attributes().hasAttribute(kModifiesDocument)) {
        const auto value = reader.attributes().value(kModifiesDocument);
        if (value == QLatin1String(kYes) || value == QLatin1String(kTrue)) {
          tool->m_modifiesCurrentDocument = true;
        } else if (value == QLatin1String(kNo) || value == QLatin1String(kFalse)) {
          tool->m_modifiesCurrentDocument = false;
        } else {
          reader.raiseError("Allowed values for modifiesdocument attribute are 'yes','true','no','false'");
          break;
        }
      }
      while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String(kPath)) {
          tool->m_executables.append(FilePath::fromString(reader.readElementText()));
        } else if (reader.name() == QLatin1String(kArguments)) {
          if (!tool->m_arguments.isEmpty()) {
            reader.raiseError("only one <arguments> element allowed");
            break;
          }
          tool->m_arguments = reader.readElementText();
        } else if (reader.name() == QLatin1String(kInput)) {
          if (!tool->m_input.isEmpty()) {
            reader.raiseError("only one <input> element allowed");
            break;
          }
          tool->m_input = reader.readElementText();
        } else if (reader.name() == QLatin1String(kWorkingDirectory)) {
          if (!tool->m_workingDirectory.isEmpty()) {
            reader.raiseError("only one <workingdirectory> element allowed");
            break;
          }
          tool->m_workingDirectory = FilePath::fromString(reader.readElementText());
        } else if (reader.name() == QLatin1String(kBaseEnvironmentId)) {
          if (tool->m_baseEnvironmentProviderId.isValid()) {
            reader.raiseError("only one <baseEnvironmentId> element allowed");
            break;
          }
          tool->m_baseEnvironmentProviderId = Id::fromString(reader.readElementText());
        } else if (reader.name() == QLatin1String(kEnvironment)) {
          if (!tool->m_environment.isEmpty()) {
            reader.raiseError("only one <environment> element allowed");
            break;
          }
          auto lines = reader.readElementText().split(QLatin1Char(';'));
          for (auto &line : lines)
            line = QString::fromUtf8(QByteArray::fromPercentEncoding(line.toUtf8()));
          tool->m_environment = EnvironmentItem::fromStringList(lines);
        } else {
          reader.raiseError(QString::fromLatin1("Unknown element <%1> as subelement of <%2>").arg(reader.qualifiedName().toString(), QString(kExecutable)));
          break;
        }
      }
    } else {
      reader.raiseError(QString::fromLatin1("Unknown element <%1>").arg(reader.qualifiedName().toString()));
    }
  }
  if (reader.hasError()) {
    if (errorMessage)
      *errorMessage = reader.errorString();
    delete tool;
    return nullptr;
  }
  return tool;
}

auto ExternalTool::createFromFile(const Utils::FilePath &fileName, QString *errorMessage, const QString &locale) -> ExternalTool*
{
  auto absFileName = fileName.absoluteFilePath();
  FileReader reader;
  if (!reader.fetch(absFileName, errorMessage))
    return nullptr;
  auto tool = ExternalTool::createFromXml(reader.data(), errorMessage, locale);
  if (!tool)
    return nullptr;
  tool->m_filePath = absFileName;
  return tool;
}

static auto stringForOutputHandling(ExternalTool::OutputHandling handling) -> QString
{
  switch (handling) {
  case ExternalTool::Ignore:
    return QLatin1String(kOutputIgnore);
  case ExternalTool::ShowInPane:
    return QLatin1String(kOutputShowInPane);
  case ExternalTool::ReplaceSelection:
    return QLatin1String(kOutputReplaceSelection);
  }
  return {};
}

auto ExternalTool::save(QString *errorMessage) const -> bool
{
  if (m_filePath.isEmpty())
    return false;
  FileSaver saver(m_filePath);
  if (!saver.hasError()) {
    QXmlStreamWriter out(saver.file());
    out.setAutoFormatting(true);
    out.writeStartDocument("1.0");
    out.writeComment(QString::fromLatin1("Written on %1 by %2").arg(QDateTime::currentDateTime().toString(), ICore::versionString()));
    out.writeStartElement(kExternalTool);
    out.writeAttribute(kId, m_id);
    out.writeTextElement(kDescription, m_description);
    out.writeTextElement(kDisplayName, m_displayName);
    out.writeTextElement(kCategory, m_displayCategory);
    if (m_order != -1)
      out.writeTextElement(kOrder, QString::number(m_order));

    out.writeStartElement(kExecutable);
    out.writeAttribute(kOutput, stringForOutputHandling(m_outputHandling));
    out.writeAttribute(kError, stringForOutputHandling(m_errorHandling));
    out.writeAttribute(kModifiesDocument, QLatin1String(m_modifiesCurrentDocument ? kYes : kNo));
    for (const auto &executable : m_executables)
      out.writeTextElement(kPath, executable.toString());
    if (!m_arguments.isEmpty())
      out.writeTextElement(kArguments, m_arguments);
    if (!m_input.isEmpty())
      out.writeTextElement(kInput, m_input);
    if (!m_workingDirectory.isEmpty())
      out.writeTextElement(kWorkingDirectory, m_workingDirectory.toString());
    if (m_baseEnvironmentProviderId.isValid())
      out.writeTextElement(kBaseEnvironmentId, m_baseEnvironmentProviderId.toString());
    if (!m_environment.isEmpty()) {
      auto envLines = EnvironmentItem::toStringList(m_environment);
      for (auto &envLine : envLines)
        envLine = QString::fromUtf8(envLine.toUtf8().toPercentEncoding());
      out.writeTextElement(kEnvironment, envLines.join(QLatin1Char(';')));
    }
    out.writeEndElement();

    out.writeEndDocument();

    saver.setResult(&out);
  }
  return saver.finalize(errorMessage);
}

auto ExternalTool::operator==(const ExternalTool &other) const -> bool
{
  return m_id == other.m_id && m_description == other.m_description && m_displayName == other.m_displayName && m_displayCategory == other.m_displayCategory && m_order == other.m_order && m_executables == other.m_executables && m_arguments == other.m_arguments && m_input == other.m_input && m_workingDirectory == other.m_workingDirectory && m_baseEnvironmentProviderId == other.m_baseEnvironmentProviderId && m_environment == other.m_environment && m_outputHandling == other.m_outputHandling && m_modifiesCurrentDocument == other.m_modifiesCurrentDocument && m_errorHandling == other.m_errorHandling && m_filePath == other.m_filePath;
}

// #pragma mark -- ExternalToolRunner

ExternalToolRunner::ExternalToolRunner(const ExternalTool *tool) : m_tool(new ExternalTool(tool)), m_process(nullptr), m_outputCodec(QTextCodec::codecForLocale()), m_hasError(false)
{
  run();
}

ExternalToolRunner::~ExternalToolRunner()
{
  delete m_tool;
}

auto ExternalToolRunner::hasError() const -> bool
{
  return m_hasError;
}

auto ExternalToolRunner::errorString() const -> QString
{
  return m_errorString;
}

auto ExternalToolRunner::resolve() -> bool
{
  if (!m_tool)
    return false;
  m_resolvedExecutable.clear();
  m_resolvedArguments.clear();
  m_resolvedWorkingDirectory.clear();
  m_resolvedEnvironment = m_tool->baseEnvironment();

  auto expander = globalMacroExpander();
  auto expandedEnvironment = Utils::transform(m_tool->environmentUserChanges(), [expander](const EnvironmentItem &item) {
    return EnvironmentItem(item.name, expander->expand(item.value), item.operation);
  });
  m_resolvedEnvironment.modify(expandedEnvironment);

  {
    // executable
    FilePaths expandedExecutables; /* for error message */
    const auto executables = m_tool->executables();
    for (const auto &executable : executables) {
      auto expanded = expander->expand(executable);
      expandedExecutables.append(expanded);
      m_resolvedExecutable = m_resolvedEnvironment.searchInPath(expanded.path());
      if (!m_resolvedExecutable.isEmpty())
        break;
    }
    if (m_resolvedExecutable.isEmpty()) {
      m_hasError = true;
      for (auto i = 0; i < expandedExecutables.size(); ++i) {
        m_errorString += tr(R"(Could not find executable for "%1" (expanded "%2"))").arg(m_tool->executables().at(i).toUserOutput(), expandedExecutables.at(i).toUserOutput());
        m_errorString += QLatin1Char('\n');
      }
      if (!m_errorString.isEmpty())
        m_errorString.chop(1);
      return false;
    }
  }

  m_resolvedArguments = expander->expandProcessArgs(m_tool->arguments());
  m_resolvedInput = expander->expand(m_tool->input());
  m_resolvedWorkingDirectory = expander->expand(m_tool->workingDirectory());

  return true;
}

auto ExternalToolRunner::run() -> void
{
  if (!resolve()) {
    deleteLater();
    return;
  }
  if (m_tool->modifiesCurrentDocument()) {
    if (auto document = EditorManager::currentDocument()) {
      m_expectedFilePath = document->filePath();
      if (!DocumentManager::saveModifiedDocument(document)) {
        deleteLater();
        return;
      }
      DocumentManager::expectFileChange(m_expectedFilePath);
    }
  }
  m_process = new QtcProcess(this);
  connect(m_process, &QtcProcess::finished, this, &ExternalToolRunner::finished);
  connect(m_process, &QtcProcess::errorOccurred, this, &ExternalToolRunner::error);
  connect(m_process, &QtcProcess::readyReadStandardOutput, this, &ExternalToolRunner::readStandardOutput);
  connect(m_process, &QtcProcess::readyReadStandardError, this, &ExternalToolRunner::readStandardError);
  if (!m_resolvedWorkingDirectory.isEmpty())
    m_process->setWorkingDirectory(m_resolvedWorkingDirectory);
  const CommandLine cmd{m_resolvedExecutable, m_resolvedArguments, CommandLine::Raw};
  m_process->setCommand(cmd);
  m_process->setEnvironment(m_resolvedEnvironment);
  const auto write = m_tool->outputHandling() == ExternalTool::ShowInPane ? QOverload<const QString&>::of(MessageManager::writeDisrupting) : QOverload<const QString&>::of(MessageManager::writeSilently);
  write(tr("Starting external tool \"%1\"").arg(cmd.toUserOutput()));
  if (!m_resolvedInput.isEmpty())
    m_process->setWriteData(m_resolvedInput.toLocal8Bit());
  m_process->start();
}

auto ExternalToolRunner::finished() -> void
{
  if (m_process->result() == QtcProcess::FinishedWithSuccess && (m_tool->outputHandling() == ExternalTool::ReplaceSelection || m_tool->errorHandling() == ExternalTool::ReplaceSelection)) {
    ExternalToolManager::emitReplaceSelectionRequested(m_processOutput);
  }
  if (m_tool->modifiesCurrentDocument())
    DocumentManager::unexpectFileChange(m_expectedFilePath);
  const auto write = m_tool->outputHandling() == ExternalTool::ShowInPane ? QOverload<const QString&>::of(MessageManager::writeFlashing) : QOverload<const QString&>::of(MessageManager::writeSilently);
  write(tr("\"%1\" finished").arg(m_resolvedExecutable.toUserOutput()));
  deleteLater();
}

auto ExternalToolRunner::error(QProcess::ProcessError error) -> void
{
  if (m_tool->modifiesCurrentDocument())
    DocumentManager::unexpectFileChange(m_expectedFilePath);
  // TODO inform about errors
  Q_UNUSED(error)
  deleteLater();
}

auto ExternalToolRunner::readStandardOutput() -> void
{
  if (m_tool->outputHandling() == ExternalTool::Ignore)
    return;
  const auto data = m_process->readAllStandardOutput();
  const auto output = m_outputCodec->toUnicode(data.constData(), data.length(), &m_outputCodecState);
  if (m_tool->outputHandling() == ExternalTool::ShowInPane)
    MessageManager::writeSilently(output);
  else if (m_tool->outputHandling() == ExternalTool::ReplaceSelection)
    m_processOutput.append(output);
}

auto ExternalToolRunner::readStandardError() -> void
{
  if (m_tool->errorHandling() == ExternalTool::Ignore)
    return;
  const auto data = m_process->readAllStandardError();
  const auto output = m_outputCodec->toUnicode(data.constData(), data.length(), &m_errorCodecState);
  if (m_tool->errorHandling() == ExternalTool::ShowInPane)
    MessageManager::writeSilently(output);
  else if (m_tool->errorHandling() == ExternalTool::ReplaceSelection)
    m_processOutput.append(output);
}

} // namespace Orca::Plugin::Core
