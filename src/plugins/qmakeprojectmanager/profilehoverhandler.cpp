// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "profilehoverhandler.hpp"
#include "profilecompletionassist.hpp"
#include "qmakeprojectmanagerconstants.hpp"

#include <core/core-help-manager.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/htmldocextractor.hpp>
#include <utils/executeondestruction.hpp>

#include <QTextBlock>
#include <QUrl>

using namespace Orca::Plugin::Core;

namespace QmakeProjectManager {
namespace Internal {

ProFileHoverHandler::ProFileHoverHandler() : m_keywords(qmakeKeywords()) {}

auto ProFileHoverHandler::identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void
{
  Utils::ExecuteOnDestruction reportPriority([this, report]() { report(priority()); });

  m_docFragment.clear();
  m_manualKind = UnknownManual;
  if (!editorWidget->extraSelectionTooltip(pos).isEmpty()) {
    setToolTip(editorWidget->extraSelectionTooltip(pos));
  } else {
    auto document = editorWidget->document();
    auto block = document->findBlock(pos);
    identifyQMakeKeyword(block.text(), pos - block.position());

    if (m_manualKind != UnknownManual) {
      QUrl url(QString::fromLatin1("qthelp://org.qt-project.qmake/qmake/qmake-%1-reference.html#%2").arg(manualName()).arg(m_docFragment));
      setLastHelpItemIdentified(Orca::Plugin::Core::HelpItem(url, m_docFragment, Orca::Plugin::Core::HelpItem::QMakeVariableOfFunction));
    } else {
      // General qmake manual will be shown outside any function or variable
      setLastHelpItemIdentified("qmake");
    }
  }
}

auto ProFileHoverHandler::identifyQMakeKeyword(const QString &text, int pos) -> void
{
  if (text.isEmpty())
    return;

  QString buf;

  for (auto i = 0; i < text.length(); ++i) {
    const auto c = text.at(i);
    auto checkBuffer = false;
    if (c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('.') || c.isDigit()) {
      buf += c;
      if (i == text.length() - 1)
        checkBuffer = true;
    } else {
      checkBuffer = true;
    }
    if (checkBuffer) {
      if (!buf.isEmpty()) {
        if ((i >= pos) && (i - buf.size() <= pos)) {
          if (m_keywords.isFunction(buf))
            identifyDocFragment(FunctionManual, buf);
          else if (m_keywords.isVariable(buf))
            identifyDocFragment(VariableManual, buf);
          break;
        }
        buf.clear();
      } else {
        if (i >= pos)
          break; // we are after the tooltip pos
      }
      if (c == QLatin1Char('#'))
        break; // comment start
    }
  }
}

auto ProFileHoverHandler::manualName() const -> QString
{
  if (m_manualKind == FunctionManual)
    return QLatin1String("function");
  else if (m_manualKind == VariableManual)
    return QLatin1String("variable");
  return QString();
}

auto ProFileHoverHandler::identifyDocFragment(ProFileHoverHandler::ManualKind manualKind, const QString &keyword) -> void
{
  m_manualKind = manualKind;
  m_docFragment = keyword.toLower();
  // Special case: _PRO_FILE_ and _PRO_FILE_PWD_ ids
  // don't have starting and ending '_'.
  if (m_docFragment.startsWith(QLatin1Char('_')))
    m_docFragment = m_docFragment.mid(1);
  if (m_docFragment.endsWith(QLatin1Char('_')))
    m_docFragment = m_docFragment.left(m_docFragment.size() - 1);
  m_docFragment.replace(QLatin1Char('.'), QLatin1Char('-'));
  m_docFragment.replace(QLatin1Char('_'), QLatin1Char('-'));

  if (m_manualKind == FunctionManual) {
    QUrl url(QString::fromLatin1("qthelp://org.qt-project.qmake/qmake/qmake-%1-reference.html").arg(manualName()));
    const auto html = Orca::Plugin::Core::fileData(url);

    Utils::HtmlDocExtractor htmlExtractor;
    htmlExtractor.setMode(Utils::HtmlDocExtractor::FirstParagraph);

    // Document fragment of qmake function is retrieved from docs.
    // E.g. in case of the keyword "find" the document fragment
    // parsed from docs is "find-variablename-substr".
    m_docFragment = htmlExtractor.getQMakeFunctionId(QString::fromUtf8(html), m_docFragment);
  }
}

} // namespace Internal
} // namespace QmakeProjectManager
