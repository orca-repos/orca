// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-url-locator-filter.hpp"

#include <utils/algorithm.hpp>
#include <utils/stringutils.hpp>

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutexLocker>

using namespace Utils;

namespace Orca::Plugin::Core {

UrlFilterOptions::UrlFilterOptions(UrlLocatorFilter *filter, QWidget *parent) : QDialog(parent), m_filter(filter)
{
  m_ui.setupUi(this);
  setWindowTitle(ILocatorFilter::msgConfigureDialogTitle());

  m_ui.prefixLabel->setText(ILocatorFilter::msgPrefixLabel());
  m_ui.prefixLabel->setToolTip(ILocatorFilter::msgPrefixToolTip());
  m_ui.includeByDefault->setText(ILocatorFilter::msgIncludeByDefault());
  m_ui.includeByDefault->setToolTip(ILocatorFilter::msgIncludeByDefaultToolTip());
  m_ui.shortcutEdit->setText(m_filter->shortcutString());
  m_ui.includeByDefault->setChecked(m_filter->isIncludedByDefault());
  m_ui.nameEdit->setText(filter->displayName());
  m_ui.nameEdit->selectAll();
  m_ui.nameLabel->setVisible(filter->isCustomFilter());
  m_ui.nameEdit->setVisible(filter->isCustomFilter());
  m_ui.listWidget->setToolTip(tr("Add \"%1\" placeholder for the query string.\nDouble-click to edit item."));

  for (const auto remote_urls = m_filter->remoteUrls(); const auto &url : remote_urls) {
    const auto item = new QListWidgetItem(url);
    m_ui.listWidget->addItem(item);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
  }

  connect(m_ui.add, &QPushButton::clicked, this, &UrlFilterOptions::addNewItem);
  connect(m_ui.remove, &QPushButton::clicked, this, &UrlFilterOptions::removeItem);
  connect(m_ui.moveUp, &QPushButton::clicked, this, &UrlFilterOptions::moveItemUp);
  connect(m_ui.moveDown, &QPushButton::clicked, this, &UrlFilterOptions::moveItemDown);
  connect(m_ui.listWidget, &QListWidget::currentItemChanged, this, &UrlFilterOptions::updateActionButtons);

  updateActionButtons();
}

auto UrlFilterOptions::addNewItem() const -> void
{
  const auto item = new QListWidgetItem("https://www.example.com/search?query=%1");
  m_ui.listWidget->addItem(item);
  item->setSelected(true);
  item->setFlags(item->flags() | Qt::ItemIsEditable);
  m_ui.listWidget->setCurrentItem(item);
  m_ui.listWidget->editItem(item);
}

auto UrlFilterOptions::removeItem() const -> void
{
  if (const auto item = m_ui.listWidget->currentItem()) {
    m_ui.listWidget->removeItemWidget(item);
    delete item;
  }
}

auto UrlFilterOptions::moveItemUp() const -> void
{
  if (const auto row = m_ui.listWidget->currentRow(); row > 0) {
    const auto item = m_ui.listWidget->takeItem(row);
    m_ui.listWidget->insertItem(row - 1, item);
    m_ui.listWidget->setCurrentRow(row - 1);
  }
}

auto UrlFilterOptions::moveItemDown() const -> void
{
  if (const auto row = m_ui.listWidget->currentRow(); row >= 0 && row < m_ui.listWidget->count() - 1) {
    const auto item = m_ui.listWidget->takeItem(row);
    m_ui.listWidget->insertItem(row + 1, item);
    m_ui.listWidget->setCurrentRow(row + 1);
  }
}

auto UrlFilterOptions::updateActionButtons() const -> void
{
  m_ui.remove->setEnabled(m_ui.listWidget->currentItem());
  const auto row = m_ui.listWidget->currentRow();
  m_ui.moveUp->setEnabled(row > 0);
  m_ui.moveDown->setEnabled(row >= 0 && row < m_ui.listWidget->count() - 1);
}

// -- UrlLocatorFilter

/*!
    \class Core::UrlLocatorFilter
    \inmodule Orca
    \internal
*/

UrlLocatorFilter::UrlLocatorFilter(const Id id) : UrlLocatorFilter(tr("URL Template"), id) {}

UrlLocatorFilter::UrlLocatorFilter(const QString &display_name, const Id id)
{
  setId(id);
  m_default_display_name = display_name;
  setDisplayName(display_name);
  setDefaultIncludedByDefault(false);
}

UrlLocatorFilter::~UrlLocatorFilter() = default;

auto UrlLocatorFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> entries;

  for (const auto urls = remoteUrls(); const auto &url : urls) {
    if (future.isCanceled())
      break;
    const auto name = url.arg(entry);
    LocatorFilterEntry filter_entry(this, name, QVariant());
    filter_entry.highlight_info = {static_cast<int>(name.lastIndexOf(entry)), static_cast<int>(entry.length())};
    entries.append(filter_entry);
  }

  return entries;
}

auto UrlLocatorFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(new_text)
  Q_UNUSED(selection_start)
  Q_UNUSED(selection_length)

  if (const auto &url = selection.display_name; !url.isEmpty())
    QDesktopServices::openUrl(url);
}

constexpr char k_display_name_key[] = "displayName";
constexpr char k_remote_urls_key[] = "remoteUrls";

auto UrlLocatorFilter::saveState(QJsonObject &object) const -> void
{
  if (displayName() != m_default_display_name)
    object.insert(k_display_name_key, displayName());

  if (m_remote_urls != m_default_urls)
    object.insert(k_remote_urls_key, QJsonArray::fromStringList(m_remote_urls));
}

auto UrlLocatorFilter::restoreState(const QJsonObject &object) -> void
{
  setDisplayName(object.value(k_display_name_key).toString(m_default_display_name));
  m_remote_urls = transform(object.value(k_remote_urls_key).toArray(QJsonArray::fromStringList(m_default_urls)).toVariantList(), &QVariant::toString);
}

auto UrlLocatorFilter::restoreState(const QByteArray &state) -> void
{
  if (isOldSetting(state)) {
    // TODO read old settings, remove some time after Qt Creator 4.15
    QDataStream in(state);

    QString value;
    in >> value;
    m_remote_urls = value.split('^', Qt::SkipEmptyParts);

    QString shortcut;
    in >> shortcut;
    setShortcutString(shortcut);

    bool default_filter;
    in >> default_filter;
    setIncludedByDefault(default_filter);

    if (!in.atEnd()) {
      QString name;
      in >> name;
      setDisplayName(name);
    }
  } else {
    ILocatorFilter::restoreState(state);
  }
}

auto UrlLocatorFilter::openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool
{
  Q_UNUSED(needs_refresh)

  if (UrlFilterOptions options_dialog(this, parent); options_dialog.exec() == QDialog::Accepted) {
    QMutexLocker lock(&m_mutex);
    m_remote_urls.clear();
    setIncludedByDefault(options_dialog.m_ui.includeByDefault->isChecked());
    setShortcutString(options_dialog.m_ui.shortcutEdit->text().trimmed());

    for (auto i = 0; i < options_dialog.m_ui.listWidget->count(); ++i)
      m_remote_urls.append(options_dialog.m_ui.listWidget->item(i)->text());

    if (isCustomFilter())
      setDisplayName(options_dialog.m_ui.nameEdit->text());

    return true;
  }

  return true;
}

auto UrlLocatorFilter::addDefaultUrl(const QString &url_template) -> void
{
  m_remote_urls.append(url_template);
  m_default_urls.append(url_template);
}

auto UrlLocatorFilter::remoteUrls() const -> QStringList
{
  QMutexLocker lock(&m_mutex);
  return m_remote_urls;
}

auto UrlLocatorFilter::setIsCustomFilter(const bool value) -> void
{
  m_is_custom_filter = value;
}

auto UrlLocatorFilter::isCustomFilter() const -> bool
{
  return m_is_custom_filter;
}

} // namespace Orca::Plugin::Core
