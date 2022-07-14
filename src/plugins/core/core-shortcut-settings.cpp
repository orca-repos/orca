// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-shortcut-settings.hpp"

#include "core-action-manager.hpp"
#include "core-command.hpp"
#include "core-commands-file.hpp"
#include "core-constants.hpp"
#include "core-document-manager.hpp"
#include "core-interface.hpp"

#include <utils/algorithm.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>
#include <utils/theme/theme.hpp>

#include <QAction>
#include <QApplication>
#include <QApplication>
#include <QDebug>
#include <QDebug>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidgetItem>

using namespace Utils;

Q_DECLARE_METATYPE(Orca::Plugin::Core::ShortcutItem*)

namespace Orca::Plugin::Core {

constexpr char k_separator[] = " | ";

static auto translateModifiers(const Qt::KeyboardModifiers state, const QString &text) -> int
{
  auto result = 0;

  // The shift modifier only counts when it is not used to type a symbol
  // that is only reachable using the shift key anyway
  if (state & Qt::ShiftModifier && (text.isEmpty() || !text.at(0).isPrint() || text.at(0).isLetterOrNumber() || text.at(0).isSpace()))
    result |= Qt::SHIFT;
  if (state & Qt::ControlModifier)
    result |= Qt::CTRL;
  if (state & Qt::MetaModifier)
    result |= Qt::META;
  if (state & Qt::AltModifier)
    result |= Qt::ALT;

  return result;
}

static auto cleanKeys(const QList<QKeySequence> &ks) -> QList<QKeySequence>
{
  return filtered(ks, [](const QKeySequence &k) { return !k.isEmpty(); });
}

static auto keySequenceToEditString(const QKeySequence &sequence) -> QString
{
  auto text = sequence.toString(QKeySequence::PortableText);

  if constexpr (HostOsInfo::isMacHost()) {
    // adapt the modifier names
    text.replace(QLatin1String("Ctrl"), QLatin1String("Cmd"), Qt::CaseInsensitive);
    text.replace(QLatin1String("Alt"), QLatin1String("Opt"), Qt::CaseInsensitive);
    text.replace(QLatin1String("Meta"), QLatin1String("Ctrl"), Qt::CaseInsensitive);
  }

  return text;
}

static auto keySequencesToEditString(const QList<QKeySequence> &sequence) -> QString
{
  return transform(cleanKeys(sequence), keySequenceToEditString).join(k_separator);
}

static auto keySequencesToNativeString(const QList<QKeySequence> &sequence) -> QString
{
  return transform(cleanKeys(sequence), [](const QKeySequence &k) {
    return k.toString(QKeySequence::NativeText);
  }).join(k_separator);
}

static auto keySequenceFromEditString(const QString &editString) -> QKeySequence
{
  auto text = editString.trimmed();

  if constexpr (HostOsInfo::isMacHost()) {
    // adapt the modifier names
    text.replace(QLatin1String("Opt"), QLatin1String("Alt"), Qt::CaseInsensitive);
    text.replace(QLatin1String("Ctrl"), QLatin1String("Meta"), Qt::CaseInsensitive);
    text.replace(QLatin1String("Cmd"), QLatin1String("Ctrl"), Qt::CaseInsensitive);
  }

  return QKeySequence::fromString(text, QKeySequence::PortableText);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using key_combination = int;
#else
using key_combination = QKeyCombination;
#endif

static auto keySequenceIsValid(const QKeySequence &sequence) -> bool
{
  if (sequence.isEmpty())
    return false;

  for (auto i = 0; i < sequence.count(); ++i) {
    if (sequence[i] == key_combination(Qt::Key_unknown))
      return false;
  }

  return true;
}

static auto isTextKeySequence(const QKeySequence &sequence) -> bool
{
  if (sequence.isEmpty())
    return false;

  int key = sequence[0];
  key &= ~(Qt::ShiftModifier | Qt::KeypadModifier);

  if (key < Qt::Key_Escape)
    return true;

  return false;
}

static auto schemesPath() -> FilePath
{
  return ICore::resourcePath("schemes");
}

ShortcutButton::ShortcutButton(QWidget *parent) : QPushButton(parent), m_key({{0, 0, 0, 0}})
{
  // Using ShortcutButton::tr() as workaround for QTBUG-34128
  setToolTip(tr("Click and type the new key sequence."));
  setCheckable(true);
  m_checked_text = tr("Stop Recording");
  m_unchecked_text = tr("Record");
  updateText();
  connect(this, &ShortcutButton::toggled, this, &ShortcutButton::handleToggleChange);
}

auto ShortcutButton::sizeHint() const -> QSize
{
  if (m_preferred_width < 0) {
    // initialize size hint
    const auto original_text = text();
    const auto that = const_cast<ShortcutButton*>(this);

    that->setText(m_checked_text);
    m_preferred_width = QPushButton::sizeHint().width();
    that->setText(m_unchecked_text);

    if (const auto other_width = QPushButton::sizeHint().width(); other_width > m_preferred_width)
      m_preferred_width = other_width;

    that->setText(original_text);
  }
  return {m_preferred_width, QPushButton::sizeHint().height()};
}

auto ShortcutButton::eventFilter(QObject *obj, QEvent *evt) -> bool
{
  if (evt->type() == QEvent::ShortcutOverride) {
    evt->accept();
    return true;
  }
  if (evt->type() == QEvent::KeyRelease || evt->type() == QEvent::Shortcut || evt->type() == QEvent::Close/*Escape tries to close dialog*/) {
    return true;
  }
  if (evt->type() == QEvent::MouseButtonPress && isChecked()) {
    setChecked(false);
    return true;
  }
  if (evt->type() == QEvent::KeyPress) {
    const auto k = dynamic_cast<QKeyEvent*>(evt);
    auto next_key = k->key();
    if (m_key_num > 3 || next_key == Qt::Key_Control || next_key == Qt::Key_Shift || next_key == Qt::Key_Meta || next_key == Qt::Key_Alt) {
      return false;
    }

    next_key |= translateModifiers(k->modifiers(), k->text());
    switch (m_key_num) {
    case 0:
      m_key[0] = next_key;
      break;
    case 1:
      m_key[1] = next_key;
      break;
    case 2:
      m_key[2] = next_key;
      break;
    case 3:
      m_key[3] = next_key;
      break;
    default:
      break;
    }

    m_key_num++;
    k->accept();
    emit keySequenceChanged(QKeySequence(m_key[0], m_key[1], m_key[2], m_key[3]));

    if (m_key_num > 3)
      setChecked(false);

    return true;
  }
  return QPushButton::eventFilter(obj, evt);
}

auto ShortcutButton::updateText() -> void
{
  setText(isChecked() ? m_checked_text : m_unchecked_text);
}

auto ShortcutButton::handleToggleChange(const bool toggle_state) -> void
{
  updateText();
  m_key_num = m_key[0] = m_key[1] = m_key[2] = m_key[3] = 0;

  if (toggle_state) {
    if (QApplication::focusWidget())
      QApplication::focusWidget()->clearFocus(); // funny things happen otherwise
    qApp->installEventFilter(this);
  } else {
    qApp->removeEventFilter(this);
  }
}

class ShortcutSettingsWidget final : public CommandMappings {
  Q_DECLARE_TR_FUNCTIONS(Core::ShortcutSettings)
  Q_DISABLE_COPY_MOVE(ShortcutSettingsWidget)

public:
  ShortcutSettingsWidget();
  ~ShortcutSettingsWidget() override;

  auto apply() -> void;

private:
  auto importAction() -> void override;
  auto exportAction() -> void override;
  auto defaultAction() -> void override;
  auto filterColumn(const QString &filter_string, QTreeWidgetItem *item, int column) const -> bool override;
  auto initialize() -> void;
  auto handleCurrentCommandChanged(const QTreeWidgetItem *current) -> void;
  auto resetToDefault() -> void;
  auto updateAndCheckForConflicts(const QKeySequence &key, int index) const -> bool;
  auto markCollisions(const ShortcutItem *, int index) -> bool;
  auto markAllCollisions() -> void;
  auto showConflicts() const -> void;
  auto clear() -> void;
  auto setupShortcutBox(const ShortcutItem *scitem) -> void;

  QList<ShortcutItem*> m_scitems;
  QGroupBox *m_shortcut_box;
  QGridLayout *m_shortcut_layout;
  std::vector<std::unique_ptr<ShortcutInput>> m_shortcut_inputs;
  QPointer<QPushButton> m_add_button = nullptr;
};

ShortcutSettingsWidget::ShortcutSettingsWidget()
{
  setPageTitle(tr("Keyboard Shortcuts"));
  setTargetHeader(tr("Shortcut"));
  setResetVisible(true);

  connect(ActionManager::instance(), &ActionManager::commandListChanged, this, &ShortcutSettingsWidget::initialize);
  connect(this, &ShortcutSettingsWidget::currentCommandChanged, this, &ShortcutSettingsWidget::handleCurrentCommandChanged);
  connect(this, &ShortcutSettingsWidget::resetRequested, this, &ShortcutSettingsWidget::resetToDefault);

  m_shortcut_box = new QGroupBox(tr("Shortcut"), this);
  m_shortcut_box->setEnabled(false);
  m_shortcut_layout = new QGridLayout(m_shortcut_box);
  m_shortcut_box->setLayout(m_shortcut_layout);

  layout()->addWidget(m_shortcut_box);
  initialize();
}

ShortcutSettingsWidget::~ShortcutSettingsWidget()
{
  qDeleteAll(m_scitems);
}

ShortcutSettings::ShortcutSettings()
{
  setId(SETTINGS_ID_SHORTCUTS);
  setDisplayName(ShortcutSettingsWidget::tr("Keyboard"));
  setCategory(SETTINGS_CATEGORY_CORE);
}

auto ShortcutSettings::widget() -> QWidget*
{
  if (!m_widget)
    m_widget = new ShortcutSettingsWidget();

  return m_widget;
}

auto ShortcutSettingsWidget::apply() -> void
{
  foreach(ShortcutItem *item, m_scitems)
    item->m_cmd->setKeySequences(item->m_keys);
}

auto ShortcutSettings::apply() -> void
{
  QTC_ASSERT(m_widget, return);
  m_widget->apply();
}

auto ShortcutSettings::finish() -> void
{
  delete m_widget;
}

auto shortcutItem(const QTreeWidgetItem *tree_item) -> ShortcutItem*
{
  if (!tree_item)
    return nullptr;

  return tree_item->data(0, Qt::UserRole).value<ShortcutItem*>();
}

auto ShortcutSettingsWidget::handleCurrentCommandChanged(const QTreeWidgetItem *current) -> void
{
  if (const auto scitem = shortcutItem(current); !scitem) {
    m_shortcut_inputs.clear();
    delete m_add_button;
    m_shortcut_box->setEnabled(false);
  } else {
    // clean up before showing UI
    scitem->m_keys = cleanKeys(scitem->m_keys);
    setupShortcutBox(scitem);
    m_shortcut_box->setEnabled(true);
  }
}

auto ShortcutSettingsWidget::setupShortcutBox(const ShortcutItem *scitem) -> void
{
  const auto update_add_button = [this] {
    m_add_button->setEnabled(!anyOf(m_shortcut_inputs, [](const std::unique_ptr<ShortcutInput> &i) {
      return i->keySequence().isEmpty();
    }));
  };

  const auto add_shortcut_input = [this, update_add_button](int index, const QKeySequence &key) {
    auto input = std::make_unique<ShortcutInput>();
    input->addToLayout(m_shortcut_layout, index * 2);
    input->setConflictChecker([this, index](const QKeySequence &k) { return updateAndCheckForConflicts(k, index); });
    connect(input.get(), &ShortcutInput::showConflictsRequested, this, &ShortcutSettingsWidget::showConflicts);
    connect(input.get(), &ShortcutInput::changed, this, update_add_button);
    input->setKeySequence(key);
    m_shortcut_inputs.push_back(std::move(input));
  };

  const auto add_button_to_layout = [this, update_add_button] {
    m_shortcut_layout->addWidget(m_add_button, static_cast<int>(m_shortcut_inputs.size() * 2 - 1), m_shortcut_layout->columnCount() - 1);
    update_add_button();
  };

  m_shortcut_inputs.clear();
  delete m_add_button;
  m_add_button = new QPushButton(tr("Add"), this);

  for (auto i = 0; i < qMax(1, scitem->m_keys.size()); ++i)
    add_shortcut_input(i, scitem->m_keys.value(i));

  connect(m_add_button, &QPushButton::clicked, this, [this, add_shortcut_input, add_button_to_layout] {
    add_shortcut_input(static_cast<int>(m_shortcut_inputs.size()), {});
    add_button_to_layout();
  });

  add_button_to_layout();
  update_add_button();
}

static auto checkValidity(const QKeySequence &key, QString *warning_message) -> bool
{
  if (key.isEmpty())
    return true;

  QTC_ASSERT(warning_message, return true);

  if (!keySequenceIsValid(key)) {
    *warning_message = ShortcutSettingsWidget::tr("Invalid key sequence.");
    return false;
  }

  if (isTextKeySequence(key))
    *warning_message = ShortcutSettingsWidget::tr("Key sequence will not work in editor.");

  return true;
}

auto ShortcutSettingsWidget::updateAndCheckForConflicts(const QKeySequence &key, const int index) const -> bool
{
  const auto current = commandList()->currentItem();
  const auto item = shortcutItem(current);

  if (!item)
    return false;

  while (index >= item->m_keys.size())
    item->m_keys.append(QKeySequence());

  item->m_keys[index] = key;
  setModified(current, cleanKeys(item->m_keys) != item->m_cmd->defaultKeySequences());
  current->setText(2, keySequencesToNativeString(item->m_keys));

  const auto that = const_cast<ShortcutSettingsWidget*>(this);
  return that->markCollisions(item, index);
}

auto ShortcutSettingsWidget::filterColumn(const QString &filter_string, QTreeWidgetItem *item, const int column) const -> bool
{
  const ShortcutItem *scitem = shortcutItem(item);

  if (column == item->columnCount() - 1) {
    // shortcut
    // filter on the shortcut edit text
    if (!scitem)
      return true;

    const auto filters = transform(filter_string.split(k_separator), [](const QString &s) { return s.trimmed(); });
    for (const auto &k : scitem->m_keys) {
      if (const auto &key_string = keySequenceToEditString(k); anyOf(filters, [key_string](const QString &f) {
        return key_string.contains(f, Qt::CaseInsensitive);
      }))
      return false;
    }

    return true;
  }

  QString text;

  if (column == 0 && scitem) {
    // command id
    text = scitem->m_cmd->id().toString();
  } else {
    text = item->text(column);
  }

  return !text.contains(filter_string, Qt::CaseInsensitive);
}

auto ShortcutSettingsWidget::showConflicts() const -> void
{
  const auto current = commandList()->currentItem();
  if (const auto scitem = shortcutItem(current))
    setFilterText(keySequencesToEditString(scitem->m_keys));
}

auto ShortcutSettingsWidget::resetToDefault() -> void
{
  const auto current = commandList()->currentItem();
  if (const auto scitem = shortcutItem(current)) {
    scitem->m_keys = scitem->m_cmd->defaultKeySequences();
    current->setText(2, keySequencesToNativeString(scitem->m_keys));
    setModified(current, false);
    setupShortcutBox(scitem);
    markAllCollisions();
  }
}

auto ShortcutSettingsWidget::importAction() -> void
{
  if (const auto file_name = FileUtils::getOpenFilePath(nullptr, tr("Import Keyboard Mapping Scheme"), schemesPath(), tr("Keyboard Mapping Scheme (*.kms)")); !file_name.isEmpty()) {
    const CommandsFile cf(file_name);
    const auto mapping = cf.importCommands();
    for (const auto item : qAsConst(m_scitems)) {
      if (auto sid = item->m_cmd->id().toString(); mapping.contains(sid)) {
        item->m_keys = mapping.value(sid);
        item->m_item->setText(2, keySequencesToNativeString(item->m_keys));
        if (item->m_item == commandList()->currentItem())
          emit currentCommandChanged(item->m_item);
        if (item->m_keys != item->m_cmd->defaultKeySequences())
          setModified(item->m_item, true);
        else
          setModified(item->m_item, false);
      }
    }
    markAllCollisions();
  }
}

auto ShortcutSettingsWidget::defaultAction() -> void
{
  for (const auto item : qAsConst(m_scitems)) {
    item->m_keys = item->m_cmd->defaultKeySequences();
    item->m_item->setText(2, keySequencesToNativeString(item->m_keys));
    setModified(item->m_item, false);
    if (item->m_item == commandList()->currentItem()) emit currentCommandChanged(item->m_item);
  }
  markAllCollisions();
}

auto ShortcutSettingsWidget::exportAction() -> void
{
  if (const auto file_path = DocumentManager::getSaveFileNameWithExtension(tr("Export Keyboard Mapping Scheme"), schemesPath(), tr("Keyboard Mapping Scheme (*.kms)")); !file_path.isEmpty()) {
    const CommandsFile cf(file_path);
    cf.exportCommands(m_scitems);
  }
}

auto ShortcutSettingsWidget::clear() -> void
{
  const auto tree = commandList();

  for (auto i = tree->topLevelItemCount() - 1; i >= 0; --i) {
    delete tree->takeTopLevelItem(i);
  }

  qDeleteAll(m_scitems);
  m_scitems.clear();
}

auto ShortcutSettingsWidget::initialize() -> void
{
  clear();
  QMap<QString, QTreeWidgetItem*> sections;

  for (const auto commands = ActionManager::commands(); const auto c : commands) {
    if (c->hasAttribute(Command::CA_NonConfigurable))
      continue;
    if (c->action() && c->action()->isSeparator())
      continue;

    QTreeWidgetItem *item = nullptr;
    auto s = new ShortcutItem;
    m_scitems << s;
    item = new QTreeWidgetItem;
    s->m_cmd = c;
    s->m_item = item;

    const auto identifier = c->id().toString();
    const auto pos = identifier.indexOf(QLatin1Char('.'));
    const auto section = identifier.left(pos);
    const auto sub_id = identifier.mid(pos + 1);

    if (!sections.contains(section)) {
      auto category_item = new QTreeWidgetItem(commandList(), QStringList(section));
      auto f = category_item->font(0);
      f.setBold(true);
      category_item->setFont(0, f);
      sections.insert(section, category_item);
      commandList()->expandItem(category_item);
    }

    sections[section]->addChild(item);
    s->m_keys = c->keySequences();
    item->setText(0, sub_id);
    item->setText(1, c->description());
    item->setText(2, keySequencesToNativeString(s->m_keys));

    if (s->m_keys != s->m_cmd->defaultKeySequences())
      setModified(item, true);

    item->setData(0, Qt::UserRole, QVariant::fromValue(s));
  }

  markAllCollisions();
  filterChanged(filterText());
}

auto ShortcutSettingsWidget::markCollisions(const ShortcutItem *item, const int index) -> bool
{
  auto has_collision = false;

  if (const auto key = item->m_keys.value(index); !key.isEmpty()) {
    const Id global_id(C_GLOBAL);
    const auto item_context = item->m_cmd->context();
    const auto item_has_global_context = item_context.contains(global_id);

    for (const auto current_item : qAsConst(m_scitems)) {
      if (item == current_item)
        continue;
      if (!anyOf(current_item->m_keys, equalTo(key)))
        continue;

      // check if contexts might conflict
      const auto current_context = current_item->m_cmd->context();
      auto current_is_conflicting = item_has_global_context && !current_context.isEmpty();

      if (!current_is_conflicting) {
        for (const auto &id : current_context) {
          if (id == global_id && !item_context.isEmpty() || item_context.contains(id)) {
            current_is_conflicting = true;
            break;
          }
        }
      }

      if (current_is_conflicting) {
        current_item->m_item->setForeground(2, orcaTheme()->color(Theme::TextColorError));
        has_collision = true;
      }
    }
  }

  item->m_item->setForeground(2, has_collision ? orcaTheme()->color(Theme::TextColorError) : commandList()->palette().windowText());
  return has_collision;
}

auto ShortcutSettingsWidget::markAllCollisions() -> void
{
  for (const auto item : qAsConst(m_scitems))
    for (auto i = 0; i < item->m_keys.size(); ++i)
      markCollisions(item, i);
}

ShortcutInput::ShortcutInput()
{
  m_shortcut_label = new QLabel(tr("Key sequence:"));
  if constexpr (HostOsInfo::isMacHost())
    m_shortcut_label->setToolTip(QLatin1String("<html><body>") + tr("Use \"Cmd\", \"Opt\", \"Ctrl\", and \"Shift\" for modifier keys. " "Use \"Escape\", \"Backspace\", \"Delete\", \"Insert\", \"Home\", and so " "on, for special keys. " "Combine individual keys with \"+\", " "and combine multiple shortcuts to a shortcut sequence with \",\". " "For example, if the user must hold the Ctrl and Shift modifier keys " "while pressing Escape, and then release and press A, " "enter \"Ctrl+Shift+Escape,A\".") + QLatin1String("</body></html>"));
  else
    m_shortcut_label->setToolTip(QLatin1String("<html><body>") + tr("Use \"Ctrl\", \"Alt\", \"Meta\", and \"Shift\" for modifier keys. " "Use \"Escape\", \"Backspace\", \"Delete\", \"Insert\", \"Home\", and so " "on, for special keys. " "Combine individual keys with \"+\", " "and combine multiple shortcuts to a shortcut sequence with \",\". " "For example, if the user must hold the Ctrl and Shift modifier keys " "while pressing Escape, and then release and press A, " "enter \"Ctrl+Shift+Escape,A\".") + QLatin1String("</body></html>"));

  m_shortcut_edit = new FancyLineEdit;
  m_shortcut_edit->setFiltering(true);
  m_shortcut_edit->setPlaceholderText(tr("Enter key sequence as text"));
  connect(m_shortcut_edit, &FancyLineEdit::textChanged, this, &ShortcutInput::changed);

  m_shortcut_button = new ShortcutButton;
  connect(m_shortcut_button, &ShortcutButton::keySequenceChanged, this, [this](const QKeySequence &k) { setKeySequence(k); });

  m_warning_label = new QLabel;
  m_warning_label->setTextFormat(Qt::RichText);

  auto palette = m_warning_label->palette();
  palette.setColor(QPalette::Active, QPalette::WindowText, orcaTheme()->color(Theme::TextColorError));

  m_warning_label->setPalette(palette);
  connect(m_warning_label, &QLabel::linkActivated, this, &ShortcutInput::showConflictsRequested);

  m_shortcut_edit->setValidationFunction([this](FancyLineEdit *, QString *) {
    QString warning_message;
    const auto key = keySequenceFromEditString(m_shortcut_edit->text());
    const auto is_valid = checkValidity(key, &warning_message);
    m_warning_label->setText(warning_message);
    if (is_valid && m_conflict_checker && m_conflict_checker(key)) {
      m_warning_label->setText(ShortcutSettingsWidget::tr("Key sequence has potential conflicts. <a href=\"#conflicts\">Show.</a>"));
    }
    return is_valid;
  });
}

ShortcutInput::~ShortcutInput()
{
  delete m_shortcut_label;
  delete m_shortcut_edit;
  delete m_shortcut_button;
  delete m_warning_label;
}

auto ShortcutInput::addToLayout(QGridLayout *layout, const int row) const -> void
{
  layout->addWidget(m_shortcut_label, row, 0);
  layout->addWidget(m_shortcut_edit, row, 1);
  layout->addWidget(m_shortcut_button, row, 2);
  layout->addWidget(m_warning_label, row + 1, 0, 1, 2);
}

auto ShortcutInput::setKeySequence(const QKeySequence &key) const -> void
{
  m_shortcut_edit->setText(keySequenceToEditString(key));
}

auto ShortcutInput::keySequence() const -> QKeySequence
{
  return keySequenceFromEditString(m_shortcut_edit->text());
}

auto ShortcutInput::setConflictChecker(const conflict_checker &fun) -> void
{
  m_conflict_checker = fun;
}

} // namespace Orca::Plugin::Core
