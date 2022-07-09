// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "profileeditor.hpp"

#include "profilecompletionassist.hpp"
#include "profilehighlighter.hpp"
#include "profilehoverhandler.hpp"
#include "qmakenodes.hpp"
#include "qmakeproject.hpp"
#include "qmakeprojectmanagerconstants.hpp"

#include <core/fileiconprovider.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <qtsupport/qtsupportconstants.hpp>
#include <projectexplorer/buildconfiguration.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/session.hpp>
#include <texteditor/texteditoractionhandler.hpp>
#include <texteditor/textdocument.hpp>
#include <utils/qtcassert.hpp>
#include <utils/theme/theme.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QTextBlock>

#include <algorithm>

using namespace ProjectExplorer;
using namespace TextEditor;
using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

class ProFileEditorWidget : public TextEditorWidget {
  auto findLinkAt(const QTextCursor &, Utils::ProcessLinkCallback &&processLinkCallback, bool resolveTarget = true, bool inNextSplit = false) -> void override;
  auto contextMenuEvent(QContextMenuEvent *) -> void override;
  auto checkForPrfFile(const QString &baseName) const -> QString;
};

static auto isValidFileNameChar(const QChar &c) -> bool
{
  return c.isLetterOrNumber() || c == QLatin1Char('.') || c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char('/') || c == QLatin1Char('\\');
}

auto ProFileEditorWidget::checkForPrfFile(const QString &baseName) const -> QString
{
  const auto projectFile = textDocument()->filePath();
  const QmakePriFileNode *projectNode = nullptr;

  // FIXME: Remove this check once project nodes are fully "static".
  for (const Project *const project : SessionManager::projects()) {
    static const auto isParsing = [](const Project *project) {
      for (const Target *const t : project->targets()) {
        for (const BuildConfiguration *const bc : t->buildConfigurations()) {
          if (bc->buildSystem()->isParsing())
            return true;
        }
      }
      return false;
    };
    if (isParsing(project))
      continue;

    const auto rootNode = project->rootProjectNode();
    QTC_ASSERT(rootNode, continue);
    projectNode = dynamic_cast<const QmakePriFileNode*>(rootNode->findProjectNode([&projectFile](const ProjectNode *pn) {
      return pn->filePath() == projectFile;
    }));
    if (projectNode)
      break;
  }
  if (!projectNode)
    return QString();
  const QmakeProFileNode *const proFileNode = projectNode->proFileNode();
  if (!proFileNode)
    return QString();
  const QmakeProFile *const proFile = proFileNode->proFile();
  if (!proFile)
    return QString();
  for (const auto &featureRoot : proFile->featureRoots()) {
    const QFileInfo candidate(featureRoot + '/' + baseName + ".prf");
    if (candidate.exists())
      return candidate.filePath();
  }
  return QString();
}

auto ProFileEditorWidget::findLinkAt(const QTextCursor &cursor, Utils::ProcessLinkCallback &&processLinkCallback, bool /*resolveTarget*/, bool /*inNextSplit*/) -> void
{
  Link link;

  auto line = 0;
  auto column = 0;
  convertPosition(cursor.position(), &line, &column);
  const auto positionInBlock = column - 1;

  const auto block = cursor.block().text();

  // check if the current position is commented out
  const int hashPos = block.indexOf(QLatin1Char('#'));
  if (hashPos >= 0 && hashPos < positionInBlock)
    return processLinkCallback(link);

  // find the beginning of a filename
  QString buffer;
  auto beginPos = positionInBlock - 1;
  auto endPos = positionInBlock;

  // Check is cursor is somewhere on $${PWD}:
  const auto chunkStart = std::max(0, positionInBlock - 7);
  const auto chunkLength = 14 + std::min(0, positionInBlock - 7);
  auto chunk = block.mid(chunkStart, chunkLength);

  const QString curlyPwd = "$${PWD}";
  const QString pwd = "$$PWD";
  const int posCurlyPwd = chunk.indexOf(curlyPwd);
  const int posPwd = chunk.indexOf(pwd);
  auto doBackwardScan = true;

  if (posCurlyPwd >= 0) {
    const int end = chunkStart + posCurlyPwd + curlyPwd.count();
    const auto start = chunkStart + posCurlyPwd;
    if (start <= positionInBlock && end >= positionInBlock) {
      buffer = pwd;
      beginPos = chunkStart + posCurlyPwd - 1;
      endPos = end;
      doBackwardScan = false;
    }
  } else if (posPwd >= 0) {
    const int end = chunkStart + posPwd + pwd.count();
    const auto start = chunkStart + posPwd;
    if (start <= positionInBlock && end >= positionInBlock) {
      buffer = pwd;
      beginPos = start - 1;
      endPos = end;
      doBackwardScan = false;
    }
  }

  while (doBackwardScan && beginPos >= 0) {
    auto c = block.at(beginPos);
    if (isValidFileNameChar(c)) {
      buffer.prepend(c);
      beginPos--;
    } else {
      break;
    }
  }

  if (doBackwardScan && beginPos > 0 && block.mid(beginPos - 1, pwd.count()) == pwd && (block.at(beginPos + pwd.count() - 1) == '/' || block.at(beginPos + pwd.count() - 1) == '\\')) {
    buffer.prepend("$$");
    beginPos -= 2;
  } else if (doBackwardScan && beginPos >= curlyPwd.count() - 1 && block.mid(beginPos - curlyPwd.count() + 1, curlyPwd.count()) == curlyPwd) {
    buffer.prepend(pwd);
    beginPos -= curlyPwd.count();
  }

  // find the end of a filename
  while (endPos < block.count()) {
    auto c = block.at(endPos);
    if (isValidFileNameChar(c)) {
      buffer.append(c);
      endPos++;
    } else {
      break;
    }
  }

  if (buffer.isEmpty())
    return processLinkCallback(link);

  // remove trailing '\' since it can be line continuation char
  if (buffer.at(buffer.size() - 1) == QLatin1Char('\\')) {
    buffer.chop(1);
    endPos--;
  }

  // if the buffer starts with $$PWD accept it
  if (buffer.startsWith("$$PWD/") || buffer.startsWith("$$PWD\\"))
    buffer = buffer.mid(6);

  QDir dir(textDocument()->filePath().toFileInfo().absolutePath());
  auto fileName = dir.filePath(buffer);
  QFileInfo fi(fileName);
  if (Utils::HostOsInfo::isWindowsHost() && fileName.startsWith("//")) {
    // Windows network paths are not supported here since checking for their existence can
    // lock the gui thread. See: QTCREATORBUG-26579
  } else if (fi.exists()) {
    if (fi.isDir()) {
      QDir subDir(fi.absoluteFilePath());
      auto subProject = subDir.filePath(subDir.dirName() + QLatin1String(".pro"));
      if (QFileInfo::exists(subProject))
        fileName = subProject;
      else
        return processLinkCallback(link);
    }
    link.targetFilePath = Utils::FilePath::fromString(QDir::cleanPath(fileName));
  } else {
    link.targetFilePath = Utils::FilePath::fromString(checkForPrfFile(buffer));
  }
  if (!link.targetFilePath.isEmpty()) {
    link.linkTextStart = cursor.position() - positionInBlock + beginPos + 1;
    link.linkTextEnd = cursor.position() - positionInBlock + endPos;
  }
  processLinkCallback(link);
}

auto ProFileEditorWidget::contextMenuEvent(QContextMenuEvent *e) -> void
{
  showDefaultContextMenu(e, Constants::M_CONTEXT);
}

static auto createProFileDocument() -> TextDocument*
{
  auto doc = new TextDocument;
  doc->setId(Constants::PROFILE_EDITOR_ID);
  doc->setMimeType(QLatin1String(Constants::PROFILE_MIMETYPE));
  // qmake project files do not support UTF8-BOM
  // If the BOM would be added qmake would fail and Qt Creator couldn't parse the project file
  doc->setSupportsUtf8Bom(false);
  return doc;
}

//
// ProFileEditorFactory
//

ProFileEditorFactory::ProFileEditorFactory()
{
  setId(Constants::PROFILE_EDITOR_ID);
  setDisplayName(QCoreApplication::translate("OpenWith::Editors", Constants::PROFILE_EDITOR_DISPLAY_NAME));
  addMimeType(Constants::PROFILE_MIMETYPE);
  addMimeType(Constants::PROINCLUDEFILE_MIMETYPE);
  addMimeType(Constants::PROFEATUREFILE_MIMETYPE);
  addMimeType(Constants::PROCONFIGURATIONFILE_MIMETYPE);
  addMimeType(Constants::PROCACHEFILE_MIMETYPE);
  addMimeType(Constants::PROSTASHFILE_MIMETYPE);

  setDocumentCreator(createProFileDocument);
  setEditorWidgetCreator([]() { return new ProFileEditorWidget; });

  const auto completionAssistProvider = new KeywordsCompletionAssistProvider(qmakeKeywords());
  completionAssistProvider->setDynamicCompletionFunction(&TextEditor::pathComplete);
  setCompletionAssistProvider(completionAssistProvider);

  setCommentDefinition(Utils::CommentDefinition::HashStyle);
  setEditorActionHandlers(TextEditorActionHandler::UnCommentSelection | TextEditorActionHandler::JumpToFileUnderCursor);

  addHoverHandler(new ProFileHoverHandler);
  setSyntaxHighlighterCreator([]() { return new ProFileHighlighter; });

  const QString defaultOverlay = QLatin1String(ProjectExplorer::Constants::FILEOVERLAY_QT);
  Core::FileIconProvider::registerIconOverlayForSuffix(orcaTheme()->imageFile(Theme::IconOverlayPro, defaultOverlay), "pro");
  Core::FileIconProvider::registerIconOverlayForSuffix(orcaTheme()->imageFile(Theme::IconOverlayPri, defaultOverlay), "pri");
  Core::FileIconProvider::registerIconOverlayForSuffix(orcaTheme()->imageFile(Theme::IconOverlayPrf, defaultOverlay), "prf");
}

} // namespace Internal
} // namespace QmakeProjectManager
