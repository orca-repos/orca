// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "circularclipboardassist.hpp"
#include "texteditor.hpp"
#include "circularclipboard.hpp"

#include <codeassist/assistinterface.hpp>
#include <codeassist/iassistprocessor.hpp>
#include <codeassist/iassistproposal.hpp>
#include <codeassist/assistproposalitem.hpp>
#include <codeassist/genericproposal.hpp>

#include <utils/utilsicons.hpp>

#include <QApplication>
#include <QClipboard>

namespace TextEditor {
namespace Internal {

class ClipboardProposalItem : public AssistProposalItem {
public:
  enum {
    maxLen = 80
  };

  ClipboardProposalItem(QSharedPointer<const QMimeData> mimeData) : m_mimeData(mimeData)
  {
    auto text = mimeData->text().simplified();
    if (text.length() > maxLen) {
      text.truncate(maxLen);
      text.append(QLatin1String("..."));
    }
    setText(text);
  }

  ~ClipboardProposalItem() noexcept override = default;

  auto apply(TextDocumentManipulatorInterface &manipulator, int /*basePosition*/) const -> void override
  {
    //Move to last in circular clipboard
    if (const auto clipboard = CircularClipboard::instance()) {
      clipboard->collect(m_mimeData);
      clipboard->toLastCollect();
    }

    //Copy the selected item
    QApplication::clipboard()->setMimeData(TextEditorWidget::duplicateMimeData(m_mimeData.data()));

    //Paste
    manipulator.paste();
  }

private:
  QSharedPointer<const QMimeData> m_mimeData;
};

class ClipboardAssistProcessor : public IAssistProcessor {
public:
  auto perform(const AssistInterface *interface) -> IAssistProposal* override
  {
    if (!interface)
      return nullptr;
    const QScopedPointer AssistInterface(interface);

    const QIcon icon = QIcon::fromTheme(QLatin1String("edit-paste"), Utils::Icons::PASTE.icon()).pixmap(16);
    const auto clipboard = CircularClipboard::instance();
    QList<AssistProposalItemInterface*> items;
    items.reserve(clipboard->size());
    for (auto i = 0; i < clipboard->size(); ++i) {
      const auto data = clipboard->next();

      AssistProposalItem *item = new ClipboardProposalItem(data);
      item->setIcon(icon);
      item->setOrder(clipboard->size() - 1 - i);
      items.append(item);
    }

    return new GenericProposal(interface->position(), items);
  }
};

auto ClipboardAssistProvider::runType() const -> RunType
{
  return Synchronous;
}

auto ClipboardAssistProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new ClipboardAssistProcessor;
}

} // namespace Internal
} // namespace TextEditor
