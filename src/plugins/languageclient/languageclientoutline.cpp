// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientoutline.hpp"

#include "languageclientmanager.hpp"
#include "languageclientutils.hpp"

#include <core/core-item-view-find.hpp>
#include <core/core-editor-interface.hpp>
#include <languageserverprotocol/languagefeatures.h>
#include <texteditor/outlinefactory.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/itemviews.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/treemodel.hpp>
#include <utils/treeviewcombobox.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QBoxLayout>
#include <QSortFilterProxyModel>

using namespace LanguageServerProtocol;

namespace LanguageClient {

class LanguageClientOutlineItem : public Utils::TypedTreeItem<LanguageClientOutlineItem> {
public:
  LanguageClientOutlineItem() = default;
  LanguageClientOutlineItem(const SymbolInformation &info) : m_name(info.name()), m_range(info.location().range()), m_type(info.kind()) { }

  LanguageClientOutlineItem(const DocumentSymbol &info, const SymbolStringifier &stringifier) : m_name(info.name()), m_detail(info.detail().value_or(QString())), m_range(info.range()), m_symbolStringifier(stringifier), m_type(info.kind())
  {
    for (const auto &child : info.children().value_or(QList<DocumentSymbol>()))
      appendChild(new LanguageClientOutlineItem(child, stringifier));
  }

  // TreeItem interface
  auto data(int column, int role) const -> QVariant override
  {
    switch (role) {
    case Qt::DecorationRole:
      return symbolIcon(m_type);
    case Qt::DisplayRole:
      return m_symbolStringifier ? m_symbolStringifier(static_cast<SymbolKind>(m_type), m_name, m_detail) : m_name;
    default:
      return Utils::TreeItem::data(column, role);
    }
  }

  auto range() const -> Range { return m_range; }
  auto pos() const -> Position { return m_range.start(); }
  auto contains(const Position &pos) const -> bool { return m_range.contains(pos); }

private:
  QString m_name;
  QString m_detail;
  Range m_range;
  SymbolStringifier m_symbolStringifier;
  int m_type = -1;
};

class LanguageClientOutlineModel : public Utils::TreeModel<LanguageClientOutlineItem> {
public:
  using Utils::TreeModel<LanguageClientOutlineItem>::TreeModel;

  auto setInfo(const QList<SymbolInformation> &info) -> void
  {
    clear();
    for (const auto &symbol : info)
      rootItem()->appendChild(new LanguageClientOutlineItem(symbol));
  }

  auto setInfo(const QList<DocumentSymbol> &info) -> void
  {
    clear();
    for (const auto &symbol : info)
      rootItem()->appendChild(new LanguageClientOutlineItem(symbol, m_symbolStringifier));
  }

  auto setSymbolStringifier(const SymbolStringifier &stringifier) -> void
  {
    m_symbolStringifier = stringifier;
  }

private:
  SymbolStringifier m_symbolStringifier;
};

class LanguageClientOutlineWidget : public TextEditor::IOutlineWidget {
public:
  LanguageClientOutlineWidget(Client *client, TextEditor::BaseTextEditor *editor);

  // IOutlineWidget interface
public:
  auto filterMenuActions() const -> QList<QAction*> override;
  auto setCursorSynchronization(bool syncWithCursor) -> void override;
  auto setSorted(bool) -> void override;
  auto isSorted() const -> bool override;
  auto restoreSettings(const QVariantMap &map) -> void override;
  auto settings() const -> QVariantMap override;

private:
  auto handleResponse(const DocumentUri &uri, const DocumentSymbolsResult &response) -> void;
  auto updateTextCursor(const QModelIndex &proxyIndex) -> void;
  auto updateSelectionInTree(const QTextCursor &currentCursor) -> void;
  auto onItemActivated(const QModelIndex &index) -> void;

  QPointer<Client> m_client;
  QPointer<TextEditor::BaseTextEditor> m_editor;
  LanguageClientOutlineModel m_model;
  QSortFilterProxyModel m_proxyModel;
  Utils::TreeView m_view;
  DocumentUri m_uri;
  bool m_sync = false;
  bool m_sorted = false;
};

LanguageClientOutlineWidget::LanguageClientOutlineWidget(Client *client, TextEditor::BaseTextEditor *editor) : m_client(client), m_editor(editor), m_view(this), m_uri(DocumentUri::fromFilePath(editor->textDocument()->filePath()))
{
  connect(client->documentSymbolCache(), &DocumentSymbolCache::gotSymbols, this, &LanguageClientOutlineWidget::handleResponse);
  connect(client, &Client::documentUpdated, this, [this](TextEditor::TextDocument *document) {
    if (m_client && m_uri == DocumentUri::fromFilePath(document->filePath()))
      m_client->documentSymbolCache()->requestSymbols(m_uri, Schedule::Delayed);
  });

  client->documentSymbolCache()->requestSymbols(m_uri, Schedule::Delayed);

  auto *layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(Orca::Plugin::Core::ItemViewFind::createSearchableWrapper(&m_view));
  setLayout(layout);
  m_model.setSymbolStringifier(m_client->symbolStringifier());
  m_proxyModel.setSourceModel(&m_model);
  m_view.setModel(&m_proxyModel);
  m_view.setHeaderHidden(true);
  m_view.setExpandsOnDoubleClick(false);
  m_view.setFrameStyle(QFrame::NoFrame);
  connect(&m_view, &QAbstractItemView::activated, this, &LanguageClientOutlineWidget::onItemActivated);
  connect(m_editor->editorWidget(), &TextEditor::TextEditorWidget::cursorPositionChanged, this, [this]() {
    if (m_sync)
      updateSelectionInTree(m_editor->textCursor());
  });
}

auto LanguageClientOutlineWidget::filterMenuActions() const -> QList<QAction*>
{
  return {};
}

auto LanguageClientOutlineWidget::setCursorSynchronization(bool syncWithCursor) -> void
{
  m_sync = syncWithCursor;
  if (m_sync && m_editor)
    updateSelectionInTree(m_editor->textCursor());
}

auto LanguageClientOutlineWidget::setSorted(bool sorted) -> void
{
  m_sorted = sorted;
  m_proxyModel.sort(sorted ? 0 : -1);
}

auto LanguageClientOutlineWidget::isSorted() const -> bool
{
  return m_sorted;
}

auto LanguageClientOutlineWidget::restoreSettings(const QVariantMap &map) -> void
{
  setSorted(map.value(QString("LspOutline.Sort"), false).toBool());
}

auto LanguageClientOutlineWidget::settings() const -> QVariantMap
{
  return {{QString("LspOutline.Sort"), m_sorted}};
}

auto LanguageClientOutlineWidget::handleResponse(const DocumentUri &uri, const DocumentSymbolsResult &result) -> void
{
  if (uri != m_uri)
    return;
  if (Utils::holds_alternative<QList<SymbolInformation>>(result))
    m_model.setInfo(Utils::get<QList<SymbolInformation>>(result));
  else if (Utils::holds_alternative<QList<DocumentSymbol>>(result))
    m_model.setInfo(Utils::get<QList<DocumentSymbol>>(result));
  else
    m_model.clear();

  // The list has changed, update the current items
  updateSelectionInTree(m_editor->textCursor());
}

auto LanguageClientOutlineWidget::updateTextCursor(const QModelIndex &proxyIndex) -> void
{
  const auto item = m_model.itemForIndex(m_proxyModel.mapToSource(proxyIndex));
  const auto &pos = item->pos();
  // line has to be 1 based, column 0 based!
  m_editor->editorWidget()->gotoLine(pos.line() + 1, pos.character(), true, true);
}

static auto itemForCursor(const LanguageClientOutlineModel &m_model, const QTextCursor &cursor) -> LanguageClientOutlineItem*
{
  const Position pos(cursor);
  LanguageClientOutlineItem *result = nullptr;
  m_model.forAllItems([&](LanguageClientOutlineItem *candidate) {
    if (!candidate->contains(pos))
      return;
    if (result && candidate->range().contains(result->range()))
      return; // skip item if the range is equal or bigger than the previous found range
    result = candidate;
  });
  return result;
}

auto LanguageClientOutlineWidget::updateSelectionInTree(const QTextCursor &currentCursor) -> void
{
  if (const auto item = itemForCursor(m_model, currentCursor)) {
    const auto index = m_proxyModel.mapFromSource(m_model.indexForItem(item));
    m_view.selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    m_view.scrollTo(index);
  } else {
    m_view.clearSelection();
  }
}

auto LanguageClientOutlineWidget::onItemActivated(const QModelIndex &index) -> void
{
  if (!index.isValid() || !m_editor)
    return;

  updateTextCursor(index);
  m_editor->widget()->setFocus();
}

auto LanguageClientOutlineWidgetFactory::clientSupportsDocumentSymbols(const Client *client, const TextEditor::TextDocument *doc) -> bool
{
  if (!client)
    return false;
  const auto dc = client->dynamicCapabilities();
  if (dc.isRegistered(DocumentSymbolsRequest::methodName).value_or(false)) {
    const TextDocumentRegistrationOptions options(dc.option(DocumentSymbolsRequest::methodName));
    return !options.isValid() || options.filterApplies(doc->filePath(), Utils::mimeTypeForName(doc->mimeType()));
  }
  const auto &provider = client->capabilities().documentSymbolProvider();
  if (!provider.has_value())
    return false;
  if (Utils::holds_alternative<bool>(*provider))
    return Utils::get<bool>(*provider);
  return true;
}

auto LanguageClientOutlineWidgetFactory::supportsEditor(Orca::Plugin::Core::IEditor *editor) const -> bool
{
  const auto doc = qobject_cast<TextEditor::TextDocument*>(editor->document());
  if (!doc)
    return false;
  return clientSupportsDocumentSymbols(LanguageClientManager::clientForDocument(doc), doc);
}

auto LanguageClientOutlineWidgetFactory::createWidget(Orca::Plugin::Core::IEditor *editor) -> TextEditor::IOutlineWidget*
{
  const auto textEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor);
  QTC_ASSERT(textEditor, return nullptr);
  const auto client = LanguageClientManager::clientForDocument(textEditor->textDocument());
  if (!client || !clientSupportsDocumentSymbols(client, textEditor->textDocument()))
    return nullptr;
  return new LanguageClientOutlineWidget(client, textEditor);
}

class OutlineComboBox : public Utils::TreeViewComboBox {
public:
  OutlineComboBox(Client *client, TextEditor::BaseTextEditor *editor);

private:
  auto updateModel(const DocumentUri &resultUri, const DocumentSymbolsResult &result) -> void;
  auto updateEntry() -> void;
  auto activateEntry() -> void;
  auto documentUpdated(TextEditor::TextDocument *document) -> void;
  auto setSorted(bool sorted) -> void;

  LanguageClientOutlineModel m_model;
  QSortFilterProxyModel m_proxyModel;
  QPointer<Client> m_client;
  TextEditor::TextEditorWidget *m_editorWidget;
  const DocumentUri m_uri;
};

auto LanguageClientOutlineWidgetFactory::createComboBox(Client *client, Orca::Plugin::Core::IEditor *editor) -> Utils::TreeViewComboBox*
{
  const auto textEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor);
  QTC_ASSERT(textEditor, return nullptr);
  const auto document = textEditor->textDocument();
  if (!client || !clientSupportsDocumentSymbols(client, document))
    return nullptr;

  return new OutlineComboBox(client, textEditor);
}

OutlineComboBox::OutlineComboBox(Client *client, TextEditor::BaseTextEditor *editor) : m_client(client), m_editorWidget(editor->editorWidget()), m_uri(DocumentUri::fromFilePath(editor->document()->filePath()))
{
  m_model.setSymbolStringifier(client->symbolStringifier());
  m_proxyModel.setSourceModel(&m_model);
  const auto sorted = LanguageClientSettings::outlineComboBoxIsSorted();
  m_proxyModel.sort(sorted ? 0 : -1);
  setModel(&m_proxyModel);
  setMinimumContentsLength(13);
  auto policy = sizePolicy();
  policy.setHorizontalPolicy(QSizePolicy::Expanding);
  setSizePolicy(policy);
  setMaxVisibleItems(40);

  setContextMenuPolicy(Qt::ActionsContextMenu);
  const auto sortActionText = QCoreApplication::translate("TextEditor::Internal::OutlineWidgetStack", "Sort Alphabetically");
  const auto sortAction = new QAction(sortActionText, this);
  sortAction->setCheckable(true);
  sortAction->setChecked(sorted);
  addAction(sortAction);

  connect(client->documentSymbolCache(), &DocumentSymbolCache::gotSymbols, this, &OutlineComboBox::updateModel);
  connect(client, &Client::documentUpdated, this, &OutlineComboBox::documentUpdated);
  connect(m_editorWidget, &TextEditor::TextEditorWidget::cursorPositionChanged, this, &OutlineComboBox::updateEntry);
  connect(this, QOverload<int>::of(&QComboBox::activated), this, &OutlineComboBox::activateEntry);
  connect(sortAction, &QAction::toggled, this, &OutlineComboBox::setSorted);

  documentUpdated(editor->textDocument());
}

auto OutlineComboBox::updateModel(const DocumentUri &resultUri, const DocumentSymbolsResult &result) -> void
{
  if (m_uri != resultUri)
    return;
  if (Utils::holds_alternative<QList<SymbolInformation>>(result))
    m_model.setInfo(Utils::get<QList<SymbolInformation>>(result));
  else if (Utils::holds_alternative<QList<DocumentSymbol>>(result))
    m_model.setInfo(Utils::get<QList<DocumentSymbol>>(result));
  else
    m_model.clear();

  view()->expandAll();
  // The list has changed, update the current item
  updateEntry();
}

auto OutlineComboBox::updateEntry() -> void
{
  if (const auto item = itemForCursor(m_model, m_editorWidget->textCursor()))
    setCurrentIndex(m_proxyModel.mapFromSource(m_model.indexForItem(item)));
}

auto OutlineComboBox::activateEntry() -> void
{
  const auto modelIndex = m_proxyModel.mapToSource(view()->currentIndex());
  if (modelIndex.isValid()) {
    const auto &pos = m_model.itemForIndex(modelIndex)->pos();
    Orca::Plugin::Core::EditorManager::cutForwardNavigationHistory();
    Orca::Plugin::Core::EditorManager::addCurrentPositionToNavigationHistory();
    // line has to be 1 based, column 0 based!
    m_editorWidget->gotoLine(pos.line() + 1, pos.character(), true, true);
    emit m_editorWidget->activateEditor();
  }
}

auto OutlineComboBox::documentUpdated(TextEditor::TextDocument *document) -> void
{
  if (document == m_editorWidget->textDocument())
    m_client->documentSymbolCache()->requestSymbols(m_uri, Schedule::Delayed);
}

auto OutlineComboBox::setSorted(bool sorted) -> void
{
  LanguageClientSettings::setOutlineComboBoxSorted(sorted);
  m_proxyModel.sort(sorted ? 0 : -1);
}

} // namespace LanguageClient
