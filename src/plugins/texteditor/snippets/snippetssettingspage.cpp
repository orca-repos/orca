// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "snippetssettingspage.hpp"
#include "snippeteditor.hpp"
#include "snippetprovider.hpp"
#include "snippet.hpp"
#include "snippetscollection.hpp"
#include "snippetssettings.hpp"
#include "ui_snippetssettingspage.h"

#include <texteditor/fontsettings.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditorconstants.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <core/icore.hpp>

#include <utils/headerviewstretcher.hpp>

#include <QAbstractTableModel>
#include <QList>
#include <QMessageBox>
#include <QPointer>

namespace TextEditor {
namespace Internal {

// SnippetsTableModel
class SnippetsTableModel : public QAbstractTableModel {
  Q_OBJECT public:
  SnippetsTableModel(QObject *parent);
  ~SnippetsTableModel() override = default;

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto flags(const QModelIndex &modelIndex) const -> Qt::ItemFlags override;
  auto data(const QModelIndex &modelIndex, int role = Qt::DisplayRole) const -> QVariant override;
  auto setData(const QModelIndex &modelIndex, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const -> QVariant override;
  auto groupIds() const -> QList<QString>;
  auto load(const QString &groupId) -> void;
  auto createSnippet() -> QModelIndex;
  auto insertSnippet(const Snippet &snippet) -> QModelIndex;
  auto removeSnippet(const QModelIndex &modelIndex) -> void;
  auto snippetAt(const QModelIndex &modelIndex) const -> const Snippet&;
  auto setSnippetContent(const QModelIndex &modelIndex, const QString &content) -> void;
  auto revertBuitInSnippet(const QModelIndex &modelIndex) -> void;
  auto restoreRemovedBuiltInSnippets() -> void;
  auto resetSnippets() -> void;

private:
  auto replaceSnippet(const Snippet &snippet, const QModelIndex &modelIndex) -> void;

  SnippetsCollection *m_collection;
  QString m_activeGroupId;
};

SnippetsTableModel::SnippetsTableModel(QObject *parent) : QAbstractTableModel(parent), m_collection(SnippetsCollection::instance()) {}

auto SnippetsTableModel::rowCount(const QModelIndex &) const -> int
{
  return m_collection->totalActiveSnippets(m_activeGroupId);
}

auto SnippetsTableModel::columnCount(const QModelIndex &) const -> int
{
  return 2;
}

auto SnippetsTableModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  auto itemFlags = QAbstractTableModel::flags(index);
  if (index.isValid())
    itemFlags |= Qt::ItemIsEditable;
  return itemFlags;
}

auto SnippetsTableModel::data(const QModelIndex &modelIndex, int role) const -> QVariant
{
  if (!modelIndex.isValid())
    return QVariant();

  if (role == Qt::DisplayRole || role == Qt::EditRole) {
    const auto &snippet = m_collection->snippet(modelIndex.row(), m_activeGroupId);
    if (modelIndex.column() == 0)
      return snippet.trigger();
    return snippet.complement();
  }
  return QVariant();
}

auto SnippetsTableModel::setData(const QModelIndex &modelIndex, const QVariant &value, int role) -> bool
{
  if (modelIndex.isValid() && role == Qt::EditRole) {
    auto snippet(m_collection->snippet(modelIndex.row(), m_activeGroupId));
    if (modelIndex.column() == 0) {
      const auto &s = value.toString();
      if (!Snippet::isValidTrigger(s)) {
        QMessageBox::critical(Core::ICore::dialogParent(), tr("Error"), tr("Not a valid trigger. A valid trigger can only contain letters, " "numbers, or underscores, where the first character is " "limited to letter or underscore."));
        if (snippet.trigger().isEmpty())
          removeSnippet(modelIndex);
        return false;
      }
      snippet.setTrigger(s);
    } else {
      snippet.setComplement(value.toString());
    }

    replaceSnippet(snippet, modelIndex);
    return true;
  }
  return false;
}

auto SnippetsTableModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return QVariant();

  if (section == 0)
    return tr("Trigger");
  return tr("Trigger Variant");
}

auto SnippetsTableModel::load(const QString &groupId) -> void
{
  beginResetModel();
  m_activeGroupId = groupId;
  endResetModel();
}

auto SnippetsTableModel::groupIds() const -> QList<QString>
{
  return m_collection->groupIds();
}

auto SnippetsTableModel::createSnippet() -> QModelIndex
{
  const Snippet snippet(m_activeGroupId);
  return insertSnippet(snippet);
}

auto SnippetsTableModel::insertSnippet(const Snippet &snippet) -> QModelIndex
{
  const auto &hint = m_collection->computeInsertionHint(snippet);
  beginInsertRows(QModelIndex(), hint.index(), hint.index());
  m_collection->insertSnippet(snippet, hint);
  endInsertRows();

  return index(hint.index(), 0);
}

auto SnippetsTableModel::removeSnippet(const QModelIndex &modelIndex) -> void
{
  beginRemoveRows(QModelIndex(), modelIndex.row(), modelIndex.row());
  m_collection->removeSnippet(modelIndex.row(), m_activeGroupId);
  endRemoveRows();
}

auto SnippetsTableModel::snippetAt(const QModelIndex &modelIndex) const -> const Snippet&
{
  return m_collection->snippet(modelIndex.row(), m_activeGroupId);
}

auto SnippetsTableModel::setSnippetContent(const QModelIndex &modelIndex, const QString &content) -> void
{
  m_collection->setSnippetContent(modelIndex.row(), m_activeGroupId, content);
}

auto SnippetsTableModel::revertBuitInSnippet(const QModelIndex &modelIndex) -> void
{
  const auto &snippet = m_collection->revertedSnippet(modelIndex.row(), m_activeGroupId);
  if (snippet.id().isEmpty()) {
    QMessageBox::critical(Core::ICore::dialogParent(), tr("Error"), tr("Error reverting snippet."));
    return;
  }
  replaceSnippet(snippet, modelIndex);
}

auto SnippetsTableModel::restoreRemovedBuiltInSnippets() -> void
{
  beginResetModel();
  m_collection->restoreRemovedSnippets(m_activeGroupId);
  endResetModel();
}

auto SnippetsTableModel::resetSnippets() -> void
{
  beginResetModel();
  m_collection->reset(m_activeGroupId);
  endResetModel();
}

auto SnippetsTableModel::replaceSnippet(const Snippet &snippet, const QModelIndex &modelIndex) -> void
{
  const auto row = modelIndex.row();
  const auto &hint = m_collection->computeReplacementHint(row, snippet);
  if (modelIndex.row() == hint.index()) {
    m_collection->replaceSnippet(row, snippet, hint);
    if (modelIndex.column() == 0) emit dataChanged(modelIndex, modelIndex.sibling(row, 1));
    else emit dataChanged(modelIndex.sibling(row, 0), modelIndex);
  } else {
    if (row < hint.index())
      // Rows will be moved down.
      beginMoveRows(QModelIndex(), row, row, QModelIndex(), hint.index() + 1);
    else
      beginMoveRows(QModelIndex(), row, row, QModelIndex(), hint.index());
    m_collection->replaceSnippet(row, snippet, hint);
    endMoveRows();
  }
}

// SnippetsSettingsPagePrivate
class SnippetsSettingsPagePrivate : public QObject {
  Q_DECLARE_TR_FUNCTIONS(TextEditor::Internal::SnippetsSettingsPage)
public:
  SnippetsSettingsPagePrivate();
  ~SnippetsSettingsPagePrivate() override { delete m_model; }

  auto configureUi(QWidget *parent) -> void;

  auto apply() -> void;
  auto finish() -> void;

  QPointer<QWidget> m_widget;

private:
  auto loadSnippetGroup(int index) -> void;
  auto markSnippetsCollection() -> void;
  auto addSnippet() -> void;
  auto removeSnippet() -> void;
  auto revertBuiltInSnippet() -> void;
  auto restoreRemovedBuiltInSnippets() -> void;
  auto resetAllSnippets() -> void;
  auto selectSnippet(const QModelIndex &parent, int row) -> void;
  auto selectMovedSnippet(const QModelIndex &, int, int, const QModelIndex &, int row) -> void;
  auto setSnippetContent() -> void;
  auto updateCurrentSnippetDependent(const QModelIndex &modelIndex = QModelIndex()) -> void;
  auto decorateEditors(const FontSettings &fontSettings) -> void;

  auto currentEditor() const -> SnippetEditorWidget*;
  auto editorAt(int i) const -> SnippetEditorWidget*;

  auto loadSettings() -> void;
  auto settingsChanged() const -> bool;
  auto writeSettings() -> void;

  const QString m_settingsPrefix;
  SnippetsTableModel *m_model;
  bool m_snippetsCollectionChanged;
  SnippetsSettings m_settings;
  Ui::SnippetsSettingsPage m_ui;
};

SnippetsSettingsPagePrivate::SnippetsSettingsPagePrivate() : m_settingsPrefix(QLatin1String("Text")), m_model(new SnippetsTableModel(nullptr)), m_snippetsCollectionChanged(false) {}

auto SnippetsSettingsPagePrivate::currentEditor() const -> SnippetEditorWidget*
{
  return editorAt(m_ui.snippetsEditorStack->currentIndex());
}

auto SnippetsSettingsPagePrivate::editorAt(int i) const -> SnippetEditorWidget*
{
  return static_cast<SnippetEditorWidget*>(m_ui.snippetsEditorStack->widget(i));
}

auto SnippetsSettingsPagePrivate::configureUi(QWidget *w) -> void
{
  m_ui.setupUi(w);

  for (const auto &provider : SnippetProvider::snippetProviders()) {
    m_ui.groupCombo->addItem(provider.displayName(), provider.groupId());
    auto snippetEditor = new SnippetEditorWidget(w);
    SnippetProvider::decorateEditor(snippetEditor, provider.groupId());
    m_ui.snippetsEditorStack->insertWidget(m_ui.groupCombo->count() - 1, snippetEditor);
    connect(snippetEditor, &SnippetEditorWidget::snippetContentChanged, this, &SnippetsSettingsPagePrivate::setSnippetContent);
  }

  m_ui.snippetsTable->setModel(m_model);
  new Utils::HeaderViewStretcher(m_ui.snippetsTable->header(), 1);

  m_ui.revertButton->setEnabled(false);

  loadSettings();
  loadSnippetGroup(m_ui.groupCombo->currentIndex());

  connect(m_model, &QAbstractItemModel::rowsInserted, this, &SnippetsSettingsPagePrivate::selectSnippet);
  connect(m_model, &QAbstractItemModel::rowsInserted, this, &SnippetsSettingsPagePrivate::markSnippetsCollection);
  connect(m_model, &QAbstractItemModel::rowsRemoved, this, &SnippetsSettingsPagePrivate::markSnippetsCollection);
  connect(m_model, &QAbstractItemModel::rowsMoved, this, &SnippetsSettingsPagePrivate::selectMovedSnippet);
  connect(m_model, &QAbstractItemModel::rowsMoved, this, &SnippetsSettingsPagePrivate::markSnippetsCollection);
  connect(m_model, &QAbstractItemModel::dataChanged, this, &SnippetsSettingsPagePrivate::markSnippetsCollection);
  connect(m_model, &QAbstractItemModel::modelReset, this, [this] { this->updateCurrentSnippetDependent(); });
  connect(m_model, &QAbstractItemModel::modelReset, this, &SnippetsSettingsPagePrivate::markSnippetsCollection);

  connect(m_ui.groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnippetsSettingsPagePrivate::loadSnippetGroup);
  connect(m_ui.addButton, &QAbstractButton::clicked, this, &SnippetsSettingsPagePrivate::addSnippet);
  connect(m_ui.removeButton, &QAbstractButton::clicked, this, &SnippetsSettingsPagePrivate::removeSnippet);
  connect(m_ui.resetAllButton, &QAbstractButton::clicked, this, &SnippetsSettingsPagePrivate::resetAllSnippets);
  connect(m_ui.restoreRemovedButton, &QAbstractButton::clicked, this, &SnippetsSettingsPagePrivate::restoreRemovedBuiltInSnippets);
  connect(m_ui.revertButton, &QAbstractButton::clicked, this, &SnippetsSettingsPagePrivate::revertBuiltInSnippet);
  connect(m_ui.snippetsTable->selectionModel(), &QItemSelectionModel::currentChanged, this, &SnippetsSettingsPagePrivate::updateCurrentSnippetDependent);

  connect(TextEditorSettings::instance(), &TextEditorSettings::fontSettingsChanged, this, &SnippetsSettingsPagePrivate::decorateEditors);
}

auto SnippetsSettingsPagePrivate::apply() -> void
{
  if (settingsChanged())
    writeSettings();

  if (currentEditor()->document()->isModified())
    setSnippetContent();

  if (m_snippetsCollectionChanged) {
    QString errorString;
    if (SnippetsCollection::instance()->synchronize(&errorString)) {
      m_snippetsCollectionChanged = false;
    } else {
      QMessageBox::critical(Core::ICore::dialogParent(), tr("Error While Saving Snippet Collection"), errorString);
    }
  }
}

auto SnippetsSettingsPagePrivate::finish() -> void
{
  if (m_snippetsCollectionChanged) {
    SnippetsCollection::instance()->reload();
    m_snippetsCollectionChanged = false;
  }

  disconnect(TextEditorSettings::instance(), nullptr, this, nullptr);
}

auto SnippetsSettingsPagePrivate::loadSettings() -> void
{
  if (m_ui.groupCombo->count() == 0)
    return;

  m_settings.fromSettings(m_settingsPrefix, Core::ICore::settings());
  const auto &lastGroupName = m_settings.lastUsedSnippetGroup();
  const int index = m_ui.groupCombo->findText(lastGroupName);
  if (index != -1)
    m_ui.groupCombo->setCurrentIndex(index);
  else
    m_ui.groupCombo->setCurrentIndex(0);
}

auto SnippetsSettingsPagePrivate::writeSettings() -> void
{
  if (m_ui.groupCombo->count() == 0)
    return;

  m_settings.setLastUsedSnippetGroup(m_ui.groupCombo->currentText());
  m_settings.toSettings(m_settingsPrefix, Core::ICore::settings());
}

auto SnippetsSettingsPagePrivate::settingsChanged() const -> bool
{
  if (m_settings.lastUsedSnippetGroup() != m_ui.groupCombo->currentText())
    return true;
  return false;
}

auto SnippetsSettingsPagePrivate::loadSnippetGroup(int index) -> void
{
  if (index == -1)
    return;

  m_ui.snippetsEditorStack->setCurrentIndex(index);
  currentEditor()->clear();
  m_model->load(m_ui.groupCombo->itemData(index).toString());
}

auto SnippetsSettingsPagePrivate::markSnippetsCollection() -> void
{
  if (!m_snippetsCollectionChanged)
    m_snippetsCollectionChanged = true;
}

auto SnippetsSettingsPagePrivate::addSnippet() -> void
{
  const auto &modelIndex = m_model->createSnippet();
  selectSnippet(QModelIndex(), modelIndex.row());
  m_ui.snippetsTable->edit(modelIndex);
}

auto SnippetsSettingsPagePrivate::removeSnippet() -> void
{
  const QModelIndex &modelIndex = m_ui.snippetsTable->selectionModel()->currentIndex();
  if (!modelIndex.isValid()) {
    QMessageBox::critical(Core::ICore::dialogParent(), tr("Error"), tr("No snippet selected."));
    return;
  }
  m_model->removeSnippet(modelIndex);
}

auto SnippetsSettingsPagePrivate::restoreRemovedBuiltInSnippets() -> void
{
  m_model->restoreRemovedBuiltInSnippets();
}

auto SnippetsSettingsPagePrivate::revertBuiltInSnippet() -> void
{
  m_model->revertBuitInSnippet(m_ui.snippetsTable->selectionModel()->currentIndex());
}

auto SnippetsSettingsPagePrivate::resetAllSnippets() -> void
{
  m_model->resetSnippets();
}

auto SnippetsSettingsPagePrivate::selectSnippet(const QModelIndex &parent, int row) -> void
{
  auto topLeft = m_model->index(row, 0, parent);
  auto bottomRight = m_model->index(row, 1, parent);
  QItemSelection selection(topLeft, bottomRight);
  m_ui.snippetsTable->selectionModel()->select(selection, QItemSelectionModel::SelectCurrent);
  m_ui.snippetsTable->setCurrentIndex(topLeft);
  m_ui.snippetsTable->scrollTo(topLeft);
}

auto SnippetsSettingsPagePrivate::selectMovedSnippet(const QModelIndex &, int sourceRow, int, const QModelIndex &destinationParent, int destinationRow) -> void
{
  QModelIndex modelIndex;
  if (sourceRow < destinationRow)
    modelIndex = m_model->index(destinationRow - 1, 0, destinationParent);
  else
    modelIndex = m_model->index(destinationRow, 0, destinationParent);
  m_ui.snippetsTable->scrollTo(modelIndex);
  currentEditor()->setPlainText(m_model->snippetAt(modelIndex).content());
}

auto SnippetsSettingsPagePrivate::updateCurrentSnippetDependent(const QModelIndex &modelIndex) -> void
{
  if (modelIndex.isValid()) {
    const auto &snippet = m_model->snippetAt(modelIndex);
    currentEditor()->setPlainText(snippet.content());
    m_ui.revertButton->setEnabled(snippet.isBuiltIn());
  } else {
    currentEditor()->clear();
    m_ui.revertButton->setEnabled(false);
  }
}

auto SnippetsSettingsPagePrivate::setSnippetContent() -> void
{
  const QModelIndex &modelIndex = m_ui.snippetsTable->selectionModel()->currentIndex();
  if (modelIndex.isValid()) {
    m_model->setSnippetContent(modelIndex, currentEditor()->toPlainText());
    markSnippetsCollection();
  }
}

auto SnippetsSettingsPagePrivate::decorateEditors(const FontSettings &fontSettings) -> void
{
  for (auto i = 0; i < m_ui.groupCombo->count(); ++i) {
    auto snippetEditor = editorAt(i);
    snippetEditor->textDocument()->setFontSettings(fontSettings);
    const QString &id = m_ui.groupCombo->itemData(i).toString();
    // This list should be quite short... Re-iterating over it is ok.
    SnippetProvider::decorateEditor(snippetEditor, id);
  }
}

// SnippetsSettingsPage
SnippetsSettingsPage::SnippetsSettingsPage() : d(new SnippetsSettingsPagePrivate)
{
  setId(Constants::TEXT_EDITOR_SNIPPETS_SETTINGS);
  setDisplayName(SnippetsSettingsPagePrivate::tr("Snippets"));
  setCategory(Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("TextEditor", "Text Editor"));
  setCategoryIconPath(Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
}

SnippetsSettingsPage::~SnippetsSettingsPage()
{
  delete d;
}

auto SnippetsSettingsPage::widget() -> QWidget*
{
  if (!d->m_widget) {
    d->m_widget = new QWidget;
    d->configureUi(d->m_widget);
  }
  return d->m_widget;
}

auto SnippetsSettingsPage::apply() -> void
{
  d->apply();
}

auto SnippetsSettingsPage::finish() -> void
{
  d->finish();
  delete d->m_widget;
}

} // Internal
} // TextEditor

#include "snippetssettingspage.moc"
