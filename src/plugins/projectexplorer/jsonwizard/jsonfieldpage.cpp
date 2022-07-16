// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonfieldpage.hpp"
#include "jsonfieldpage_p.hpp"

#include "jsonwizard.hpp"
#include "jsonwizardfactory.hpp"

#include "../project.hpp"
#include "../projecttree.hpp"

#include <core/core-interface.hpp>
#include <core/core-locator-filter-interface.hpp>
#include <utils/algorithm.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>
#include <utils/stringutils.hpp>
#include <utils/theme/theme.hpp>

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCompleter>
#include <QDebug>
#include <QDir>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QRegularExpression>
#include <QStandardItem>
#include <QTextEdit>
#include <QVariant>
#include <QVariantMap>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {

constexpr char NAME_KEY[] = "name";
constexpr char DISPLAY_NAME_KEY[] = "trDisplayName";
constexpr char TOOLTIP_KEY[] = "trToolTip";
constexpr char MANDATORY_KEY[] = "mandatory";
constexpr char PERSISTENCE_KEY_KEY[] = "persistenceKey";
constexpr char VISIBLE_KEY[] = "visible";
constexpr char ENABLED_KEY[] = "enabled";
constexpr char SPAN_KEY[] = "span";
constexpr char TYPE_KEY[] = "type";
constexpr char DATA_KEY[] = "data";
constexpr char IS_COMPLETE_KEY[] = "isComplete";
constexpr char IS_COMPLETE_MESSAGE_KEY[] = "trIncompleteMessage";

static auto consumeValue(QVariantMap &map, const QString &key, const QVariant &defaultValue = {}) -> QVariant
{
  const auto i = map.find(key);
  if (i != map.end()) {
    auto value = i.value();
    map.erase(i);
    return value;
  }
  return defaultValue;
}

static auto warnAboutUnsupportedKeys(const QVariantMap &map, const QString &name, const QString &type = {}) -> void
{
  if (!map.isEmpty()) {

    auto typeAndName = name;
    if (!type.isEmpty() && !name.isEmpty())
      typeAndName = QString("%1 (\"%2\")").arg(type, name);

    qWarning().noquote() << QString("Field %1 has unsupported keys: %2").arg(typeAndName, map.keys().join(", "));
  }
}

// --------------------------------------------------------------------
// Helper:
// --------------------------------------------------------------------

class LineEdit : public FancyLineEdit {
public:
  LineEdit(MacroExpander *expander, const QRegularExpression &pattern)
  {
    if (pattern.pattern().isEmpty() || !pattern.isValid())
      return;
    m_expander.setDisplayName(JsonFieldPage::tr("Line Edit Validator Expander"));
    m_expander.setAccumulating(true);
    m_expander.registerVariable("INPUT", JsonFieldPage::tr("The text edit input to fix up."), [this]() { return m_currentInput; });
    m_expander.registerSubProvider([expander]() -> MacroExpander* { return expander; });
    setValidationFunction([this, pattern](FancyLineEdit *, QString *) {
      return pattern.match(text()).hasMatch();
    });
  }

  auto setFixupExpando(const QString &expando) -> void { m_fixupExpando = expando; }

private:
  auto fixInputString(const QString &string) -> QString override
  {
    if (m_fixupExpando.isEmpty())
      return string;
    m_currentInput = string;
    return m_expander.expand(m_fixupExpando);
  }

private:
  MacroExpander m_expander;
  QString m_fixupExpando;
  mutable QString m_currentInput;
};

// --------------------------------------------------------------------
// JsonFieldPage::FieldData:
// --------------------------------------------------------------------

JsonFieldPage::Field::Field() : d(std::make_unique<FieldPrivate>()) { }

JsonFieldPage::Field::~Field()
{
  delete d->m_widget;
  delete d->m_label;
}

auto JsonFieldPage::Field::type() const -> QString
{
  return d->m_type;
}

auto JsonFieldPage::Field::setHasUserChanges() -> void
{
  d->m_hasUserChanges = true;
}

auto JsonFieldPage::Field::fromSettings(const QVariant &value) -> void
{
  Q_UNUSED(value);
}

auto JsonFieldPage::Field::toSettings() const -> QVariant
{
  return {};
}

auto JsonFieldPage::Field::parse(const QVariant &input, QString *errorMessage) -> Field*
{
  if (input.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Field is not an object.");
    return nullptr;
  }

  auto tmp = input.toMap();
  const auto name = consumeValue(tmp, NAME_KEY).toString();
  if (name.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Field has no name.");
    return nullptr;
  }
  const auto type = consumeValue(tmp, TYPE_KEY).toString();
  if (type.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Field \"%1\" has no type.").arg(name);
    return nullptr;
  }

  const auto data = createFieldData(type);
  if (!data) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Field \"%1\" has unsupported type \"%2\".").arg(name).arg(type);
    return nullptr;
  }
  data->setTexts(name, JsonWizardFactory::localizedString(consumeValue(tmp, DISPLAY_NAME_KEY).toString()), consumeValue(tmp, TOOLTIP_KEY).toString());

  data->setVisibleExpression(consumeValue(tmp, VISIBLE_KEY, true));
  data->setEnabledExpression(consumeValue(tmp, ENABLED_KEY, true));
  data->setIsMandatory(consumeValue(tmp, MANDATORY_KEY, true).toBool());
  data->setHasSpan(consumeValue(tmp, SPAN_KEY, false).toBool());
  data->setIsCompleteExpando(consumeValue(tmp, IS_COMPLETE_KEY, true), consumeValue(tmp, IS_COMPLETE_MESSAGE_KEY).toString());
  data->setPersistenceKey(consumeValue(tmp, PERSISTENCE_KEY_KEY).toString());

  const auto dataVal = consumeValue(tmp, DATA_KEY);
  if (!data->parseData(dataVal, errorMessage)) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "When parsing Field \"%1\": %2").arg(name).arg(*errorMessage);
    delete data;
    return nullptr;
  }

  warnAboutUnsupportedKeys(tmp, name);
  return data;
}

auto JsonFieldPage::Field::createWidget(JsonFieldPage *page) -> void
{
  const auto w = widget(displayName(), page);
  w->setObjectName(name());
  const auto layout = page->layout();

  if (suppressName()) {
    layout->addWidget(w);
  } else if (hasSpan()) {
    if (!suppressName()) {
      d->m_label = new QLabel(displayName());
      layout->addRow(d->m_label);
    }

    layout->addRow(w);
  } else {
    d->m_label = new QLabel(displayName());
    layout->addRow(d->m_label, w);
  }

  setup(page, name());
}

auto JsonFieldPage::Field::adjustState(MacroExpander *expander) -> void
{
  setVisible(JsonWizard::boolFromVariant(d->m_visibleExpression, expander));
  setEnabled(JsonWizard::boolFromVariant(d->m_enabledExpression, expander));
  QTC_ASSERT(d->m_widget, return);
  d->m_widget->setToolTip(expander->expand(toolTip()));
}

auto JsonFieldPage::Field::setEnabled(bool e) -> void
{
  QTC_ASSERT(d->m_widget, return);
  d->m_widget->setEnabled(e);
}

auto JsonFieldPage::Field::setVisible(bool v) -> void
{
  QTC_ASSERT(d->m_widget, return);
  if (d->m_label)
    d->m_label->setVisible(v);
  d->m_widget->setVisible(v);
}

auto JsonFieldPage::Field::setType(const QString &type) -> void
{
  d->m_type = type;
}

auto JsonFieldPage::Field::validate(MacroExpander *expander, QString *message) -> bool
{
  if (!JsonWizard::boolFromVariant(d->m_isCompleteExpando, expander)) {
    if (message)
      *message = expander->expand(d->m_isCompleteExpandoMessage);
    return false;
  }
  return true;
}

auto JsonFieldPage::Field::initialize(MacroExpander *expander) -> void
{
  adjustState(expander);
  initializeData(expander);
}

auto JsonFieldPage::Field::widget(const QString &displayName, JsonFieldPage *page) -> QWidget*
{
  QTC_ASSERT(!d->m_widget, return d->m_widget);

  d->m_widget = createWidget(displayName, page);
  return d->m_widget;
}

auto JsonFieldPage::Field::name() const -> QString
{
  return d->m_name;
}

auto JsonFieldPage::Field::displayName() const -> QString
{
  return d->m_displayName;
}

auto JsonFieldPage::Field::toolTip() const -> QString
{
  return d->m_toolTip;
}

auto JsonFieldPage::Field::persistenceKey() const -> QString
{
  return d->m_persistenceKey;
}

auto JsonFieldPage::Field::isMandatory() const -> bool
{
  return d->m_isMandatory;
}

auto JsonFieldPage::Field::hasSpan() const -> bool
{
  return d->m_hasSpan;
}

auto JsonFieldPage::Field::hasUserChanges() const -> bool
{
  return d->m_hasUserChanges;
}

auto JsonFieldPage::value(const QString &key) -> QVariant
{
  auto v = property(key.toUtf8());
  if (v.isValid())
    return v;
  const auto w = qobject_cast<JsonWizard*>(wizard());
  QTC_ASSERT(w, return QVariant());
  return w->value(key);
}

auto JsonFieldPage::Field::widget() const -> QWidget*
{
  return d->m_widget;
}

auto JsonFieldPage::Field::setTexts(const QString &name, const QString &displayName, const QString &toolTip) -> void
{
  d->m_name = name;
  d->m_displayName = displayName;
  d->m_toolTip = toolTip;
}

auto JsonFieldPage::Field::setIsMandatory(bool b) -> void
{
  d->m_isMandatory = b;
}

auto JsonFieldPage::Field::setHasSpan(bool b) -> void
{
  d->m_hasSpan = b;
}

auto JsonFieldPage::Field::setVisibleExpression(const QVariant &v) -> void
{
  d->m_visibleExpression = v;
}

auto JsonFieldPage::Field::setEnabledExpression(const QVariant &v) -> void
{
  d->m_enabledExpression = v;
}

auto JsonFieldPage::Field::setIsCompleteExpando(const QVariant &v, const QString &m) -> void
{
  d->m_isCompleteExpando = v;
  d->m_isCompleteExpandoMessage = m;
}

auto JsonFieldPage::Field::setPersistenceKey(const QString &key) -> void
{
  d->m_persistenceKey = key;
}

inline auto operator<<(QDebug &debug, const JsonFieldPage::Field::FieldPrivate &field) -> QDebug&
{
  debug << "name:" << field.m_name << "; displayName:" << field.m_displayName << "; type:" << field.m_type << "; mandatory:" << field.m_isMandatory << "; hasUserChanges:" << field.m_hasUserChanges << "; visibleExpression:" << field.m_visibleExpression << "; enabledExpression:" << field.m_enabledExpression << "; isComplete:" << field.m_isCompleteExpando << "; isCompleteMessage:" << field.m_isCompleteExpandoMessage << "; persistenceKey:" << field.m_persistenceKey;

  return debug;
}

auto operator<<(QDebug &debug, const JsonFieldPage::Field &field) -> QDebug&
{
  debug << "Field{_: " << *field.d << "; subclass: " << field.toString() << "}";

  return debug;
}

// --------------------------------------------------------------------
// LabelFieldData:
// --------------------------------------------------------------------

auto LabelField::parseData(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Label (\"%1\") data is not an object.").arg(name());
    return false;
  }

  auto tmp = data.toMap();

  m_wordWrap = consumeValue(tmp, "wordWrap", false).toBool();
  m_text = JsonWizardFactory::localizedString(consumeValue(tmp, "trText"));

  if (m_text.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Label (\"%1\") has no trText.").arg(name());
    return false;
  }
  warnAboutUnsupportedKeys(tmp, name(), type());
  return true;
}

auto LabelField::createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget*
{
  Q_UNUSED(displayName)
  Q_UNUSED(page)
  const auto w = new QLabel;
  w->setWordWrap(m_wordWrap);
  w->setText(m_text);
  w->setSizePolicy(QSizePolicy::Expanding, w->sizePolicy().verticalPolicy());
  return w;
}

// --------------------------------------------------------------------
// SpacerFieldData:
// --------------------------------------------------------------------

auto SpacerField::parseData(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.isNull())
    return true;

  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Spacer (\"%1\") data is not an object.").arg(name());
    return false;
  }

  auto tmp = data.toMap();

  bool ok;
  m_factor = consumeValue(tmp, "factor", 1).toInt(&ok);

  if (!ok) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Spacer (\"%1\") property \"factor\" is no integer value.").arg(name());
    return false;
  }
  warnAboutUnsupportedKeys(tmp, name(), type());

  return true;
}

auto SpacerField::createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget*
{
  Q_UNUSED(displayName)
  Q_UNUSED(page)
  const auto hspace = QApplication::style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
  const auto vspace = QApplication::style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
  const auto hsize = hspace * m_factor;
  const auto vsize = vspace * m_factor;

  const auto w = new QWidget();
  w->setMinimumSize(hsize, vsize);
  w->setMaximumSize(hsize, vsize);
  w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  return w;
}

// --------------------------------------------------------------------
// LineEditFieldData:
// --------------------------------------------------------------------

auto LineEditField::parseData(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.isNull())
    return true;

  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "LineEdit (\"%1\") data is not an object.").arg(name());
    return false;
  }

  auto tmp = data.toMap();

  m_isPassword = consumeValue(tmp, "isPassword", false).toBool();
  m_defaultText = JsonWizardFactory::localizedString(consumeValue(tmp, "trText").toString());
  m_disabledText = JsonWizardFactory::localizedString(consumeValue(tmp, "trDisabledText").toString());
  m_placeholderText = JsonWizardFactory::localizedString(consumeValue(tmp, "trPlaceholder").toString());
  m_historyId = consumeValue(tmp, "historyId").toString();
  m_restoreLastHistoryItem = consumeValue(tmp, "restoreLastHistoryItem", false).toBool();
  auto pattern = consumeValue(tmp, "validator").toString();
  if (!pattern.isEmpty()) {
    m_validatorRegExp = QRegularExpression('^' + pattern + '$');
    if (!m_validatorRegExp.isValid()) {
      *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "LineEdit (\"%1\") has an invalid regular expression \"%2\" in \"validator\".").arg(name(), pattern);
      m_validatorRegExp = QRegularExpression();
      return false;
    }
  }
  m_fixupExpando = consumeValue(tmp, "fixup").toString();

  const auto completion = consumeValue(tmp, "completion").toString();
  if (completion == "classes") {
    m_completion = Completion::Classes;
  } else if (completion == "namespaces") {
    m_completion = Completion::Namespaces;
  } else if (!completion.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "LineEdit (\"%1\") has an invalid value \"%2\" in \"completion\".").arg(name(), completion);
    return false;
  }

  warnAboutUnsupportedKeys(tmp, name(), type());

  return true;
}

auto LineEditField::createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget*
{
  Q_UNUSED(displayName)
  const auto w = new LineEdit(page->expander(), m_validatorRegExp);
  w->setFixupExpando(m_fixupExpando);

  if (!m_historyId.isEmpty())
    w->setHistoryCompleter(m_historyId, m_restoreLastHistoryItem);

  w->setEchoMode(m_isPassword ? QLineEdit::Password : QLineEdit::Normal);
  QObject::connect(w, &FancyLineEdit::textEdited, [this] { setHasUserChanges(); });
  setupCompletion(w);

  return w;
}

auto LineEditField::setup(JsonFieldPage *page, const QString &name) -> void
{
  const auto w = qobject_cast<FancyLineEdit*>(widget());
  QTC_ASSERT(w, return);
  page->registerFieldWithName(name, w);
  QObject::connect(w, &FancyLineEdit::textChanged, page, [this, page]() -> void {
    m_isModified = true;
    emit page->completeChanged();
  });
}

auto LineEditField::validate(MacroExpander *expander, QString *message) -> bool
{
  if (m_isValidating)
    return true;
  m_isValidating = true;

  const auto w = qobject_cast<FancyLineEdit*>(widget());
  QTC_ASSERT(w, return false);

  if (w->isEnabled()) {
    if (m_isModified) {
      if (!m_currentText.isNull()) {
        w->setText(m_currentText);
        m_currentText.clear();
      }
    } else {
      w->setText(expander->expand(m_defaultText));
      m_isModified = false;
    }
  } else {
    if (!m_disabledText.isNull() && m_currentText.isNull())
      m_currentText = w->text();
  }

  const auto baseValid = Field::validate(expander, message);
  m_isValidating = false;
  return baseValid && !w->text().isEmpty() && w->isValid();
}

auto LineEditField::initializeData(MacroExpander *expander) -> void
{
  const auto w = qobject_cast<FancyLineEdit*>(widget());
  QTC_ASSERT(w, return);
  m_isValidating = true;
  w->setText(expander->expand(m_defaultText));
  w->setPlaceholderText(m_placeholderText);
  m_isModified = false;
  m_isValidating = false;
}

auto LineEditField::fromSettings(const QVariant &value) -> void
{
  m_defaultText = value.toString();
}

auto LineEditField::toSettings() const -> QVariant
{
  return qobject_cast<FancyLineEdit*>(widget())->text();
}

auto LineEditField::setupCompletion(FancyLineEdit *lineEdit) -> void
{
  using namespace Orca::Plugin::Core;
  using namespace Utils;
  if (m_completion == Completion::None)
    return;
  const auto classesFilter = findOrDefault(ILocatorFilter::allLocatorFilters(), equal(&ILocatorFilter::id, Id("Classes")));
  if (!classesFilter)
    return;
  classesFilter->prepareSearch({});
  const auto watcher = new QFutureWatcher<LocatorFilterEntry>;
  const auto handleResults = [this, lineEdit, watcher](int firstIndex, int endIndex) {
    QSet<QString> namespaces;
    QStringList classes;
    const auto project = ProjectTree::currentProject();
    for (auto i = firstIndex; i < endIndex; ++i) {
      static const auto isReservedName = [](const QString &name) {
        static const QRegularExpression rx1("^_[A-Z].*");
        static const QRegularExpression rx2(".*::_[A-Z].*");
        return name.contains("__") || rx1.match(name).hasMatch() || rx2.match(name).hasMatch();
      };
      const auto &entry = watcher->resultAt(i);
      const bool hasNamespace = !entry.extra_info.isEmpty() && !entry.extra_info.startsWith('<') && !entry.extra_info.contains("::<") && !isReservedName(entry.extra_info) && !entry.extra_info.startsWith('~') && !entry.extra_info.contains("Anonymous:") && !FileUtils::isAbsolutePath(entry.extra_info);
      const bool isBaseClassCandidate = !isReservedName(entry.display_name) && !entry.display_name.startsWith("Anonymous:");
      if (isBaseClassCandidate)
        classes << entry.display_name;
      if (hasNamespace) {
        if (isBaseClassCandidate)
          classes << (entry.extra_info + "::" + entry.display_name);
        if (m_completion == Completion::Namespaces) {
          if (!project || entry.file_path.startsWith(project->projectDirectory().toString())) {
            namespaces << entry.extra_info;
          }
        }
      }
    }
    QStringList completionList;
    if (m_completion == Completion::Namespaces) {
      completionList = toList(namespaces);
      completionList = filtered(completionList, [&classes](const QString &ns) {
        return !classes.contains(ns);
      });
      completionList = transform(completionList, [](const QString &ns) {
        return QString(ns + "::");
      });
    } else {
      completionList = classes;
    }
    completionList.sort();
    lineEdit->setSpecialCompleter(new QCompleter(completionList, lineEdit));
  };
  QObject::connect(watcher, &QFutureWatcher<LocatorFilterEntry>::resultsReadyAt, lineEdit, handleResults);
  QObject::connect(watcher, &QFutureWatcher<LocatorFilterEntry>::finished, watcher, &QFutureWatcher<LocatorFilterEntry>::deleteLater);
  watcher->setFuture(runAsync([classesFilter](QFutureInterface<LocatorFilterEntry> &f) {
    const auto matches = classesFilter->matchesFor(f, {});
    if (!matches.isEmpty())
      f.reportResults(QVector<LocatorFilterEntry>(matches.cbegin(), matches.cend()));
    f.reportFinished();
  }));
}

auto LineEditField::setText(const QString &text) -> void
{
  m_currentText = text;

  const auto w = qobject_cast<FancyLineEdit*>(widget());
  w->setText(m_currentText);
}

// --------------------------------------------------------------------
// TextEditFieldData:
// --------------------------------------------------------------------

auto TextEditField::parseData(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.isNull())
    return true;

  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "TextEdit (\"%1\") data is not an object.").arg(name());
    return false;
  }

  auto tmp = data.toMap();

  m_defaultText = JsonWizardFactory::localizedString(consumeValue(tmp, "trText").toString());
  m_disabledText = JsonWizardFactory::localizedString(consumeValue(tmp, "trDisabledText").toString());
  m_acceptRichText = consumeValue(tmp, "richText", true).toBool();

  warnAboutUnsupportedKeys(tmp, name(), type());
  return true;
}

auto TextEditField::createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget*
{
  // TODO: Set up modification monitoring...
  Q_UNUSED(displayName)
  Q_UNUSED(page)
  auto w = new QTextEdit;
  w->setAcceptRichText(m_acceptRichText);
  QObject::connect(w, &QTextEdit::textChanged, [this, w] {
    if (w->toPlainText() != m_defaultText)
      setHasUserChanges();
  });
  return w;
}

auto TextEditField::setup(JsonFieldPage *page, const QString &name) -> void
{
  const auto w = qobject_cast<QTextEdit*>(widget());
  QTC_ASSERT(w, return);
  page->registerFieldWithName(name, w, "plainText", SIGNAL(textChanged()));
  QObject::connect(w, &QTextEdit::textChanged, page, &QWizardPage::completeChanged);
}

auto TextEditField::validate(MacroExpander *expander, QString *message) -> bool
{
  if (!Field::validate(expander, message))
    return false;

  const auto w = qobject_cast<QTextEdit*>(widget());
  QTC_ASSERT(w, return false);

  if (!w->isEnabled() && !m_disabledText.isNull() && m_currentText.isNull()) {
    m_currentText = w->toHtml();
    w->setPlainText(expander->expand(m_disabledText));
  } else if (w->isEnabled() && !m_currentText.isNull()) {
    w->setText(m_currentText);
    m_currentText.clear();
  }

  return !w->toPlainText().isEmpty();
}

auto TextEditField::initializeData(MacroExpander *expander) -> void
{
  const auto w = qobject_cast<QTextEdit*>(widget());
  QTC_ASSERT(w, return);
  w->setPlainText(expander->expand(m_defaultText));
}

auto TextEditField::fromSettings(const QVariant &value) -> void
{
  m_defaultText = value.toString();
}

auto TextEditField::toSettings() const -> QVariant
{
  return qobject_cast<QTextEdit*>(widget())->toPlainText();
}

// --------------------------------------------------------------------
// PathChooserFieldData:
// --------------------------------------------------------------------

auto PathChooserField::parseData(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.isNull())
    return true;

  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "PathChooser data is not an object.");
    return false;
  }

  auto tmp = data.toMap();

  m_path = FilePath::fromVariant(consumeValue(tmp, "path"));
  m_basePath = FilePath::fromVariant(consumeValue(tmp, "basePath"));
  m_historyId = consumeValue(tmp, "historyId").toString();

  const auto kindStr = consumeValue(tmp, "kind", "existingDirectory").toString();
  if (kindStr == "existingDirectory") {
    m_kind = PathChooser::ExistingDirectory;
  } else if (kindStr == "directory") {
    m_kind = PathChooser::Directory;
  } else if (kindStr == "file") {
    m_kind = PathChooser::File;
  } else if (kindStr == "saveFile") {
    m_kind = PathChooser::SaveFile;
  } else if (kindStr == "existingCommand") {
    m_kind = PathChooser::ExistingCommand;
  } else if (kindStr == "command") {
    m_kind = PathChooser::Command;
  } else if (kindStr == "any") {
    m_kind = PathChooser::Any;
  } else {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "kind \"%1\" is not one of the supported \"existingDirectory\", " "\"directory\", \"file\", \"saveFile\", \"existingCommand\", " "\"command\", \"any\".").arg(kindStr);
    return false;
  }

  warnAboutUnsupportedKeys(tmp, name(), type());
  return true;
}

auto PathChooserField::createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget*
{
  Q_UNUSED(displayName)
  Q_UNUSED(page)
  auto w = new PathChooser;
  if (!m_historyId.isEmpty())
    w->setHistoryCompleter(m_historyId);
  QObject::connect(w, &PathChooser::pathChanged, [this, w] {
    if (w->filePath() != m_path)
      setHasUserChanges();
  });
  return w;
}

auto PathChooserField::setEnabled(bool e) -> void
{
  const auto w = qobject_cast<PathChooser*>(widget());
  QTC_ASSERT(w, return);
  w->setReadOnly(!e);
}

auto PathChooserField::setup(JsonFieldPage *page, const QString &name) -> void
{
  const auto w = qobject_cast<PathChooser*>(widget());
  QTC_ASSERT(w, return);
  page->registerFieldWithName(name, w, "path", SIGNAL(rawPathChanged(QString)));
  QObject::connect(w, &PathChooser::rawPathChanged, page, [page](QString) { emit page->completeChanged(); });
}

auto PathChooserField::validate(MacroExpander *expander, QString *message) -> bool
{
  if (!Field::validate(expander, message))
    return false;

  const auto w = qobject_cast<PathChooser*>(widget());
  QTC_ASSERT(w, return false);
  return w->isValid();
}

auto PathChooserField::initializeData(MacroExpander *expander) -> void
{
  const auto w = qobject_cast<PathChooser*>(widget());
  QTC_ASSERT(w, return);
  w->setBaseDirectory(expander->expand(m_basePath));
  w->setExpectedKind(m_kind);
  w->setFilePath(expander->expand(m_path));
}

auto PathChooserField::fromSettings(const QVariant &value) -> void
{
  m_path = FilePath::fromVariant(value);
}

auto PathChooserField::toSettings() const -> QVariant
{
  return qobject_cast<PathChooser*>(widget())->filePath().toVariant();
}

// --------------------------------------------------------------------
// CheckBoxFieldData:
// --------------------------------------------------------------------

auto CheckBoxField::parseData(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.isNull())
    return true;

  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "CheckBox (\"%1\") data is not an object.").arg(name());
    return false;
  }

  auto tmp = data.toMap();

  m_checkedValue = consumeValue(tmp, "checkedValue", true).toString();
  m_uncheckedValue = consumeValue(tmp, "uncheckedValue", false).toString();
  if (m_checkedValue == m_uncheckedValue) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "CheckBox (\"%1\") values for checked and unchecked state are identical.").arg(name());
    return false;
  }
  m_checkedExpression = consumeValue(tmp, "checked", false);

  warnAboutUnsupportedKeys(tmp, name(), type());
  return true;
}

auto CheckBoxField::createWidget(const QString &displayName, JsonFieldPage *page) -> QWidget*
{
  Q_UNUSED(page)
  return new QCheckBox(displayName);
}

auto CheckBoxField::setup(JsonFieldPage *page, const QString &name) -> void
{
  auto w = qobject_cast<QCheckBox*>(widget());
  QTC_ASSERT(w, return);
  page->registerObjectAsFieldWithName<QCheckBox>(name, w, &QCheckBox::stateChanged, [this, page, w]() -> QString {
    if (w->checkState() == Qt::Checked)
      return page->expander()->expand(m_checkedValue);
    return page->expander()->expand(m_uncheckedValue);
  });

  QObject::connect(w, &QCheckBox::clicked, page, [this, page]() {
    m_isModified = true;
    setHasUserChanges();
    emit page->completeChanged();
  });
}

auto CheckBoxField::setChecked(bool value) -> void
{
  const auto w = qobject_cast<QCheckBox*>(widget());
  QTC_ASSERT(w, return);

  w->setChecked(value);
  emit w->clicked(value);
}

auto CheckBoxField::validate(MacroExpander *expander, QString *message) -> bool
{
  if (!Field::validate(expander, message))
    return false;

  if (!m_isModified) {
    const auto w = qobject_cast<QCheckBox*>(widget());
    QTC_ASSERT(w, return false);
    w->setChecked(JsonWizard::boolFromVariant(m_checkedExpression, expander));
  }
  return true;
}

auto CheckBoxField::initializeData(MacroExpander *expander) -> void
{
  const auto w = qobject_cast<QCheckBox*>(widget());
  QTC_ASSERT(widget(), return);

  w->setChecked(JsonWizard::boolFromVariant(m_checkedExpression, expander));
}

auto CheckBoxField::fromSettings(const QVariant &value) -> void
{
  m_checkedExpression = value;
}

auto CheckBoxField::toSettings() const -> QVariant
{
  return qobject_cast<QCheckBox*>(widget())->isChecked();
}

// --------------------------------------------------------------------
// ListFieldData:
// --------------------------------------------------------------------

auto createStandardItemFromListItem(const QVariant &item, QString *errorMessage) -> std::unique_ptr<QStandardItem>
{
  if (item.type() == QVariant::List) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "No JSON lists allowed inside List items.");
    return {};
  }
  auto standardItem = std::make_unique<QStandardItem>();
  if (item.type() == QVariant::Map) {
    auto tmp = item.toMap();
    const auto key = JsonWizardFactory::localizedString(consumeValue(tmp, "trKey", QString()).toString());
    const auto value = consumeValue(tmp, "value", key);

    if (key.isNull() || key.isEmpty()) {
      *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "No \"key\" found in List items.");
      return {};
    }
    standardItem->setText(key);
    standardItem->setData(value, ListField::ValueRole);
    standardItem->setData(consumeValue(tmp, "condition", true), ListField::ConditionRole);
    standardItem->setData(consumeValue(tmp, "icon"), ListField::IconStringRole);
    standardItem->setToolTip(JsonWizardFactory::localizedString(consumeValue(tmp, "trToolTip", QString()).toString()));
    warnAboutUnsupportedKeys(tmp, QString(), "List");
  } else {
    const auto keyvalue = item.toString();
    standardItem->setText(keyvalue);
    standardItem->setData(keyvalue, ListField::ValueRole);
    standardItem->setData(true, ListField::ConditionRole);
  }
  return standardItem;
}

ListField::ListField() = default;

ListField::~ListField() = default;

auto ListField::parseData(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "%1 (\"%2\") data is not an object.").arg(type(), name());
    return false;
  }

  auto tmp = data.toMap();

  bool ok;
  m_index = consumeValue(tmp, "index", 0).toInt(&ok);
  if (!ok) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "%1 (\"%2\") \"index\" is not an integer value.").arg(type(), name());
    return false;
  }
  m_disabledIndex = consumeValue(tmp, "disabledIndex", -1).toInt(&ok);
  if (!ok) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "%1 (\"%2\") \"disabledIndex\" is not an integer value.").arg(type(), name());
    return false;
  }

  const auto value = consumeValue(tmp, "items");
  if (value.isNull()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "%1 (\"%2\") \"items\" missing.").arg(type(), name());
    return false;
  }
  if (value.type() != QVariant::List) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "%1 (\"%2\") \"items\" is not a JSON list.").arg(type(), name());
    return false;
  }

  for (const auto &i : value.toList()) {
    auto item = createStandardItemFromListItem(i, errorMessage);
    QTC_ASSERT(!item || !item->text().isEmpty(), continue);
    m_itemList.emplace_back(std::move(item));
  }

  warnAboutUnsupportedKeys(tmp, name(), type());
  return true;
}

auto ListField::validate(MacroExpander *expander, QString *message) -> bool
{
  if (!Field::validate(expander, message))
    return false;

  updateIndex();
  return selectionModel()->hasSelection();
}

auto ListField::initializeData(MacroExpander *expander) -> void
{
  QTC_ASSERT(widget(), return);

  if (m_index >= int(m_itemList.size())) {
    qWarning().noquote() << QString("%1 (\"%2\") has an index of %3 which does not exist.").arg(type(), name(), QString::number(m_index));
    m_index = -1;
  }

  auto currentItem = m_index >= 0 ? m_itemList[uint(m_index)].get() : nullptr;
  QList<QStandardItem*> expandedValuesItems;
  expandedValuesItems.reserve(int(m_itemList.size()));

  for (const auto &item : m_itemList) {
    const auto condition = JsonWizard::boolFromVariant(item->data(ConditionRole), expander);
    if (!condition)
      continue;
    const auto expandedValuesItem = item->clone();
    if (item.get() == currentItem)
      currentItem = expandedValuesItem;
    expandedValuesItem->setText(expander->expand(item->text()));
    expandedValuesItem->setData(expander->expandVariant(item->data(ValueRole)), ValueRole);
    expandedValuesItem->setData(expander->expand(item->data(IconStringRole).toString()), IconStringRole);
    expandedValuesItem->setData(condition, ConditionRole);

    auto iconPath = expandedValuesItem->data(IconStringRole).toString();
    if (!iconPath.isEmpty()) {
      if (auto *page = qobject_cast<JsonFieldPage*>(widget()->parentWidget())) {
        const auto wizardDirectory = page->value("WizardDir").toString();
        iconPath = QDir::cleanPath(QDir(wizardDirectory).absoluteFilePath(iconPath));
        if (QFileInfo::exists(iconPath)) {
          QIcon icon(iconPath);
          expandedValuesItem->setIcon(icon);
          addPossibleIconSize(icon);
        } else {
          qWarning().noquote() << QString("Icon file \"%1\" not found.").arg(QDir::toNativeSeparators(iconPath));
        }
      } else {
        qWarning().noquote() << QString("%1 (\"%2\") has no parentWidget JsonFieldPage to get the icon path.").arg(type(), name());
      }
    }
    expandedValuesItems.append(expandedValuesItem);
  }

  itemModel()->clear();
  itemModel()->appendColumn(expandedValuesItems); // inserts the first column

  selectionModel()->setCurrentIndex(itemModel()->indexFromItem(currentItem), QItemSelectionModel::ClearAndSelect);

  updateIndex();
}

auto ListField::itemModel() -> QStandardItemModel*
{
  if (!m_itemModel)
    m_itemModel = new QStandardItemModel(widget());
  return m_itemModel;
}

auto ListField::selectRow(int row) -> bool
{
  const auto index = itemModel()->index(row, 0);
  if (!index.isValid())
    return false;

  selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);

  this->updateIndex();
  return true;
}

auto ListField::selectionModel() const -> QItemSelectionModel*
{
  return m_selectionModel;
}

auto ListField::setSelectionModel(QItemSelectionModel *selectionModel) -> void
{
  m_selectionModel = selectionModel;
}

auto ListField::maxIconSize() const -> QSize
{
  return m_maxIconSize;
}

auto ListField::addPossibleIconSize(const QIcon &icon) -> void
{
  const auto iconSize = icon.availableSizes().value(0);
  if (iconSize.height() > m_maxIconSize.height())
    m_maxIconSize = iconSize;
}

auto ListField::updateIndex() -> void
{
  if (!widget()->isEnabled() && m_disabledIndex >= 0 && m_savedIndex < 0) {
    m_savedIndex = selectionModel()->currentIndex().row();
    selectionModel()->setCurrentIndex(itemModel()->index(m_disabledIndex, 0), QItemSelectionModel::ClearAndSelect);
  } else if (widget()->isEnabled() && m_savedIndex >= 0) {
    selectionModel()->setCurrentIndex(itemModel()->index(m_savedIndex, 0), QItemSelectionModel::ClearAndSelect);
    m_savedIndex = -1;
  }
}

auto ListField::fromSettings(const QVariant &value) -> void
{
  for (decltype(m_itemList)::size_type i = 0; i < m_itemList.size(); ++i) {
    if (m_itemList.at(i)->data(ValueRole) == value) {
      m_index = int(i);
      break;
    }
  }
}

auto ListField::toSettings() const -> QVariant
{
  const auto idx = selectionModel()->currentIndex().row();
  return idx >= 0 ? m_itemList.at(idx)->data(ValueRole) : QVariant();
}

auto ComboBoxField::setup(JsonFieldPage *page, const QString &name) -> void
{
  auto w = qobject_cast<QComboBox*>(widget());
  QTC_ASSERT(w, return);
  w->setModel(itemModel());
  w->setInsertPolicy(QComboBox::NoInsert);

  auto s = w->sizePolicy();
  s.setHorizontalPolicy(QSizePolicy::Expanding);
  w->setSizePolicy(s);

  setSelectionModel(w->view()->selectionModel());

  // the selectionModel does not behave like expected and wanted - so we block signals here
  // (for example there was some losing focus thing when hovering over items, ...)
  selectionModel()->blockSignals(true);
  QObject::connect(w, QOverload<int>::of(&QComboBox::activated), [w, this](int index) {
    w->blockSignals(true);
    selectionModel()->clearSelection();

    selectionModel()->blockSignals(false);
    selectionModel()->setCurrentIndex(w->model()->index(index, 0), QItemSelectionModel::ClearAndSelect);
    selectionModel()->blockSignals(true);
    w->blockSignals(false);
  });
  page->registerObjectAsFieldWithName<QComboBox>(name, w, QOverload<int>::of(&QComboBox::activated), [w]() {
    return w->currentData(ValueRole);
  });
  QObject::connect(selectionModel(), &QItemSelectionModel::selectionChanged, page, [page]() {
    emit page->completeChanged();
  });
}

auto ComboBoxField::createWidget(const QString & /*displayName*/, JsonFieldPage * /*page*/) -> QWidget*
{
  const auto comboBox = new QComboBox;
  QObject::connect(comboBox, QOverload<int>::of(&QComboBox::activated), [this] { setHasUserChanges(); });
  return comboBox;
}

auto ComboBoxField::initializeData(MacroExpander *expander) -> void
{
  ListField::initializeData(expander);
  // refresh also the current text of the combobox
  const auto w = qobject_cast<QComboBox*>(widget());
  w->setCurrentIndex(selectionModel()->currentIndex().row());
}

auto ComboBoxField::toSettings() const -> QVariant
{
  if (const auto w = qobject_cast<QComboBox*>(widget()))
    return w->currentData(ValueRole);
  return {};
}

auto ComboBoxField::selectRow(int row) -> bool
{
  if (!ListField::selectRow(row))
    return false;

  const auto w = qobject_cast<QComboBox*>(widget());
  w->setCurrentIndex(row);

  return true;
}

auto ComboBoxField::selectedRow() const -> int
{
  const auto w = qobject_cast<QComboBox*>(widget());
  return w->currentIndex();
}

auto IconListField::setup(JsonFieldPage *page, const QString &name) -> void
{
  const auto w = qobject_cast<QListView*>(widget());
  QTC_ASSERT(w, return);

  w->setViewMode(QListView::IconMode);
  w->setMovement(QListView::Static);
  w->setResizeMode(QListView::Adjust);
  w->setSelectionRectVisible(false);
  w->setWrapping(true);
  w->setWordWrap(true);

  w->setModel(itemModel());
  setSelectionModel(w->selectionModel());
  page->registerObjectAsFieldWithName<QItemSelectionModel>(name, selectionModel(), &QItemSelectionModel::selectionChanged, [this]() {
    const auto i = selectionModel()->currentIndex();
    if (i.isValid())
      return i.data(ValueRole);
    return QVariant();
  });
  QObject::connect(selectionModel(), &QItemSelectionModel::selectionChanged, page, [page]() {
    emit page->completeChanged();
  });
}

auto IconListField::createWidget(const QString & /*displayName*/, JsonFieldPage * /*page*/) -> QWidget*
{
  const auto listView = new QListView;
  QObject::connect(listView->selectionModel(), &QItemSelectionModel::currentChanged, [this] { setHasUserChanges(); });
  return listView;
}

auto IconListField::initializeData(MacroExpander *expander) -> void
{
  ListField::initializeData(expander);
  const auto w = qobject_cast<QListView*>(widget());
  const auto spacing = 4;
  w->setSpacing(spacing);
  w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // adding 1/3 height of the icon to see following items if there are some
  w->setMinimumHeight(maxIconSize().height() + maxIconSize().height() / 3);
  w->setIconSize(maxIconSize());
}

// --------------------------------------------------------------------
// JsonFieldPage:
// --------------------------------------------------------------------

QHash<QString, JsonFieldPage::FieldFactory> JsonFieldPage::m_factories;

JsonFieldPage::JsonFieldPage(MacroExpander *expander, QWidget *parent) : WizardPage(parent), m_formLayout(new QFormLayout), m_errorLabel(new QLabel), m_expander(expander)
{
  QTC_CHECK(m_expander);

  const auto vLayout = new QVBoxLayout;
  m_formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  vLayout->addLayout(m_formLayout);
  m_errorLabel->setVisible(false);
  auto palette = m_errorLabel->palette();
  palette.setColor(QPalette::WindowText, orcaTheme()->color(Theme::TextColorError));
  m_errorLabel->setPalette(palette);
  vLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
  vLayout->addWidget(m_errorLabel);
  setLayout(vLayout);
}

JsonFieldPage::~JsonFieldPage()
{
  // Do not delete m_expander, it belongs to the wizard!
  qDeleteAll(m_fields);
}

auto JsonFieldPage::registerFieldFactory(const QString &id, const FieldFactory &ff) -> void
{
  QTC_ASSERT(!m_factories.contains(id), return);
  m_factories.insert(id, ff);
}

auto JsonFieldPage::setup(const QVariant &data) -> bool
{
  QString errorMessage;
  auto fieldList = JsonWizardFactory::objectOrList(data, &errorMessage);
  foreach(const QVariant &field, fieldList) {
    const auto f = Field::parse(field, &errorMessage);
    if (!f)
      continue;
    f->createWidget(this);
    if (!f->persistenceKey().isEmpty()) {
      f->setPersistenceKey(m_expander->expand(f->persistenceKey()));
      const auto value = Orca::Plugin::Core::ICore::settings()->value(fullSettingsKey(f->persistenceKey()));
      if (value.isValid())
        f->fromSettings(value);
    }
    m_fields.append(f);
  }
  return true;
}

auto JsonFieldPage::isComplete() const -> bool
{
  QString message;

  auto result = true;
  auto hasErrorMessage = false;
  foreach(Field *f, m_fields) {
    f->adjustState(m_expander);
    if (!f->validate(m_expander, &message)) {
      if (!message.isEmpty()) {
        showError(message);
        hasErrorMessage = true;
      }
      if (f->isMandatory() && !f->widget()->isHidden())
        result = false;
    }
  }

  if (!hasErrorMessage)
    clearError();

  return result;
}

auto JsonFieldPage::initializePage() -> void
{
  foreach(Field *f, m_fields)
    f->initialize(m_expander);
}

auto JsonFieldPage::cleanupPage() -> void
{
  foreach(Field *f, m_fields)
    f->cleanup(m_expander);
}

auto JsonFieldPage::validatePage() -> bool
{
  for (const auto f : qAsConst(m_fields))
    if (!f->persistenceKey().isEmpty() && f->hasUserChanges()) {
      const auto value = f->toSettings();
      if (value.isValid())
        Orca::Plugin::Core::ICore::settings()->setValue(fullSettingsKey(f->persistenceKey()), value);
    }
  return true;
}

auto JsonFieldPage::showError(const QString &m) const -> void
{
  m_errorLabel->setText(m);
  m_errorLabel->setVisible(true);
}

auto JsonFieldPage::clearError() const -> void
{
  m_errorLabel->setText(QString());
  m_errorLabel->setVisible(false);
}

auto JsonFieldPage::expander() -> MacroExpander*
{
  return m_expander;
}

auto JsonFieldPage::createFieldData(const QString &type) -> Field*
{
  if (const auto factory = m_factories.value(type)) {
    const auto field = factory();
    field->setType(type);
    return field;
  }
  return nullptr;
}

auto JsonFieldPage::fullSettingsKey(const QString &fieldKey) -> QString
{
  return "Wizards/" + fieldKey;
}

} // namespace ProjectExplorer
