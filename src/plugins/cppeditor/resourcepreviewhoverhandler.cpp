// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourcepreviewhoverhandler.hpp"

#include <core/icore.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/projecttree.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/executeondestruction.hpp>
#include <utils/fileutils.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/tooltip/tooltip.hpp>

#include <QPoint>
#include <QTextBlock>

using namespace Core;
using namespace TextEditor;

namespace CppEditor {
namespace Internal {

/*
 * finds a quoted sub-string around the pos in the given string
 *
 * note: the returned string includes the quotes.
 */
static auto extractQuotedString(const QString &s, int pos) -> QString
{
  if (s.length() < 2 || pos < 0 || pos >= s.length())
    return QString();

  const int firstQuote = s.lastIndexOf('"', pos);
  if (firstQuote >= 0) {
    int endQuote = s.indexOf('"', firstQuote + 1);
    if (endQuote > firstQuote)
      return s.mid(firstQuote, endQuote - firstQuote);
  }

  return QString();
}

static auto makeResourcePath(const QStringList &prefixList, const QString &file) -> QString
{
  QTC_ASSERT(!prefixList.isEmpty(), return QString());

  const QChar sep = '/';
  auto prefix = prefixList.join(sep);
  if (prefix == sep)
    return prefix + file;

  return prefix + sep + file;
}

/*
 * tries to match a resource within a given .qrc file, including by alias
 *
 * note: resource name should not have any semi-colon in front of it
 */
static auto findResourceInFile(const QString &resName, const QString &filePathName) -> QString
{
  Utils::FileReader reader;
  if (!reader.fetch(Utils::FilePath::fromString(filePathName)))
    return QString();

  const auto contents = reader.data();
  QXmlStreamReader xmlr(contents);

  QStringList prefixStack;

  while (!xmlr.atEnd() && !xmlr.hasError()) {
    const auto token = xmlr.readNext();
    if (token == QXmlStreamReader::StartElement) {
      if (xmlr.name() == QLatin1String("qresource")) {
        const auto sa = xmlr.attributes();
        const auto prefixName = sa.value("prefix").toString();
        if (!prefixName.isEmpty())
          prefixStack.push_back(prefixName);
      } else if (xmlr.name() == QLatin1String("file")) {
        const auto sa = xmlr.attributes();
        const auto aliasName = sa.value("alias").toString();
        const auto fileName = xmlr.readElementText();

        if (!aliasName.isEmpty()) {
          const auto fullAliasName = makeResourcePath(prefixStack, aliasName);
          if (resName == fullAliasName)
            return fileName;
        }

        const auto fullResName = makeResourcePath(prefixStack, fileName);
        if (resName == fullResName)
          return fileName;
      }
    } else if (token == QXmlStreamReader::EndElement) {
      if (xmlr.name() == QLatin1String("qresource")) {
        if (!prefixStack.isEmpty())
          prefixStack.pop_back();
      }
    }
  }

  return QString();
}

/*
 * A more efficient way to do this would be to parse the relevant project files
 * before hand, or cache them as we go - but this works well enough so far.
 */
static auto findResourceInProject(const QString &resName) -> QString
{
  auto s = resName;
  s.remove('"');

  if (s.startsWith(":/"))
    s.remove(0, 1);
  else if (s.startsWith("qrc://"))
    s.remove(0, 5);
  else
    return QString();

  if (auto *project = ProjectExplorer::ProjectTree::currentProject()) {
    const auto files = project->files([](const ProjectExplorer::Node *n) { return n->filePath().endsWith(".qrc"); });
    for (const auto &file : files) {
      const auto fi = file.toFileInfo();
      if (!fi.isReadable())
        continue;
      const auto fileName = findResourceInFile(s, file.toString());
      if (fileName.isEmpty())
        continue;

      auto ret = fi.absolutePath();
      if (!ret.endsWith('/'))
        ret.append('/');
      ret.append(fileName);
      return ret;
    }
  }

  return QString();
}

auto ResourcePreviewHoverHandler::identifyMatch(TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void
{
  Utils::ExecuteOnDestruction reportPriority([this, report]() { report(priority()); });

  if (editorWidget->extraSelectionTooltip(pos).isEmpty()) {
    const auto tb = editorWidget->document()->findBlock(pos);
    const auto tbpos = pos - tb.position();
    const auto tbtext = tb.text();

    const auto resPath = extractQuotedString(tbtext, tbpos);
    m_resPath = findResourceInProject(resPath);

    setPriority(m_resPath.isEmpty() ? Priority_None : Priority_Diagnostic + 1);
  }
}

auto ResourcePreviewHoverHandler::operateTooltip(TextEditorWidget *editorWidget, const QPoint &point) -> void
{
  const auto tt = makeTooltip();
  if (!tt.isEmpty())
    Utils::ToolTip::show(point, tt, editorWidget);
  else
    Utils::ToolTip::hide();
}

auto ResourcePreviewHoverHandler::makeTooltip() const -> QString
{
  if (m_resPath.isEmpty())
    return QString();

  QString ret;

  const auto mimeType = Utils::mimeTypeForFile(m_resPath);
  if (mimeType.name().startsWith("image", Qt::CaseInsensitive))
    ret += QString("<img src=\"file:///%1\" /><br/>").arg(m_resPath);

  ret += QString("<a href=\"file:///%1\">%2</a>").arg(m_resPath, QDir::toNativeSeparators(m_resPath));
  return ret;
}

} // namespace Internal
} // namespace CppEditor
