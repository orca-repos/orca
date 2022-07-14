// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-command-mappings.hpp"

#include "core-commands-file.hpp"
#include "core-shortcut-settings.hpp"

#include <utils/fancylineedit.hpp>
#include <utils/headerviewstretcher.hpp>

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidgetItem>

Q_DECLARE_METATYPE(Orca::Plugin::Core::ShortcutItem*)

using namespace Utils;

namespace Orca::Plugin::Core {

class CommandMappingsPrivate {
public:
  explicit CommandMappingsPrivate(CommandMappings *parent) : command_mappings(parent)
  {
    group_box = new QGroupBox(parent);
    group_box->setTitle(CommandMappings::tr("Command Mappings"));

    filter_edit = new FancyLineEdit(group_box);
    filter_edit->setFiltering(true);

    command_list = new QTreeWidget(group_box);
    command_list->setRootIsDecorated(false);
    command_list->setUniformRowHeights(true);
    command_list->setSortingEnabled(true);
    command_list->setColumnCount(3);

    const auto item = command_list->headerItem();
    item->setText(2, CommandMappings::tr("Target"));
    item->setText(1, CommandMappings::tr("Label"));
    item->setText(0, CommandMappings::tr("Command"));

    default_button = new QPushButton(CommandMappings::tr("Reset All"), group_box);
    default_button->setToolTip(CommandMappings::tr("Reset all to default."));

    reset_button = new QPushButton(CommandMappings::tr("Reset"), group_box);
    reset_button->setToolTip(CommandMappings::tr("Reset to default."));
    reset_button->setVisible(false);

    import_button = new QPushButton(CommandMappings::tr("Import..."), group_box);
    export_button = new QPushButton(CommandMappings::tr("Export..."), group_box);

    const auto hbox_layout1 = new QHBoxLayout();
    hbox_layout1->addWidget(default_button);
    hbox_layout1->addWidget(reset_button);
    hbox_layout1->addStretch();
    hbox_layout1->addWidget(import_button);
    hbox_layout1->addWidget(export_button);

    const auto hbox_layout = new QHBoxLayout();
    hbox_layout->addWidget(filter_edit);

    const auto vbox_layout1 = new QVBoxLayout(group_box);
    vbox_layout1->addLayout(hbox_layout);
    vbox_layout1->addWidget(command_list);
    vbox_layout1->addLayout(hbox_layout1);

    const auto vbox_layout = new QVBoxLayout(parent);
    vbox_layout->addWidget(group_box);

    CommandMappings::connect(export_button, &QPushButton::clicked, command_mappings, &CommandMappings::exportAction);
    CommandMappings::connect(import_button, &QPushButton::clicked, command_mappings, &CommandMappings::importAction);
    CommandMappings::connect(default_button, &QPushButton::clicked, command_mappings, &CommandMappings::defaultAction);
    CommandMappings::connect(reset_button, &QPushButton::clicked, command_mappings, &CommandMappings::resetRequested);

    command_list->sortByColumn(0, Qt::AscendingOrder);

    CommandMappings::connect(filter_edit, &FancyLineEdit::textChanged, command_mappings, &CommandMappings::filterChanged);
    CommandMappings::connect(command_list, &QTreeWidget::currentItemChanged, command_mappings, &CommandMappings::currentCommandChanged);

    new HeaderViewStretcher(command_list->header(), 1);
  }

  CommandMappings *command_mappings;

  QGroupBox *group_box;
  FancyLineEdit *filter_edit;
  QTreeWidget *command_list;
  QPushButton *default_button;
  QPushButton *reset_button;
  QPushButton *import_button;
  QPushButton *export_button;
};

/*!
    \class Core::CommandMappings
    \inmodule Orca
    \internal
*/

CommandMappings::CommandMappings(QWidget *parent) : QWidget(parent), d(new CommandMappingsPrivate(this)) {}

CommandMappings::~CommandMappings()
{
  delete d;
}

auto CommandMappings::setImportExportEnabled(const bool enabled) const -> void
{
  d->import_button->setVisible(enabled);
  d->export_button->setVisible(enabled);
}

auto CommandMappings::setResetVisible(const bool visible) const -> void
{
  d->reset_button->setVisible(visible);
}

auto CommandMappings::commandList() const -> QTreeWidget*
{
  return d->command_list;
}

auto CommandMappings::setPageTitle(const QString &s) const -> void
{
  d->group_box->setTitle(s);
}

auto CommandMappings::setTargetHeader(const QString &s) const -> void
{
  d->command_list->setHeaderLabels({tr("Command"), tr("Label"), s});
}

auto CommandMappings::filterChanged(const QString &f) -> void
{
  for (auto i = 0; i < d->command_list->topLevelItemCount(); ++i) {
    const auto item = d->command_list->topLevelItem(i);
    filter(f, item);
  }
}

auto CommandMappings::filter(const QString &filter_string, QTreeWidgetItem *item) -> bool
{
  auto visible = filter_string.isEmpty();
  const auto column_count = item->columnCount();

  for (auto i = 0; !visible && i < column_count; ++i)
    visible |= !filterColumn(filter_string, item, i);

  if (const auto child_count = item->childCount(); child_count > 0) {
    // force visibility if this item matches
    const auto leaf_filter_string = visible ? QString() : filter_string;
    for (auto i = 0; i < child_count; ++i) {
      const auto citem = item->child(i);
      visible |= !filter(leaf_filter_string, citem); // parent visible if any child visible
    }
  }

  item->setHidden(!visible);
  return !visible;
}

auto CommandMappings::filterColumn(const QString &filter_string, QTreeWidgetItem *item, const int column) const -> bool
{
  return !item->text(column).contains(filter_string, Qt::CaseInsensitive);
}

auto CommandMappings::setModified(QTreeWidgetItem *item, const bool modified) -> void
{
  auto f = item->font(0);
  f.setItalic(modified);
  item->setFont(0, f);
  item->setFont(1, f);
  f.setBold(modified);
  item->setFont(2, f);
}

auto CommandMappings::filterText() const -> QString
{
  return d->filter_edit->text();
}

auto CommandMappings::setFilterText(const QString &text) const -> void
{
  d->filter_edit->setText(text);
}

} // namespace Orca::Plugin::Core
