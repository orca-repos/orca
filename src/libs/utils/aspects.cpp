// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "aspects.h"

#include "algorithm.h"
#include "fancylineedit.h"
#include "layoutbuilder.h"
#include "pathchooser.h"
#include "qtcassert.h"
#include "qtcprocess.h"
#include "qtcsettings.h"
#include "utilsicons.h"
#include "variablechooser.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>
#include <QToolButton>

namespace Utils {
namespace Internal {

class BaseAspectPrivate {
public:
  Utils::Id m_id;
  QVariant m_value;
  QVariant m_defaultValue;
  std::function<QVariant(const QVariant &)> m_toSettings;
  std::function<QVariant(const QVariant &)> m_fromSettings;
  QString m_displayName;
  QString m_settingsKey; // Name of data in settings.
  QString m_tooltip;
  QString m_labelText;
  QPixmap m_labelPixmap;
  QIcon m_icon;
  QPointer<QLabel> m_label;   // Owned by configuration widget
  QPointer<QAction> m_action; // Owned by us.
  bool m_visible = true;
  bool m_enabled = true;
  bool m_readOnly = true;
  bool m_autoApply = true;
  int m_spanX = 1;
  int m_spanY = 1;
  BaseAspect::ConfigWidgetCreator m_configWidgetCreator;
  QList<QPointer<QWidget>> m_subWidgets;
};

} // Internal

/*!
    \class Utils::BaseAspect
    \inmodule Orca

    \brief The \c BaseAspect class provides a common base for classes implementing
    aspects.

    An aspect is a hunk of data like a property or collection of related
    properties of some object, together with a description of its behavior
    for common operations like visualizing or persisting.

    Simple aspects are for example a boolean property represented by a QCheckBox
    in the user interface, or a string property represented by a PathChooser,
    selecting directories in the filesystem.

    While aspects implementations usually have the ability to visualize and to persist
    their data, or use an ID, neither of these is mandatory.
*/

/*!
    Constructs a BaseAspect.
*/
BaseAspect::BaseAspect() : d(new Internal::BaseAspectPrivate) {}

/*!
    Destructs a BaseAspect.
*/
BaseAspect::~BaseAspect()
{
  delete d->m_action;
}

auto BaseAspect::id() const -> Id
{
  return d->m_id;
}

auto BaseAspect::setId(Id id) -> void
{
  d->m_id = id;
}

auto BaseAspect::value() const -> QVariant
{
  return d->m_value;
}

/*!
    Sets value.

    Emits changed() if the value changed.
*/
auto BaseAspect::setValue(const QVariant &value) -> void
{
  if (setValueQuietly(value)) {
    emit changed();
    emitChangedValue();
  }
}

/*!
    Sets value without emitting changed()

    Returns whether the value changed.
*/
auto BaseAspect::setValueQuietly(const QVariant &value) -> bool
{
  if (d->m_value == value)
    return false;
  d->m_value = value;
  return true;
}

auto BaseAspect::defaultValue() const -> QVariant
{
  return d->m_defaultValue;
}

/*!
    Sets a default value and the current value for this aspect.

    \note The current value will be set silently to the same value.
    It is reasonable to only set default values in the setup phase
    of the aspect.

    Default values will not be stored in settings.
*/
auto BaseAspect::setDefaultValue(const QVariant &value) -> void
{
  d->m_defaultValue = value;
  d->m_value = value;
}

auto BaseAspect::setDisplayName(const QString &displayName) -> void
{
  d->m_displayName = displayName;
}

auto BaseAspect::isVisible() const -> bool
{
  return d->m_visible;
}

/*!
    Shows or hides the visual representation of this aspect depending
    on the value of \a visible.
    By default, it is visible.
 */
auto BaseAspect::setVisible(bool visible) -> void
{
  d->m_visible = visible;
  for (QWidget *w : qAsConst(d->m_subWidgets)) {
    QTC_ASSERT(w, continue);
    // This may happen during layout building. Explicit setting visibility here
    // may create a show a toplevel widget for a moment until it is parented
    // to some non-shown widget.
    if (w->parentWidget())
      w->setVisible(visible);
  }
}

auto BaseAspect::setupLabel() -> void
{
  QTC_ASSERT(!d->m_label, delete d->m_label);
  if (d->m_labelText.isEmpty() && d->m_labelPixmap.isNull())
    return;
  d->m_label = new QLabel(d->m_labelText);
  d->m_label->setTextInteractionFlags(d->m_label->textInteractionFlags() | Qt::TextSelectableByMouse);
  connect(d->m_label, &QLabel::linkActivated, this, [this](const QString &link) {
    emit labelLinkActivated(link);
  });
  if (!d->m_labelPixmap.isNull())
    d->m_label->setPixmap(d->m_labelPixmap);
  registerSubWidget(d->m_label);
}

auto BaseAspect::addLabeledItem(LayoutBuilder &builder, QWidget *widget) -> void
{
  setupLabel();
  if (QLabel *l = label()) {
    l->setBuddy(widget);
    builder.addItem(l);
    LayoutBuilder::LayoutItem item(widget);
    item.span = std::max(d->m_spanX - 1, 1);
    builder.addItem(item);
  } else {
    builder.addItem(LayoutBuilder::LayoutItem(widget));
  }
}

/*!
    Sets \a labelText as text for the separate label in the visual
    representation of this aspect.
*/
auto BaseAspect::setLabelText(const QString &labelText) -> void
{
  d->m_labelText = labelText;
  if (d->m_label)
    d->m_label->setText(labelText);
}

/*!
    Sets \a labelPixmap as pixmap for the separate label in the visual
    representation of this aspect.
*/
auto BaseAspect::setLabelPixmap(const QPixmap &labelPixmap) -> void
{
  d->m_labelPixmap = labelPixmap;
  if (d->m_label)
    d->m_label->setPixmap(labelPixmap);
}

auto BaseAspect::setIcon(const QIcon &icon) -> void
{
  d->m_icon = icon;
  if (d->m_action)
    d->m_action->setIcon(icon);
}

/*!
    Returns the current text for the separate label in the visual
    representation of this aspect.
*/
auto BaseAspect::labelText() const -> QString
{
  return d->m_labelText;
}

auto BaseAspect::label() const -> QLabel*
{
  return d->m_label.data();
}

auto BaseAspect::toolTip() const -> QString
{
  return d->m_tooltip;
}

/*!
    Sets \a tooltip as tool tip for the visual representation of this aspect.
 */
auto BaseAspect::setToolTip(const QString &tooltip) -> void
{
  d->m_tooltip = tooltip;
  for (QWidget *w : qAsConst(d->m_subWidgets)) {
    QTC_ASSERT(w, continue);
    w->setToolTip(tooltip);
  }
}

auto BaseAspect::isEnabled() const -> bool
{
  return d->m_enabled;
}

auto BaseAspect::setEnabled(bool enabled) -> void
{
  d->m_enabled = enabled;
  for (QWidget *w : qAsConst(d->m_subWidgets)) {
    QTC_ASSERT(w, continue);
    w->setEnabled(enabled);
  }
}

/*!
    Makes the enabled state of this aspect depend on the checked state of \a checker.
*/
auto BaseAspect::setEnabler(BoolAspect *checker) -> void
{
  QTC_ASSERT(checker, return);
  setEnabled(checker->value());
  connect(checker, &BoolAspect::volatileValueChanged, this, &BaseAspect::setEnabled);
  connect(checker, &BoolAspect::valueChanged, this, &BaseAspect::setEnabled);
}

auto BaseAspect::isReadOnly() const -> bool
{
  return d->m_readOnly;
}

auto BaseAspect::setReadOnly(bool readOnly) -> void
{
  d->m_readOnly = readOnly;
  for (QWidget *w : qAsConst(d->m_subWidgets)) {
    QTC_ASSERT(w, continue);
    if (auto lineEdit = qobject_cast<QLineEdit*>(w))
      lineEdit->setReadOnly(readOnly);
    else if (auto textEdit = qobject_cast<QTextEdit*>(w))
      textEdit->setReadOnly(readOnly);
  }
}

auto BaseAspect::setSpan(int x, int y) -> void
{
  d->m_spanX = x;
  d->m_spanY = y;
}

auto BaseAspect::isAutoApply() const -> bool
{
  return d->m_autoApply;
}

/*!
    Sets auto-apply mode. When auto-apply mode is on, user interaction to this
    aspect's widget will not modify the \c value of the aspect until \c apply()
    is called programmatically.

    \sa setSettingsKey()
*/

auto BaseAspect::setAutoApply(bool on) -> void
{
  d->m_autoApply = on;
}

/*!
    \internal
*/
auto BaseAspect::setConfigWidgetCreator(const ConfigWidgetCreator &configWidgetCreator) -> void
{
  d->m_configWidgetCreator = configWidgetCreator;
}

/*!
    Returns the key to be used when accessing the settings.

    \sa setSettingsKey()
*/
auto BaseAspect::settingsKey() const -> QString
{
  return d->m_settingsKey;
}

/*!
    Sets the key to be used when accessing the settings.

    \sa settingsKey()
*/
auto BaseAspect::setSettingsKey(const QString &key) -> void
{
  d->m_settingsKey = key;
}

/*!
    Sets the key and group to be used when accessing the settings.

    \sa settingsKey()
*/
auto BaseAspect::setSettingsKey(const QString &group, const QString &key) -> void
{
  d->m_settingsKey = group + "/" + key;
}

/*!
    Returns the string that should be used when this action appears in menus
    or other places that are typically used with Book style capitalization.

    If no display name is set, the label text will be used as fallback.
*/

auto BaseAspect::displayName() const -> QString
{
  return d->m_displayName.isEmpty() ? d->m_labelText : d->m_displayName;
}

/*!
    \internal
*/
auto BaseAspect::createConfigWidget() const -> QWidget*
{
  return d->m_configWidgetCreator ? d->m_configWidgetCreator() : nullptr;
}

auto BaseAspect::action() -> QAction*
{
  if (!d->m_action) {
    d->m_action = new QAction(labelText());
    d->m_action->setIcon(d->m_icon);
  }
  return d->m_action;
}

/*!
    Adds the visual representation of this aspect to a layout using
    a layout builder.
*/
auto BaseAspect::addToLayout(LayoutBuilder &) -> void {}

/*!
    Updates this aspect's value from user-initiated changes in the widget.

    This has only an effect if \c isAutoApply is false.
*/
auto BaseAspect::apply() -> void
{
  QTC_CHECK(!d->m_autoApply);
  if (isDirty())
    setValue(volatileValue());
}

/*!
    Discard user changes in the widget and restore widget contents from
    aspect's value.

    This has only an effect if \c isAutoApply is false.
*/
auto BaseAspect::cancel() -> void
{
  QTC_CHECK(!d->m_autoApply);
  if (!d->m_subWidgets.isEmpty())
    setVolatileValue(d->m_value);
}

auto BaseAspect::finish() -> void
{
  // No qDeleteAll() possible as long as the connect in registerSubWidget() exist.
  while (d->m_subWidgets.size())
    delete d->m_subWidgets.takeLast();
}

auto BaseAspect::hasAction() const -> bool
{
  return d->m_action != nullptr;
}

auto BaseAspect::isDirty() const -> bool
{
  QTC_CHECK(!isAutoApply());
  // Aspects that were never shown cannot contain unsaved user changes.
  if (d->m_subWidgets.isEmpty())
    return false;
  return volatileValue() != d->m_value;
}

auto BaseAspect::volatileValue() const -> QVariant
{
  QTC_CHECK(!isAutoApply());
  return {};
}

auto BaseAspect::setVolatileValue(const QVariant &val) -> void
{
  Q_UNUSED(val);
}

auto BaseAspect::registerSubWidget(QWidget *widget) -> void
{
  d->m_subWidgets.append(widget);

  // FIXME: This interferes with qDeleteAll() in finish() and destructor,
  // it would not be needed when all users actually deleted their subwidgets,
  // e.g. the SettingsPage::finish() base implementation, but this still
  // leaves the cases where no such base functionality is available, e.g.
  // in the run/build config aspects.
  connect(widget, &QObject::destroyed, this, [this, widget] {
    d->m_subWidgets.removeAll(widget);
  });

  widget->setEnabled(d->m_enabled);
  widget->setToolTip(d->m_tooltip);

  // Visible is on by default. Not setting it explicitly avoid popping
  // it up when the parent is not set yet, the normal case.
  if (!d->m_visible)
    widget->setVisible(d->m_visible);
}

auto BaseAspect::saveToMap(QVariantMap &data, const QVariant &value, const QVariant &defaultValue, const QString &key) -> void
{
  if (key.isEmpty())
    return;
  if (value == defaultValue)
    data.remove(key);
  else
    data.insert(key, value);
}

/*!
    Retrieves the internal value of this BaseAspect from a \c QVariantMap.
*/
auto BaseAspect::fromMap(const QVariantMap &map) -> void
{
  const QVariant val = map.value(settingsKey(), toSettingsValue(defaultValue()));
  setValue(fromSettingsValue(val));
}

/*!
    Stores the internal value of this BaseAspect into a \c QVariantMap.
*/
auto BaseAspect::toMap(QVariantMap &map) const -> void
{
  saveToMap(map, toSettingsValue(d->m_value), toSettingsValue(d->m_defaultValue), settingsKey());
}

auto BaseAspect::readSettings(const QSettings *settings) -> void
{
  if (settingsKey().isEmpty())
    return;
  const QVariant &val = settings->value(settingsKey());
  setValue(val.isValid() ? fromSettingsValue(val) : defaultValue());
}

auto BaseAspect::writeSettings(QSettings *settings) const -> void
{
  if (settingsKey().isEmpty())
    return;
  QtcSettings::setValueWithDefault(settings, settingsKey(), toSettingsValue(value()), toSettingsValue(defaultValue()));
}

auto BaseAspect::setFromSettingsTransformation(const SavedValueTransformation &transform) -> void
{
  d->m_fromSettings = transform;
}

auto BaseAspect::setToSettingsTransformation(const SavedValueTransformation &transform) -> void
{
  d->m_toSettings = transform;
}

auto BaseAspect::toSettingsValue(const QVariant &val) const -> QVariant
{
  return d->m_toSettings ? d->m_toSettings(val) : val;
}

auto BaseAspect::fromSettingsValue(const QVariant &val) const -> QVariant
{
  return d->m_fromSettings ? d->m_fromSettings(val) : val;
}

/*!
    \internal
*/
auto BaseAspect::acquaintSiblings(const AspectContainer &) -> void {}

namespace Internal {

class BoolAspectPrivate {
public:
  BoolAspect::LabelPlacement m_labelPlacement = BoolAspect::LabelPlacement::AtCheckBox;
  QPointer<QCheckBox> m_checkBox; // Owned by configuration widget
  QPointer<QGroupBox> m_groupBox; // For BoolAspects handling GroupBox check boxes
};

class SelectionAspectPrivate {
public:
  ~SelectionAspectPrivate() { delete m_buttonGroup; }

  SelectionAspect::DisplayStyle m_displayStyle = SelectionAspect::DisplayStyle::RadioButtons;
  QVector<SelectionAspect::Option> m_options;

  // These are all owned by the configuration widget.
  QList<QPointer<QRadioButton>> m_buttons;
  QPointer<QComboBox> m_comboBox;
  QPointer<QButtonGroup> m_buttonGroup;
};

class MultiSelectionAspectPrivate {
public:
  explicit MultiSelectionAspectPrivate(MultiSelectionAspect *q) : q(q) {}

  auto setValueSelectedHelper(const QString &value, bool on) -> bool;

  MultiSelectionAspect *q;
  QStringList m_allValues;
  MultiSelectionAspect::DisplayStyle m_displayStyle = MultiSelectionAspect::DisplayStyle::ListView;

  // These are all owned by the configuration widget.
  QPointer<QListWidget> m_listView;
};

class StringAspectPrivate {
public:
  StringAspect::DisplayStyle m_displayStyle = StringAspect::LabelDisplay;
  StringAspect::CheckBoxPlacement m_checkBoxPlacement = StringAspect::CheckBoxPlacement::Right;
  StringAspect::UncheckedSemantics m_uncheckedSemantics = StringAspect::UncheckedSemantics::Disabled;
  std::function<QString(const QString &)> m_displayFilter;
  std::unique_ptr<BoolAspect> m_checker;

  Qt::TextElideMode m_elideMode = Qt::ElideNone;
  QString m_placeHolderText;
  QString m_historyCompleterKey;
  PathChooser::Kind m_expectedKind = PathChooser::File;
  EnvironmentChange m_environmentChange;
  QPointer<ElidingLabel> m_labelDisplay;
  QPointer<FancyLineEdit> m_lineEditDisplay;
  QPointer<PathChooser> m_pathChooserDisplay;
  QPointer<QTextEdit> m_textEditDisplay;
  MacroExpanderProvider m_expanderProvider;
  FilePath m_baseFileName;
  StringAspect::ValueAcceptor m_valueAcceptor;
  FancyLineEdit::ValidationFunction m_validator;
  std::function<void()> m_openTerminal;

  bool m_undoRedoEnabled = true;
  bool m_acceptRichText = false;
  bool m_showToolTipOnLabel = false;
  bool m_fileDialogOnly = false;
  bool m_useResetButton = false;
  bool m_autoApplyOnEditingFinished = false;
  // Used to block recursive editingFinished signals for example when return is pressed, and
  // the validation changes focus by opening a dialog
  bool m_blockAutoApply = false;

  template <class Widget>
  auto updateWidgetFromCheckStatus(StringAspect *aspect, Widget *w) -> void
  {
    const bool enabled = !m_checker || m_checker->value();
    if (m_uncheckedSemantics == StringAspect::UncheckedSemantics::Disabled)
      w->setEnabled(enabled && aspect->isEnabled());
    else
      w->setReadOnly(!enabled || aspect->isReadOnly());
  }
};

class IntegerAspectPrivate {
public:
  Utils::optional<qint64> m_minimumValue;
  Utils::optional<qint64> m_maximumValue;
  int m_displayIntegerBase = 10;
  qint64 m_displayScaleFactor = 1;
  QString m_prefix;
  QString m_suffix;
  QString m_specialValueText;
  int m_singleStep = 1;
  QPointer<QSpinBox> m_spinBox; // Owned by configuration widget
};

class DoubleAspectPrivate {
public:
  Utils::optional<double> m_minimumValue;
  Utils::optional<double> m_maximumValue;
  QString m_prefix;
  QString m_suffix;
  QString m_specialValueText;
  double m_singleStep = 1;
  QPointer<QDoubleSpinBox> m_spinBox; // Owned by configuration widget
};

class StringListAspectPrivate {
public:
};

class TextDisplayPrivate {
public:
  QString m_message;
  Utils::InfoLabel::InfoType m_type;
  QPointer<InfoLabel> m_label;
};

} // Internal

/*!
    \enum Utils::StringAspect::DisplayStyle
    \inmodule Orca

    The DisplayStyle enum describes the main visual characteristics of a
    string aspect.

      \value LabelDisplay
             Based on QLabel, used for text that cannot be changed by the
             user in this place, for example names of executables that are
             defined in the build system.

      \value LineEditDisplay
             Based on QLineEdit, used for user-editable strings that usually
             fit on a line.

      \value TextEditDisplay
             Based on QTextEdit, used for user-editable strings that often
             do not fit on a line.

      \value PathChooserDisplay
             Based on Utils::PathChooser.

    \sa Utils::PathChooser
*/

/*!
    \class Utils::StringAspect
    \inmodule Orca

    \brief A string aspect is a string-like property of some object, together with
    a description of its behavior for common operations like visualizing or
    persisting.

    String aspects can represent for example a parameter for an external commands,
    paths in a file system, or simply strings.

    The string can be displayed using a QLabel, QLineEdit, QTextEdit or
    Utils::PathChooser.

    The visual representation often contains a label in front of the display
    of the actual value.
*/

/*!
    Constructs a StringAspect.
 */

StringAspect::StringAspect() : d(new Internal::StringAspectPrivate)
{
  setDefaultValue(QString());
  setSpan(2, 1); // Default: Label + something
}

/*!
    \internal
*/
StringAspect::~StringAspect() = default;

/*!
    \internal
*/
auto StringAspect::setValueAcceptor(StringAspect::ValueAcceptor &&acceptor) -> void
{
  d->m_valueAcceptor = std::move(acceptor);
}

/*!
    Returns the value of this StringAspect as an ordinary \c QString.
*/
auto StringAspect::value() const -> QString
{
  return BaseAspect::value().toString();
}

/*!
    Sets the \a value of this StringAspect from an ordinary \c QString.
*/
auto StringAspect::setValue(const QString &val) -> void
{
  const bool isSame = val == value();
  if (isSame)
    return;

  QString processedValue = val;
  if (d->m_valueAcceptor) {
    const Utils::optional<QString> tmp = d->m_valueAcceptor(value(), val);
    if (!tmp) {
      update(); // Make sure the original value is retained in the UI
      return;
    }
    processedValue = tmp.value();
  }

  if (BaseAspect::setValueQuietly(QVariant(processedValue))) {
    update();
    emit changed();
    emit valueChanged(processedValue);
  }
}

auto StringAspect::setDefaultValue(const QString &val) -> void
{
  BaseAspect::setDefaultValue(val);
}

/*!
    \reimp
*/
auto StringAspect::fromMap(const QVariantMap &map) -> void
{
  if (!settingsKey().isEmpty())
    BaseAspect::setValueQuietly(map.value(settingsKey(), defaultValue()));
  if (d->m_checker)
    d->m_checker->fromMap(map);
}

/*!
    \reimp
*/
auto StringAspect::toMap(QVariantMap &map) const -> void
{
  saveToMap(map, value(), defaultValue(), settingsKey());
  if (d->m_checker)
    d->m_checker->toMap(map);
}

/*!
    Returns the value of this string aspect as \c Utils::FilePath.

    \note This simply uses \c FilePath::fromUserInput() for the
    conversion. It does not use any check that the value is actually
    a valid file path.
*/
auto StringAspect::filePath() const -> FilePath
{
  return FilePath::fromUserInput(value());
}

/*!
    Sets the value of this string aspect to \a value.

    \note This simply uses \c FilePath::toUserOutput() for the
    conversion. It does not use any check that the value is actually
    a file path.
*/
auto StringAspect::setFilePath(const FilePath &value) -> void
{
  setValue(value.toUserOutput());
}

auto StringAspect::pathChooser() const -> PathChooser*
{
  return d->m_pathChooserDisplay.data();
}

/*!
    \internal
*/
auto StringAspect::setShowToolTipOnLabel(bool show) -> void
{
  d->m_showToolTipOnLabel = show;
  update();
}

/*!
    Sets a \a displayFilter for fine-tuning the visual appearance
    of the value of this string aspect.
*/
auto StringAspect::setDisplayFilter(const std::function<QString(const QString &)> &displayFilter) -> void
{
  d->m_displayFilter = displayFilter;
}

/*!
    Returns the check box value.

    \sa makeCheckable(), setChecked()
*/
auto StringAspect::isChecked() const -> bool
{
  return !d->m_checker || d->m_checker->value();
}

/*!
    Sets the check box of this aspect to \a checked.

    \sa makeCheckable(), isChecked()
*/
auto StringAspect::setChecked(bool checked) -> void
{
  QTC_ASSERT(d->m_checker, return);
  d->m_checker->setValue(checked);
}

/*!
    Selects the main display characteristics of the aspect according to
    \a displayStyle.

    \note Not all StringAspect features are available with all display styles.

    \sa Utils::StringAspect::DisplayStyle
*/
auto StringAspect::setDisplayStyle(DisplayStyle displayStyle) -> void
{
  d->m_displayStyle = displayStyle;
}

/*!
    Sets \a placeHolderText as place holder for line and text displays.
*/
auto StringAspect::setPlaceHolderText(const QString &placeHolderText) -> void
{
  d->m_placeHolderText = placeHolderText;
  if (d->m_lineEditDisplay)
    d->m_lineEditDisplay->setPlaceholderText(placeHolderText);
  if (d->m_textEditDisplay)
    d->m_textEditDisplay->setPlaceholderText(placeHolderText);
}

/*!
    Sets \a elideMode as label elide mode.
*/
auto StringAspect::setElideMode(Qt::TextElideMode elideMode) -> void
{
  d->m_elideMode = elideMode;
  if (d->m_labelDisplay)
    d->m_labelDisplay->setElideMode(elideMode);
}

/*!
    Sets \a historyCompleterKey as key for the history completer settings for
    line edits and path chooser displays.

    \sa Utils::PathChooser::setExpectedKind()
*/
auto StringAspect::setHistoryCompleter(const QString &historyCompleterKey) -> void
{
  d->m_historyCompleterKey = historyCompleterKey;
  if (d->m_lineEditDisplay)
    d->m_lineEditDisplay->setHistoryCompleter(historyCompleterKey);
  if (d->m_pathChooserDisplay)
    d->m_pathChooserDisplay->setHistoryCompleter(historyCompleterKey);
}

/*!
  Sets \a expectedKind as expected kind for path chooser displays.

  \sa Utils::PathChooser::setExpectedKind()
*/
auto StringAspect::setExpectedKind(const PathChooser::Kind expectedKind) -> void
{
  d->m_expectedKind = expectedKind;
  if (d->m_pathChooserDisplay)
    d->m_pathChooserDisplay->setExpectedKind(expectedKind);
}

auto StringAspect::setEnvironmentChange(const EnvironmentChange &change) -> void
{
  d->m_environmentChange = change;
  if (d->m_pathChooserDisplay)
    d->m_pathChooserDisplay->setEnvironmentChange(change);
}

auto StringAspect::setBaseFileName(const FilePath &baseFileName) -> void
{
  d->m_baseFileName = baseFileName;
  if (d->m_pathChooserDisplay)
    d->m_pathChooserDisplay->setBaseDirectory(baseFileName);
}

auto StringAspect::setUndoRedoEnabled(bool undoRedoEnabled) -> void
{
  d->m_undoRedoEnabled = undoRedoEnabled;
  if (d->m_textEditDisplay)
    d->m_textEditDisplay->setUndoRedoEnabled(undoRedoEnabled);
}

auto StringAspect::setAcceptRichText(bool acceptRichText) -> void
{
  d->m_acceptRichText = acceptRichText;
  if (d->m_textEditDisplay)
    d->m_textEditDisplay->setAcceptRichText(acceptRichText);
}

auto StringAspect::setMacroExpanderProvider(const MacroExpanderProvider &expanderProvider) -> void
{
  d->m_expanderProvider = expanderProvider;
}

auto StringAspect::setUseGlobalMacroExpander() -> void
{
  d->m_expanderProvider = &globalMacroExpander;
}

auto StringAspect::setUseResetButton() -> void
{
  d->m_useResetButton = true;
}

auto StringAspect::setValidationFunction(const FancyLineEdit::ValidationFunction &validator) -> void
{
  d->m_validator = validator;
  if (d->m_lineEditDisplay)
    d->m_lineEditDisplay->setValidationFunction(d->m_validator);
  else if (d->m_pathChooserDisplay)
    d->m_pathChooserDisplay->setValidationFunction(d->m_validator);
}

auto StringAspect::setOpenTerminalHandler(const std::function<void ()> &openTerminal) -> void
{
  d->m_openTerminal = openTerminal;
  if (d->m_pathChooserDisplay)
    d->m_pathChooserDisplay->setOpenTerminalHandler(openTerminal);
}

auto StringAspect::setAutoApplyOnEditingFinished(bool applyOnEditingFinished) -> void
{
  d->m_autoApplyOnEditingFinished = applyOnEditingFinished;
}

auto StringAspect::validateInput() -> void
{
  if (d->m_pathChooserDisplay)
    d->m_pathChooserDisplay->triggerChanged();
  if (d->m_lineEditDisplay)
    d->m_lineEditDisplay->validate();
}

auto StringAspect::setUncheckedSemantics(StringAspect::UncheckedSemantics semantics) -> void
{
  d->m_uncheckedSemantics = semantics;
}

auto StringAspect::addToLayout(LayoutBuilder &builder) -> void
{
  if (d->m_checker && d->m_checkBoxPlacement == CheckBoxPlacement::Top) {
    d->m_checker->addToLayout(builder);
    builder.finishRow();
  }

  const auto useMacroExpander = [this](QWidget *w) {
    if (!d->m_expanderProvider)
      return;
    const auto chooser = new VariableChooser(w);
    chooser->addSupportedWidget(w);
    chooser->addMacroExpanderProvider(d->m_expanderProvider);
  };

  const QString displayedString = d->m_displayFilter ? d->m_displayFilter(value()) : value();

  switch (d->m_displayStyle) {
  case PathChooserDisplay:
    d->m_pathChooserDisplay = createSubWidget<PathChooser>();
    d->m_pathChooserDisplay->setExpectedKind(d->m_expectedKind);
    if (!d->m_historyCompleterKey.isEmpty())
      d->m_pathChooserDisplay->setHistoryCompleter(d->m_historyCompleterKey);
    if (d->m_validator)
      d->m_pathChooserDisplay->setValidationFunction(d->m_validator);
    d->m_pathChooserDisplay->setEnvironmentChange(d->m_environmentChange);
    d->m_pathChooserDisplay->setBaseDirectory(d->m_baseFileName);
    d->m_pathChooserDisplay->setOpenTerminalHandler(d->m_openTerminal);
    d->m_pathChooserDisplay->setFilePath(FilePath::fromUserInput(displayedString));
    d->updateWidgetFromCheckStatus(this, d->m_pathChooserDisplay.data());
    addLabeledItem(builder, d->m_pathChooserDisplay);
    useMacroExpander(d->m_pathChooserDisplay->lineEdit());
    if (isAutoApply()) {
      if (d->m_autoApplyOnEditingFinished) {
        const auto setPathChooserValue = [this] {
          if (d->m_blockAutoApply)
            return;
          d->m_blockAutoApply = true;
          setValue(d->m_pathChooserDisplay->filePath().toString());
          d->m_blockAutoApply = false;
        };
        connect(d->m_pathChooserDisplay, &PathChooser::editingFinished, this, setPathChooserValue);
        connect(d->m_pathChooserDisplay, &PathChooser::browsingFinished, this, setPathChooserValue);
      } else {
        connect(d->m_pathChooserDisplay, &PathChooser::pathChanged, this, [this](const QString &path) {
          setValue(path);
        });
      }
    }
    break;
  case LineEditDisplay:
    d->m_lineEditDisplay = createSubWidget<FancyLineEdit>();
    d->m_lineEditDisplay->setPlaceholderText(d->m_placeHolderText);
    if (!d->m_historyCompleterKey.isEmpty())
      d->m_lineEditDisplay->setHistoryCompleter(d->m_historyCompleterKey);
    if (d->m_validator)
      d->m_lineEditDisplay->setValidationFunction(d->m_validator);
    d->m_lineEditDisplay->setTextKeepingActiveCursor(displayedString);
    d->updateWidgetFromCheckStatus(this, d->m_lineEditDisplay.data());
    addLabeledItem(builder, d->m_lineEditDisplay);
    useMacroExpander(d->m_lineEditDisplay);
    if (isAutoApply()) {
      if (d->m_autoApplyOnEditingFinished) {
        connect(d->m_lineEditDisplay, &FancyLineEdit::editingFinished, this, [this] {
          if (d->m_blockAutoApply)
            return;
          d->m_blockAutoApply = true;
          setValue(d->m_lineEditDisplay->text());
          d->m_blockAutoApply = false;
        });
      } else {
        connect(d->m_lineEditDisplay, &FancyLineEdit::textEdited, this, &StringAspect::setValue);
      }
    }
    if (d->m_useResetButton) {
      auto resetButton = createSubWidget<QPushButton>(tr("Reset"));
      resetButton->setEnabled(d->m_lineEditDisplay->text() != defaultValue());
      connect(resetButton, &QPushButton::clicked, this, [this] {
        d->m_lineEditDisplay->setText(defaultValue().toString());
      });
      connect(d->m_lineEditDisplay, &QLineEdit::textChanged, this, [this, resetButton] {
        resetButton->setEnabled(d->m_lineEditDisplay->text() != defaultValue());
      });
      builder.addItem(resetButton);
    }
    break;
  case TextEditDisplay:
    d->m_textEditDisplay = createSubWidget<QTextEdit>();
    d->m_textEditDisplay->setPlaceholderText(d->m_placeHolderText);
    d->m_textEditDisplay->setUndoRedoEnabled(d->m_undoRedoEnabled);
    d->m_textEditDisplay->setAcceptRichText(d->m_acceptRichText);
    d->m_textEditDisplay->setTextInteractionFlags(Qt::TextEditorInteraction);
    d->m_textEditDisplay->setText(displayedString);
    d->updateWidgetFromCheckStatus(this, d->m_textEditDisplay.data());
    addLabeledItem(builder, d->m_textEditDisplay);
    useMacroExpander(d->m_textEditDisplay);
    if (isAutoApply()) {
      connect(d->m_textEditDisplay, &QTextEdit::textChanged, this, [this] {
        setValue(d->m_textEditDisplay->document()->toPlainText());
      });
    }
    break;
  case LabelDisplay:
    d->m_labelDisplay = createSubWidget<ElidingLabel>();
    d->m_labelDisplay->setElideMode(d->m_elideMode);
    d->m_labelDisplay->setTextInteractionFlags(Qt::TextSelectableByMouse);
    d->m_labelDisplay->setText(displayedString);
    d->m_labelDisplay->setToolTip(d->m_showToolTipOnLabel ? displayedString : toolTip());
    addLabeledItem(builder, d->m_labelDisplay);
    break;
  }

  validateInput();

  if (d->m_checker && d->m_checkBoxPlacement == CheckBoxPlacement::Right)
    d->m_checker->addToLayout(builder);
}

auto StringAspect::volatileValue() const -> QVariant
{
  QTC_CHECK(!isAutoApply());
  switch (d->m_displayStyle) {
  case PathChooserDisplay: QTC_ASSERT(d->m_pathChooserDisplay, return {});
    return d->m_pathChooserDisplay->filePath().toString();
  case LineEditDisplay: QTC_ASSERT(d->m_lineEditDisplay, return {});
    return d->m_lineEditDisplay->text();
  case TextEditDisplay: QTC_ASSERT(d->m_textEditDisplay, return {});
    return d->m_textEditDisplay->document()->toPlainText();
  case LabelDisplay:
    break;
  }
  return {};
}

auto StringAspect::setVolatileValue(const QVariant &val) -> void
{
  switch (d->m_displayStyle) {
  case PathChooserDisplay:
    if (d->m_pathChooserDisplay)
      d->m_pathChooserDisplay->setFilePath(FilePath::fromVariant(val));
    break;
  case LineEditDisplay:
    if (d->m_lineEditDisplay)
      d->m_lineEditDisplay->setText(val.toString());
    break;
  case TextEditDisplay:
    if (d->m_textEditDisplay)
      d->m_textEditDisplay->document()->setPlainText(val.toString());
    break;
  case LabelDisplay:
    break;
  }
}

auto StringAspect::emitChangedValue() -> void
{
  emit valueChanged(value());
}

auto StringAspect::update() -> void
{
  const QString displayedString = d->m_displayFilter ? d->m_displayFilter(value()) : value();

  if (d->m_pathChooserDisplay) {
    d->m_pathChooserDisplay->setFilePath(FilePath::fromString(displayedString));
    d->updateWidgetFromCheckStatus(this, d->m_pathChooserDisplay.data());
  }

  if (d->m_lineEditDisplay) {
    d->m_lineEditDisplay->setTextKeepingActiveCursor(displayedString);
    d->updateWidgetFromCheckStatus(this, d->m_lineEditDisplay.data());
  }

  if (d->m_textEditDisplay) {
    const QString old = d->m_textEditDisplay->document()->toPlainText();
    if (displayedString != old)
      d->m_textEditDisplay->setText(displayedString);
    d->updateWidgetFromCheckStatus(this, d->m_textEditDisplay.data());
  }

  if (d->m_labelDisplay) {
    d->m_labelDisplay->setText(displayedString);
    d->m_labelDisplay->setToolTip(d->m_showToolTipOnLabel ? displayedString : toolTip());
  }

  validateInput();
}

/*!
    Adds a check box with a \a checkerLabel according to \a checkBoxPlacement
    to the line edit.

    The state of the check box is made persistent when using a non-emtpy
    \a checkerKey.
*/
auto StringAspect::makeCheckable(CheckBoxPlacement checkBoxPlacement, const QString &checkerLabel, const QString &checkerKey) -> void
{
  QTC_ASSERT(!d->m_checker, return);
  d->m_checkBoxPlacement = checkBoxPlacement;
  d->m_checker.reset(new BoolAspect);
  d->m_checker->setLabel(checkerLabel, checkBoxPlacement == CheckBoxPlacement::Top ? BoolAspect::LabelPlacement::InExtraLabel : BoolAspect::LabelPlacement::AtCheckBox);
  d->m_checker->setSettingsKey(checkerKey);

  connect(d->m_checker.get(), &BoolAspect::changed, this, &StringAspect::update);
  connect(d->m_checker.get(), &BoolAspect::changed, this, &StringAspect::changed);
  connect(d->m_checker.get(), &BoolAspect::changed, this, &StringAspect::checkedChanged);

  update();
}

/*!
    \class Utils::BoolAspect
    \inmodule Orca

    \brief A boolean aspect is a boolean property of some object, together with
    a description of its behavior for common operations like visualizing or
    persisting.

    The boolean aspect is displayed using a QCheckBox.

    The visual representation often contains a label in front or after
    the display of the actual checkmark.
*/

BoolAspect::BoolAspect(const QString &settingsKey) : d(new Internal::BoolAspectPrivate)
{
  setDefaultValue(false);
  setSettingsKey(settingsKey);
  setSpan(2, 1);
}

/*!
    \reimp
*/
BoolAspect::~BoolAspect() = default;

/*!
    \reimp
*/
auto BoolAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(!d->m_checkBox);
  d->m_checkBox = createSubWidget<QCheckBox>();
  switch (d->m_labelPlacement) {
  case LabelPlacement::AtCheckBoxWithoutDummyLabel:
    d->m_checkBox->setText(labelText());
    builder.addItem(d->m_checkBox.data());
    break;
  case LabelPlacement::AtCheckBox: {
    d->m_checkBox->setText(labelText());
    LayoutBuilder::LayoutType type = builder.layoutType();
    if (type == LayoutBuilder::FormLayout)
      builder.addItem(createSubWidget<QLabel>());
    builder.addItem(d->m_checkBox.data());
    break;
  }
  case LabelPlacement::InExtraLabel:
    addLabeledItem(builder, d->m_checkBox);
    break;
  }
  d->m_checkBox->setChecked(value());
  if (isAutoApply()) {
    connect(d->m_checkBox.data(), &QAbstractButton::clicked, this, [this](bool val) { setValue(val); });
  }
  connect(d->m_checkBox.data(), &QAbstractButton::clicked, this, &BoolAspect::volatileValueChanged);
}

auto BoolAspect::action() -> QAction*
{
  if (hasAction())
    return BaseAspect::action();
  auto act = BaseAspect::action(); // Creates it.
  act->setCheckable(true);
  act->setChecked(value());
  act->setToolTip(toolTip());
  connect(act, &QAction::triggered, this, [this](bool newValue) {
    // The check would be nice to have in simple conditions, but if we
    // have an action that's used both on a settings page and as action
    // in a menu like "Use FakeVim", isAutoApply() is false, and yet this
    // here can trigger.
    //QTC_CHECK(isAutoApply());
    setValue(newValue);
  });
  return act;
}

auto BoolAspect::volatileValue() const -> QVariant
{
  QTC_CHECK(!isAutoApply());
  if (d->m_checkBox)
    return d->m_checkBox->isChecked();
  if (d->m_groupBox)
    return d->m_groupBox->isChecked();
  QTC_CHECK(false);
  return {};
}

auto BoolAspect::setVolatileValue(const QVariant &val) -> void
{
  QTC_CHECK(!isAutoApply());
  if (d->m_checkBox)
    d->m_checkBox->setChecked(val.toBool());
  else if (d->m_groupBox)
    d->m_groupBox->setChecked(val.toBool());
}

auto BoolAspect::emitChangedValue() -> void
{
  emit valueChanged(value());
}

/*!
    \reimp
*/

auto BoolAspect::value() const -> bool
{
  return BaseAspect::value().toBool();
}

auto BoolAspect::setValue(bool value) -> void
{
  if (BaseAspect::setValueQuietly(value)) {
    if (d->m_checkBox)
      d->m_checkBox->setChecked(value);
    //qDebug() << "SetValue: Changing" << labelText() << " to " << value;
    emit changed();
    //QTC_CHECK(!labelText().isEmpty());
    emit valueChanged(value);
    //qDebug() << "SetValue: Changed" << labelText() << " to " << value;
    if (hasAction()) {
      //qDebug() << "SetValue: Triggering " << labelText() << "with" << value;
      emit action()->triggered(value);
    }
  }
}

auto BoolAspect::setDefaultValue(bool val) -> void
{
  BaseAspect::setDefaultValue(val);
}

auto BoolAspect::setLabel(const QString &labelText, LabelPlacement labelPlacement) -> void
{
  BaseAspect::setLabelText(labelText);
  d->m_labelPlacement = labelPlacement;
}

auto BoolAspect::setLabelPlacement(BoolAspect::LabelPlacement labelPlacement) -> void
{
  d->m_labelPlacement = labelPlacement;
}

auto BoolAspect::setHandlesGroup(QGroupBox *box) -> void
{
  registerSubWidget(box);
  d->m_groupBox = box;
}

/*!
    \class Utils::SelectionAspect
    \inmodule Orca

    \brief A selection aspect represents a specific choice out of
    several.

    The selection aspect is displayed using a QComboBox or
    QRadioButtons in a QButtonGroup.
*/

SelectionAspect::SelectionAspect() : d(new Internal::SelectionAspectPrivate)
{
  setSpan(2, 1);
}

/*!
    \reimp
*/
SelectionAspect::~SelectionAspect() = default;

/*!
    \reimp
*/
auto SelectionAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(d->m_buttonGroup == nullptr);
  QTC_CHECK(!d->m_comboBox);
  QTC_ASSERT(d->m_buttons.isEmpty(), d->m_buttons.clear());

  switch (d->m_displayStyle) {
  case DisplayStyle::RadioButtons:
    d->m_buttonGroup = new QButtonGroup();
    d->m_buttonGroup->setExclusive(true);
    for (int i = 0, n = d->m_options.size(); i < n; ++i) {
      const Option &option = d->m_options.at(i);
      auto button = createSubWidget<QRadioButton>(option.displayName);
      button->setChecked(i == value());
      button->setEnabled(option.enabled);
      button->setToolTip(option.tooltip);
      builder.addItems({{}, button});
      d->m_buttons.append(button);
      d->m_buttonGroup->addButton(button, i);
      if (isAutoApply()) {
        connect(button, &QAbstractButton::clicked, this, [this, i] {
          setValue(i);
        });
      }
    }
    break;
  case DisplayStyle::ComboBox:
    setLabelText(displayName());
    d->m_comboBox = createSubWidget<QComboBox>();
    for (int i = 0, n = d->m_options.size(); i < n; ++i)
      d->m_comboBox->addItem(d->m_options.at(i).displayName);
    if (isAutoApply()) {
      connect(d->m_comboBox.data(), QOverload<int>::of(&QComboBox::activated), this, &SelectionAspect::setValue);
    }
    connect(d->m_comboBox.data(), QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SelectionAspect::volatileValueChanged);
    d->m_comboBox->setCurrentIndex(value());
    addLabeledItem(builder, d->m_comboBox);
    break;
  }
}

auto SelectionAspect::volatileValue() const -> QVariant
{
  QTC_CHECK(!isAutoApply());
  switch (d->m_displayStyle) {
  case DisplayStyle::RadioButtons: QTC_ASSERT(d->m_buttonGroup, return {});
    return d->m_buttonGroup->checkedId();
  case DisplayStyle::ComboBox: QTC_ASSERT(d->m_comboBox, return {});
    return d->m_comboBox->currentIndex();
  }
  return {};
}

auto SelectionAspect::setVolatileValue(const QVariant &val) -> void
{
  QTC_CHECK(!isAutoApply());
  switch (d->m_displayStyle) {
  case DisplayStyle::RadioButtons: {
    if (d->m_buttonGroup) {
      QAbstractButton *button = d->m_buttonGroup->button(val.toInt());
      QTC_ASSERT(button, return);
      button->setChecked(true);
    }
    break;
  }
  case DisplayStyle::ComboBox:
    if (d->m_comboBox)
      d->m_comboBox->setCurrentIndex(val.toInt());
    break;
  }
}

auto SelectionAspect::finish() -> void
{
  delete d->m_buttonGroup;
  d->m_buttonGroup = nullptr;
  BaseAspect::finish();
  d->m_buttons.clear();
}

auto SelectionAspect::setDisplayStyle(SelectionAspect::DisplayStyle style) -> void
{
  d->m_displayStyle = style;
}

auto SelectionAspect::value() const -> int
{
  return BaseAspect::value().toInt();
}

auto SelectionAspect::setValue(int value) -> void
{
  if (BaseAspect::setValueQuietly(value)) {
    if (d->m_buttonGroup && 0 <= value && value < d->m_buttons.size())
      d->m_buttons.at(value)->setChecked(true);
    else if (d->m_comboBox)
      d->m_comboBox->setCurrentIndex(value);
    emit changed();
  }
}

auto SelectionAspect::setStringValue(const QString &val) -> void
{
  const int index = indexForDisplay(val);
  QTC_ASSERT(index >= 0, return);
  setValue(index);
}

auto SelectionAspect::setDefaultValue(int val) -> void
{
  BaseAspect::setDefaultValue(val);
}

// Note: This needs to be set after all options are added.
auto SelectionAspect::setDefaultValue(const QString &val) -> void
{
  BaseAspect::setDefaultValue(indexForDisplay(val));
}

auto SelectionAspect::stringValue() const -> QString
{
  return d->m_options.at(value()).displayName;
}

auto SelectionAspect::itemValue() const -> QVariant
{
  return d->m_options.at(value()).itemData;
}

auto SelectionAspect::addOption(const QString &displayName, const QString &toolTip) -> void
{
  d->m_options.append(Option(displayName, toolTip, {}));
}

auto SelectionAspect::addOption(const Option &option) -> void
{
  d->m_options.append(option);
}

auto SelectionAspect::indexForDisplay(const QString &displayName) const -> int
{
  for (int i = 0, n = d->m_options.size(); i < n; ++i) {
    if (d->m_options.at(i).displayName == displayName)
      return i;
  }
  return -1;
}

auto SelectionAspect::displayForIndex(int index) const -> QString
{
  QTC_ASSERT(index >= 0 && index < d->m_options.size(), return {});
  return d->m_options.at(index).displayName;
}

auto SelectionAspect::indexForItemValue(const QVariant &value) const -> int
{
  for (int i = 0, n = d->m_options.size(); i < n; ++i) {
    if (d->m_options.at(i).itemData == value)
      return i;
  }
  return -1;
}

auto SelectionAspect::itemValueForIndex(int index) const -> QVariant
{
  QTC_ASSERT(index >= 0 && index < d->m_options.size(), return {});
  return d->m_options.at(index).itemData;
}

/*!
    \class Utils::MultiSelectionAspect
    \inmodule Orca

    \brief A multi-selection aspect represents one or more choices out of
    several.

    The multi-selection aspect is displayed using a QListWidget with
    checkable items.
*/

MultiSelectionAspect::MultiSelectionAspect() : d(new Internal::MultiSelectionAspectPrivate(this))
{
  setDefaultValue(QStringList());
  setSpan(2, 1);
}

/*!
    \reimp
*/
MultiSelectionAspect::~MultiSelectionAspect() = default;

/*!
    \reimp
*/
auto MultiSelectionAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(d->m_listView == nullptr);
  if (d->m_allValues.isEmpty())
    return;

  switch (d->m_displayStyle) {
  case DisplayStyle::ListView:
    d->m_listView = createSubWidget<QListWidget>();
    for (const QString &val : qAsConst(d->m_allValues)) {
      auto item = new QListWidgetItem(val, d->m_listView);
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      item->setCheckState(value().contains(item->text()) ? Qt::Checked : Qt::Unchecked);
    }
    connect(d->m_listView, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
      if (d->setValueSelectedHelper(item->text(), item->checkState() & Qt::Checked)) emit changed();
    });
    addLabeledItem(builder, d->m_listView);
  }
}

auto Internal::MultiSelectionAspectPrivate::setValueSelectedHelper(const QString &val, bool on) -> bool
{
  QStringList list = q->value();
  if (on && !list.contains(val)) {
    list.append(val);
    q->setValue(list);
    return true;
  }
  if (!on && list.contains(val)) {
    list.removeOne(val);
    q->setValue(list);
    return true;
  }
  return false;
}

auto MultiSelectionAspect::allValues() const -> QStringList
{
  return d->m_allValues;
}

auto MultiSelectionAspect::setAllValues(const QStringList &val) -> void
{
  d->m_allValues = val;
}

auto MultiSelectionAspect::setDisplayStyle(MultiSelectionAspect::DisplayStyle style) -> void
{
  d->m_displayStyle = style;
}

auto MultiSelectionAspect::value() const -> QStringList
{
  return BaseAspect::value().toStringList();
}

auto MultiSelectionAspect::setValue(const QStringList &value) -> void
{
  if (BaseAspect::setValueQuietly(value)) {
    if (d->m_listView) {
      const int n = d->m_listView->count();
      QTC_CHECK(n == d->m_allValues.size());
      for (int i = 0; i != n; ++i) {
        auto item = d->m_listView->item(i);
        item->setCheckState(value.contains(item->text()) ? Qt::Checked : Qt::Unchecked);
      }
    } else {
      emit changed();
    }
  }
}

/*!
    \class Utils::IntegerAspect
    \inmodule Orca

    \brief An integer aspect is a integral property of some object, together with
    a description of its behavior for common operations like visualizing or
    persisting.

    The integer aspect is displayed using a \c QSpinBox.

    The visual representation often contains a label in front
    the display of the spin box.
*/

// IntegerAspect

IntegerAspect::IntegerAspect() : d(new Internal::IntegerAspectPrivate)
{
  setDefaultValue(qint64(0));
  setSpan(2, 1);
}

/*!
    \reimp
*/
IntegerAspect::~IntegerAspect() = default;

/*!
    \reimp
*/
auto IntegerAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(!d->m_spinBox);
  d->m_spinBox = createSubWidget<QSpinBox>();
  d->m_spinBox->setDisplayIntegerBase(d->m_displayIntegerBase);
  d->m_spinBox->setPrefix(d->m_prefix);
  d->m_spinBox->setSuffix(d->m_suffix);
  d->m_spinBox->setSingleStep(d->m_singleStep);
  d->m_spinBox->setSpecialValueText(d->m_specialValueText);
  if (d->m_maximumValue && d->m_maximumValue)
    d->m_spinBox->setRange(int(d->m_minimumValue.value() / d->m_displayScaleFactor), int(d->m_maximumValue.value() / d->m_displayScaleFactor));
  d->m_spinBox->setValue(int(value() / d->m_displayScaleFactor)); // Must happen after setRange()
  addLabeledItem(builder, d->m_spinBox);

  if (isAutoApply()) {
    connect(d->m_spinBox.data(), QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { setValue(d->m_spinBox->value()); });
  }
}

auto IntegerAspect::volatileValue() const -> QVariant
{
  QTC_CHECK(!isAutoApply());
  QTC_ASSERT(d->m_spinBox, return {});
  return d->m_spinBox->value() * d->m_displayScaleFactor;
}

auto IntegerAspect::setVolatileValue(const QVariant &val) -> void
{
  QTC_CHECK(!isAutoApply());
  if (d->m_spinBox)
    d->m_spinBox->setValue(int(val.toLongLong() / d->m_displayScaleFactor));
}

auto IntegerAspect::value() const -> qint64
{
  return BaseAspect::value().toLongLong();
}

auto IntegerAspect::setValue(qint64 value) -> void
{
  BaseAspect::setValue(value);
}

auto IntegerAspect::setRange(qint64 min, qint64 max) -> void
{
  d->m_minimumValue = min;
  d->m_maximumValue = max;
}

auto IntegerAspect::setLabel(const QString &label) -> void
{
  setLabelText(label);
}

auto IntegerAspect::setPrefix(const QString &prefix) -> void
{
  d->m_prefix = prefix;
}

auto IntegerAspect::setSuffix(const QString &suffix) -> void
{
  d->m_suffix = suffix;
}

auto IntegerAspect::setDisplayIntegerBase(int base) -> void
{
  d->m_displayIntegerBase = base;
}

auto IntegerAspect::setDisplayScaleFactor(qint64 factor) -> void
{
  d->m_displayScaleFactor = factor;
}

auto IntegerAspect::setDefaultValue(qint64 defaultValue) -> void
{
  BaseAspect::setDefaultValue(defaultValue);
}

auto IntegerAspect::setSpecialValueText(const QString &specialText) -> void
{
  d->m_specialValueText = specialText;
}

auto IntegerAspect::setSingleStep(qint64 step) -> void
{
  d->m_singleStep = step;
}

/*!
    \class Utils::DoubleAspect
    \inmodule Orca

    \brief An double aspect is a numerical property of some object, together with
    a description of its behavior for common operations like visualizing or
    persisting.

    The double aspect is displayed using a \c QDoubleSpinBox.

    The visual representation often contains a label in front
    the display of the spin box.
*/

DoubleAspect::DoubleAspect() : d(new Internal::DoubleAspectPrivate)
{
  setDefaultValue(double(0));
  setSpan(2, 1);
}

/*!
    \reimp
*/
DoubleAspect::~DoubleAspect() = default;

/*!
    \reimp
*/
auto DoubleAspect::addToLayout(LayoutBuilder &builder) -> void
{
  QTC_CHECK(!d->m_spinBox);
  d->m_spinBox = createSubWidget<QDoubleSpinBox>();
  d->m_spinBox->setPrefix(d->m_prefix);
  d->m_spinBox->setSuffix(d->m_suffix);
  d->m_spinBox->setSingleStep(d->m_singleStep);
  d->m_spinBox->setSpecialValueText(d->m_specialValueText);
  if (d->m_maximumValue && d->m_maximumValue)
    d->m_spinBox->setRange(d->m_minimumValue.value(), d->m_maximumValue.value());
  d->m_spinBox->setValue(value()); // Must happen after setRange()!
  addLabeledItem(builder, d->m_spinBox);

  if (isAutoApply()) {
    connect(d->m_spinBox.data(), QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] { setValue(d->m_spinBox->value()); });
  }
}

auto DoubleAspect::volatileValue() const -> QVariant
{
  QTC_CHECK(!isAutoApply());
  QTC_ASSERT(d->m_spinBox, return {});
  return d->m_spinBox->value();
}

auto DoubleAspect::setVolatileValue(const QVariant &val) -> void
{
  QTC_CHECK(!isAutoApply());
  if (d->m_spinBox)
    d->m_spinBox->setValue(val.toDouble());
}

auto DoubleAspect::value() const -> double
{
  return BaseAspect::value().toDouble();
}

auto DoubleAspect::setValue(double value) -> void
{
  BaseAspect::setValue(value);
}

auto DoubleAspect::setRange(double min, double max) -> void
{
  d->m_minimumValue = min;
  d->m_maximumValue = max;
}

auto DoubleAspect::setPrefix(const QString &prefix) -> void
{
  d->m_prefix = prefix;
}

auto DoubleAspect::setSuffix(const QString &suffix) -> void
{
  d->m_suffix = suffix;
}

auto DoubleAspect::setDefaultValue(double defaultValue) -> void
{
  BaseAspect::setDefaultValue(defaultValue);
}

auto DoubleAspect::setSpecialValueText(const QString &specialText) -> void
{
  d->m_specialValueText = specialText;
}

auto DoubleAspect::setSingleStep(double step) -> void
{
  d->m_singleStep = step;
}

/*!
    \class Utils::BaseTristateAspect
    \inmodule Orca

    \brief A tristate aspect is a property of some object that can have
    three values: enabled, disabled, and unspecified.

    Its visual representation is a QComboBox with three items.
*/

TriStateAspect::TriStateAspect(const QString onString, const QString &offString, const QString &defaultString)
{
  setDisplayStyle(DisplayStyle::ComboBox);
  setDefaultValue(TriState::Default);
  addOption(onString);
  addOption(offString);
  addOption(defaultString);
}

auto TriStateAspect::value() const -> TriState
{
  return TriState::fromVariant(BaseAspect::value());
}

auto TriStateAspect::setValue(TriState value) -> void
{
  BaseAspect::setValue(value.toVariant());
}

auto TriStateAspect::setDefaultValue(TriState value) -> void
{
  BaseAspect::setDefaultValue(value.toVariant());
}

const TriState TriState::Enabled{TriState::EnabledValue};
const TriState TriState::Disabled{TriState::DisabledValue};
const TriState TriState::Default{TriState::DefaultValue};

auto TriState::fromVariant(const QVariant &variant) -> TriState
{
  int v = variant.toInt();
  QTC_ASSERT(v == EnabledValue || v == DisabledValue || v == DefaultValue, v = DefaultValue);
  return TriState(Value(v));
}

/*!
    \class Utils::StringListAspect
    \inmodule Orca

    \brief A string list aspect represents a property of some object
    that is a list of strings.
*/

StringListAspect::StringListAspect() : d(new Internal::StringListAspectPrivate)
{
  setDefaultValue(QStringList());
}

/*!
    \reimp
*/
StringListAspect::~StringListAspect() = default;

/*!
    \reimp
*/
auto StringListAspect::addToLayout(LayoutBuilder &builder) -> void
{
  Q_UNUSED(builder)
  // TODO - when needed.
}

auto StringListAspect::value() const -> QStringList
{
  return BaseAspect::value().toStringList();
}

auto StringListAspect::setValue(const QStringList &value) -> void
{
  BaseAspect::setValue(value);
}

auto StringListAspect::appendValue(const QString &s, bool allowDuplicates) -> void
{
  QStringList val = value();
  if (allowDuplicates || !val.contains(s))
    val.append(s);
  setValue(val);
}

auto StringListAspect::removeValue(const QString &s) -> void
{
  QStringList val = value();
  val.removeAll(s);
  setValue(val);
}

auto StringListAspect::appendValues(const QStringList &values, bool allowDuplicates) -> void
{
  QStringList val = value();
  for (const QString &s : values) {
    if (allowDuplicates || !val.contains(s))
      val.append(s);
  }
  setValue(val);
}

auto StringListAspect::removeValues(const QStringList &values) -> void
{
  QStringList val = value();
  for (const QString &s : values)
    val.removeAll(s);
  setValue(val);
}

/*!
    \class Utils::IntegerListAspect
    \inmodule Orca

    \brief A string list aspect represents a property of some object
    that is a list of strings.
*/

IntegersAspect::IntegersAspect()
{
  setDefaultValue({});
}

/*!
    \reimp
*/
IntegersAspect::~IntegersAspect() = default;

/*!
    \reimp
*/
auto IntegersAspect::addToLayout(LayoutBuilder &builder) -> void
{
  Q_UNUSED(builder)
  // TODO - when needed.
}

auto IntegersAspect::emitChangedValue() -> void
{
  emit valueChanged(value());
}

auto IntegersAspect::value() const -> QList<int>
{
  return Utils::transform(BaseAspect::value().toList(), [](QVariant v) { return v.toInt(); });
}

auto IntegersAspect::setValue(const QList<int> &value) -> void
{
  BaseAspect::setValue(Utils::transform(value, &QVariant::fromValue<int>));
}

auto IntegersAspect::setDefaultValue(const QList<int> &value) -> void
{
  BaseAspect::setDefaultValue(Utils::transform(value, &QVariant::fromValue<int>));
}

/*!
    \class Utils::TextDisplay

    \brief A text display is a phony aspect with the sole purpose of providing
    some text display using an Utils::InfoLabel in places where otherwise
    more expensive Utils::StringAspect items would be used.

    A text display does not have a real value.
*/

/*!
    Constructs a text display showing the \a message with an icon representing
    type \a type.
 */
TextDisplay::TextDisplay(const QString &message, InfoLabel::InfoType type) : d(new Internal::TextDisplayPrivate)
{
  d->m_message = message;
  d->m_type = type;
}

/*!
    \reimp
*/
TextDisplay::~TextDisplay() = default;

/*!
    \reimp
*/
auto TextDisplay::addToLayout(LayoutBuilder &builder) -> void
{
  if (!d->m_label) {
    d->m_label = createSubWidget<InfoLabel>(d->m_message, d->m_type);
    d->m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    d->m_label->setElideMode(Qt::ElideNone);
    d->m_label->setWordWrap(true);
    // Do not use m_label->setVisible(isVisible()) unconditionally, it does not
    // have a QWidget parent yet when used in a LayoutBuilder.
    if (!isVisible())
      d->m_label->setVisible(false);
  }
  builder.addItem(d->m_label.data());
}

/*!
    Sets \a t as the information label type for the visual representation
    of this aspect.
 */
auto TextDisplay::setIconType(InfoLabel::InfoType t) -> void
{
  d->m_type = t;
  if (d->m_label)
    d->m_label->setType(t);
}

auto TextDisplay::setText(const QString &message) -> void
{
  d->m_message = message;
}

/*!
    \class Utils::AspectContainer
    \inmodule Orca

    \brief The AspectContainer class wraps one or more aspects while providing
    the interface of a single aspect.

    Sub-aspects ownership can be declared using \a setOwnsSubAspects.
*/

namespace Internal {

class AspectContainerPrivate {
public:
  QList<BaseAspect*> m_items; // Not owned
  bool m_autoApply = true;
  bool m_ownsSubAspects = false;
  QStringList m_settingsGroup;
};

} // Internal

AspectContainer::AspectContainer(QObject *parent) : QObject(parent), d(new Internal::AspectContainerPrivate) {}

/*!
    \reimp
*/
AspectContainer::~AspectContainer()
{
  if (d->m_ownsSubAspects)
    qDeleteAll(d->m_items);
}

/*!
    \internal
*/
auto AspectContainer::registerAspect(BaseAspect *aspect) -> void
{
  aspect->setAutoApply(d->m_autoApply);
  d->m_items.append(aspect);
}

auto AspectContainer::registerAspects(const AspectContainer &aspects) -> void
{
  for (BaseAspect *aspect : qAsConst(aspects.d->m_items))
    registerAspect(aspect);
}

/*!
    Retrieves a BaseAspect with a given \a id, or nullptr if no such aspect is contained.

    \sa BaseAspect.
*/
auto AspectContainer::aspect(Id id) const -> BaseAspect*
{
  return Utils::findOrDefault(d->m_items, Utils::equal(&BaseAspect::id, id));
}

auto AspectContainer::begin() const -> AspectContainer::const_iterator
{
  return d->m_items.begin();
}

auto AspectContainer::end() const -> AspectContainer::const_iterator
{
  return d->m_items.end();
}

auto AspectContainer::aspects() const -> const QList<BaseAspect*>&
{
  return d->m_items;
}

auto AspectContainer::fromMap(const QVariantMap &map) -> void
{
  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->fromMap(map);

  emit fromMapFinished();
}

auto AspectContainer::toMap(QVariantMap &map) const -> void
{
  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->toMap(map);
}

auto AspectContainer::readSettings(QSettings *settings) -> void
{
  for (const QString &group : d->m_settingsGroup)
    settings->beginGroup(group);

  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->readSettings(settings);

  for (int i = 0; i != d->m_settingsGroup.size(); ++i)
    settings->endGroup();
}

auto AspectContainer::writeSettings(QSettings *settings) const -> void
{
  for (const QString &group : d->m_settingsGroup)
    settings->beginGroup(group);

  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->writeSettings(settings);

  for (int i = 0; i != d->m_settingsGroup.size(); ++i)
    settings->endGroup();
}

auto AspectContainer::setSettingsGroup(const QString &groupKey) -> void
{
  d->m_settingsGroup = QStringList{groupKey};
}

auto AspectContainer::setSettingsGroups(const QString &groupKey, const QString &subGroupKey) -> void
{
  d->m_settingsGroup = QStringList{groupKey, subGroupKey};
}

auto AspectContainer::apply() -> void
{
  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->apply();

  emit applied();
}

auto AspectContainer::cancel() -> void
{
  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->cancel();
}

auto AspectContainer::finish() -> void
{
  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->finish();
}

auto AspectContainer::reset() -> void
{
  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->setValueQuietly(aspect->defaultValue());
}

auto AspectContainer::setAutoApply(bool on) -> void
{
  d->m_autoApply = on;
  for (BaseAspect *aspect : qAsConst(d->m_items))
    aspect->setAutoApply(on);
}

auto AspectContainer::setOwnsSubAspects(bool on) -> void
{
  d->m_ownsSubAspects = on;
}

auto AspectContainer::isDirty() const -> bool
{
  for (BaseAspect *aspect : qAsConst(d->m_items)) {
    if (aspect->isDirty())
      return true;
  }
  return false;
}

auto AspectContainer::equals(const AspectContainer &other) const -> bool
{
  // FIXME: Expensive, but should not really be needed in a fully aspectified world.
  QVariantMap thisMap, thatMap;
  toMap(thisMap);
  other.toMap(thatMap);
  return thisMap == thatMap;
}

auto AspectContainer::copyFrom(const AspectContainer &other) -> void
{
  QVariantMap map;
  other.toMap(map);
  fromMap(map);
}

auto AspectContainer::forEachAspect(const std::function<void(BaseAspect *)> &run) const -> void
{
  for (BaseAspect *aspect : qAsConst(d->m_items)) {
    if (auto container = dynamic_cast<AspectContainer*>(aspect))
      container->forEachAspect(run);
    else
      run(aspect);
  }
}

} // namespace Utils
