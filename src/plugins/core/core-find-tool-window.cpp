// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-find-tool-window.hpp"

#include "core-find-filter-interface.hpp"
#include "core-find-plugin.hpp"
#include "core-interface.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <QCompleter>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStringListModel>

namespace Orca::Plugin::Core {

static FindToolWindow *m_instance = nullptr;

static auto validateRegExp(const Utils::FancyLineEdit *edit, QString *error_message) -> bool
{
  if (edit->text().isEmpty()) {
    if (error_message)
      *error_message = FindToolWindow::tr("Empty search term.");
    return false;
  }

  if (Find::hasFindFlag(FindRegularExpression)) {
    const QRegularExpression regexp(edit->text());
    const auto regexp_valid = regexp.isValid();
    if (!regexp_valid && error_message)
      *error_message = regexp.errorString();
    return regexp_valid;
  }
  return true;
}

FindToolWindow::FindToolWindow(QWidget *parent) : QWidget(parent), m_find_completer(new QCompleter(this)), m_current_filter(nullptr), m_config_widget(nullptr)
{
  m_instance = this;
  m_ui.setupUi(this);
  m_ui.searchTerm->setFiltering(true);
  m_ui.searchTerm->setPlaceholderText(QString());
  setFocusProxy(m_ui.searchTerm);

  connect(m_ui.searchButton, &QAbstractButton::clicked, this, &FindToolWindow::search);
  connect(m_ui.replaceButton, &QAbstractButton::clicked, this, &FindToolWindow::replace);
  connect(m_ui.matchCase, &QAbstractButton::toggled, Find::instance(), &Find::setCaseSensitive);
  connect(m_ui.wholeWords, &QAbstractButton::toggled, Find::instance(), &Find::setWholeWord);
  connect(m_ui.regExp, &QAbstractButton::toggled, Find::instance(), &Find::setRegularExpression);
  connect(m_ui.filterList, QOverload<int>::of(&QComboBox::activated), this, QOverload<int>::of(&FindToolWindow::setCurrentFilter));

  m_find_completer->setModel(Find::findCompletionModel());
  m_ui.searchTerm->setSpecialCompleter(m_find_completer);
  m_ui.searchTerm->installEventFilter(this);
  connect(m_find_completer, QOverload<const QModelIndex&>::of(&QCompleter::activated), this, &FindToolWindow::findCompleterActivated);

  m_ui.searchTerm->setValidationFunction(validateRegExp);
  connect(Find::instance(), &Find::findFlagsChanged, m_ui.searchTerm, &Utils::FancyLineEdit::validate);
  connect(m_ui.searchTerm, &Utils::FancyLineEdit::validChanged, this, &FindToolWindow::updateButtonStates);

  const auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  m_ui.configWidget->setLayout(layout);
  updateButtonStates();

  connect(Find::instance(), &Find::findFlagsChanged, this, &FindToolWindow::updateFindFlags);
}

FindToolWindow::~FindToolWindow()
{
  qDeleteAll(m_config_widgets);
}

auto FindToolWindow::instance() -> FindToolWindow*
{
  return m_instance;
}

auto FindToolWindow::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::KeyPress) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && (ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::KeypadModifier)) {
      ke->accept();
      if (m_ui.searchButton->isEnabled())
        search();
      return true;
    }
  }
  return QWidget::event(event);
}

auto FindToolWindow::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj == m_ui.searchTerm && event->type() == QEvent::KeyPress) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key() == Qt::Key_Down) {
      if (m_ui.searchTerm->text().isEmpty())
        m_find_completer->setCompletionPrefix(QString());
      m_find_completer->complete();
    }
  }
  return QWidget::eventFilter(obj, event);
}

auto FindToolWindow::updateButtonStates() const -> void
{
  const auto filter_enabled = m_current_filter && m_current_filter->isEnabled();
  const auto enabled = filter_enabled && (!m_current_filter->showSearchTermInput() || m_ui.searchTerm->isValid()) && m_current_filter->isValid();

  m_ui.searchButton->setEnabled(enabled);
  m_ui.replaceButton->setEnabled(m_current_filter && m_current_filter->isReplaceSupported() && enabled);

  if (m_config_widget)
    m_config_widget->setEnabled(filter_enabled);

  if (m_current_filter) {
    m_ui.searchTerm->setVisible(m_current_filter->showSearchTermInput());
    m_ui.searchLabel->setVisible(m_current_filter->showSearchTermInput());
    m_ui.optionsWidget->setVisible(m_current_filter->supportedFindFlags() & (FindCaseSensitively | FindWholeWords | FindRegularExpression));
  }

  m_ui.matchCase->setEnabled(filter_enabled && (m_current_filter->supportedFindFlags() & FindCaseSensitively));
  m_ui.wholeWords->setEnabled(filter_enabled && (m_current_filter->supportedFindFlags() & FindWholeWords));
  m_ui.regExp->setEnabled(filter_enabled && (m_current_filter->supportedFindFlags() & FindRegularExpression));
  m_ui.searchTerm->setEnabled(filter_enabled);
}

auto FindToolWindow::updateFindFlags() const -> void
{
  m_ui.matchCase->setChecked(Find::hasFindFlag(FindCaseSensitively));
  m_ui.wholeWords->setChecked(Find::hasFindFlag(FindWholeWords));
  m_ui.regExp->setChecked(Find::hasFindFlag(FindRegularExpression));
}

auto FindToolWindow::setFindFilters(const QList<IFindFilter*> &filters) -> void
{
  qDeleteAll(m_config_widgets);
  m_config_widgets.clear();

  for(const auto &filter: m_filters)
    filter->disconnect(this);

  m_filters = filters;
  m_ui.filterList->clear();

  QStringList names;
  for(auto &filter: filters) {
    names << filter->displayName();
    m_config_widgets.append(filter->createConfigWidget());
    connect(filter, &IFindFilter::displayNameChanged, this, [this, filter]() { updateFindFilterName(filter); });
  }

  m_ui.filterList->addItems(names);
  if (!m_filters.empty())
    setCurrentFilter(0);
}

auto FindToolWindow::findFilters() const -> QList<IFindFilter*>
{
  return m_filters;
}

auto FindToolWindow::updateFindFilterName(IFindFilter *filter) const -> void
{
  if (const auto index = static_cast<int>(m_filters.indexOf(filter)); QTC_GUARD(index >= 0))
    m_ui.filterList->setItemText(index, filter->displayName());
}

auto FindToolWindow::setFindText(const QString &text) const -> void
{
  m_ui.searchTerm->setText(text);
}

auto FindToolWindow::setCurrentFilter(IFindFilter *filter) -> void
{
  if (!filter)
    filter = m_current_filter;

  if (const auto index = static_cast<int>(m_filters.indexOf(filter)); index >= 0)
    setCurrentFilter(index);

  updateFindFlags();

  m_ui.searchTerm->setFocus();
  m_ui.searchTerm->selectAll();
}

auto FindToolWindow::setCurrentFilter(const int index) -> void
{
  m_ui.filterList->setCurrentIndex(index);
  for (auto i = 0; i < m_config_widgets.size(); ++i) {
    const auto config_widget = m_config_widgets.at(i);
    if (i == index) {
      m_config_widget = config_widget;
      if (m_current_filter) {
        disconnect(m_current_filter, &IFindFilter::enabledChanged, this, &FindToolWindow::updateButtonStates);
        disconnect(m_current_filter, &IFindFilter::validChanged, this, &FindToolWindow::updateButtonStates);
      }
      m_current_filter = m_filters.at(i);
      connect(m_current_filter, &IFindFilter::enabledChanged, this, &FindToolWindow::updateButtonStates);
      connect(m_current_filter, &IFindFilter::validChanged, this, &FindToolWindow::updateButtonStates);
      updateButtonStates();
      if (m_config_widget)
        m_ui.configWidget->layout()->addWidget(m_config_widget);
    } else {
      if (config_widget)
        config_widget->setParent(nullptr);
    }
  }

  auto w = m_ui.configWidget;

  while (w) {
    if (const auto sa = qobject_cast<QScrollArea*>(w)) {
      sa->updateGeometry();
      break;
    }
    w = w->parentWidget();
  }

  for (w = m_config_widget ? m_config_widget : m_ui.configWidget; w; w = w->parentWidget()) {
    if (w->layout())
      w->layout()->activate();
  }
}

auto FindToolWindow::acceptAndGetParameters(QString *term, IFindFilter **filter) const -> void
{
  QTC_ASSERT(filter, return);
  *filter = nullptr;
  Find::updateFindCompletion(m_ui.searchTerm->text(), Find::findFlags());
  const auto index = m_ui.filterList->currentIndex();
  const auto search_term = m_ui.searchTerm->text();

  if (index >= 0)
    *filter = m_filters.at(index);

  if (term)
    *term = search_term;

  if (search_term.isEmpty() && *filter && !(*filter)->isValid())
    *filter = nullptr;
}

auto FindToolWindow::search() const -> void
{
  QString term;
  IFindFilter *filter = nullptr;
  acceptAndGetParameters(&term, &filter);
  QTC_ASSERT(filter, return);
  filter->findAll(term, Find::findFlags());
}

auto FindToolWindow::replace() const -> void
{
  QString term;
  IFindFilter *filter = nullptr;
  acceptAndGetParameters(&term, &filter);
  QTC_ASSERT(filter, return);
  filter->replaceAll(term, Find::findFlags());
}

auto FindToolWindow::writeSettings() -> void
{
  const auto settings = ICore::settings();
  settings->beginGroup("Find");
  settings->setValueWithDefault("CurrentFilter", m_current_filter ? m_current_filter->id() : QString());

  for(const auto &filter: m_filters)
    filter->writeSettings(settings);

  settings->endGroup();
}

auto FindToolWindow::readSettings() -> void
{
  QSettings *settings = ICore::settings();
  settings->beginGroup(QLatin1String("Find"));
  const auto current_filter = settings->value(QLatin1String("CurrentFilter")).toString();

  for (auto i = 0; i < m_filters.size(); ++i) {
    const auto filter = m_filters.at(i);
    filter->readSettings(settings);

    if (filter->id() == current_filter)
      setCurrentFilter(i);
  }

  settings->endGroup();
}

auto FindToolWindow::findCompleterActivated(const QModelIndex &index) -> void
{
  const auto find_flags_i = index.data(Find::completion_model_find_flags_role).toInt();
  const FindFlags find_flags(find_flags_i);
  Find::setCaseSensitive(find_flags.testFlag(FindCaseSensitively));
  Find::setBackward(find_flags.testFlag(FindBackward));
  Find::setWholeWord(find_flags.testFlag(FindWholeWords));
  Find::setRegularExpression(find_flags.testFlag(FindRegularExpression));
  Find::setPreserveCase(find_flags.testFlag(FindPreserveCase));
}

} // namespace Orca::Plugin::Core
