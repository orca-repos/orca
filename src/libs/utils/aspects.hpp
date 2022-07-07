// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fileutils.hpp"
#include "id.hpp"
#include "infolabel.hpp"
#include "macroexpander.hpp"
#include "optional.hpp"
#include "pathchooser.hpp"

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QAction;
class QGroupBox;
class QSettings;
QT_END_NAMESPACE

namespace Utils {

class AspectContainer;
class BoolAspect;
class LayoutBuilder;

namespace Internal {
class AspectContainerPrivate;
class BaseAspectPrivate;
class BoolAspectPrivate;
class DoubleAspectPrivate;
class IntegerAspectPrivate;
class MultiSelectionAspectPrivate;
class SelectionAspectPrivate;
class StringAspectPrivate;
class StringListAspectPrivate;
class TextDisplayPrivate;
} // Internal

class ORCA_UTILS_EXPORT BaseAspect : public QObject {
  Q_OBJECT

public:
  BaseAspect();
  ~BaseAspect() override;

  using ConfigWidgetCreator = std::function<QWidget *()>;
  using SavedValueTransformation = std::function<QVariant(const QVariant &)>;

  auto id() const -> Utils::Id;
  auto setId(Utils::Id id) -> void;
  auto value() const -> QVariant;
  auto setValue(const QVariant &value) -> void;
  auto setValueQuietly(const QVariant &value) -> bool;
  auto defaultValue() const -> QVariant;
  auto setDefaultValue(const QVariant &value) -> void;
  auto settingsKey() const -> QString;
  auto setSettingsKey(const QString &settingsKey) -> void;
  auto setSettingsKey(const QString &group, const QString &key) -> void;
  auto displayName() const -> QString;
  auto setDisplayName(const QString &displayName) -> void;
  auto toolTip() const -> QString;
  auto setToolTip(const QString &tooltip) -> void;
  auto isVisible() const -> bool;
  auto setVisible(bool visible) -> void;
  auto isAutoApply() const -> bool;
  auto setAutoApply(bool on) -> void;
  auto isEnabled() const -> bool;
  auto setEnabled(bool enabled) -> void;
  auto setEnabler(BoolAspect *checker) -> void;
  auto isReadOnly() const -> bool;
  auto setReadOnly(bool enabled) -> void;
  auto setSpan(int x, int y = 1) -> void;
  auto labelText() const -> QString;
  auto setLabelText(const QString &labelText) -> void;
  auto setLabelPixmap(const QPixmap &labelPixmap) -> void;
  auto setIcon(const QIcon &labelIcon) -> void;
  auto setConfigWidgetCreator(const ConfigWidgetCreator &configWidgetCreator) -> void;
  auto createConfigWidget() const -> QWidget*;
  virtual auto action() -> QAction*;
  virtual auto fromMap(const QVariantMap &map) -> void;
  virtual auto toMap(QVariantMap &map) const -> void;
  virtual auto toActiveMap(QVariantMap &map) const -> void { toMap(map); }
  virtual auto acquaintSiblings(const AspectContainer &) -> void;
  virtual auto addToLayout(LayoutBuilder &builder) -> void;
  virtual auto volatileValue() const -> QVariant;
  virtual auto setVolatileValue(const QVariant &val) -> void;
  virtual auto emitChangedValue() -> void {}
  virtual auto readSettings(const QSettings *settings) -> void;
  virtual auto writeSettings(QSettings *settings) const -> void;
  auto setFromSettingsTransformation(const SavedValueTransformation &transform) -> void;
  auto setToSettingsTransformation(const SavedValueTransformation &transform) -> void;
  auto toSettingsValue(const QVariant &val) const -> QVariant;
  auto fromSettingsValue(const QVariant &val) const -> QVariant;
  virtual auto apply() -> void;
  virtual auto cancel() -> void;
  virtual auto finish() -> void;
  auto isDirty() const -> bool;
  auto hasAction() const -> bool;

signals:
  auto changed() -> void;
  auto labelLinkActivated(const QString &link) -> void;

protected:
  auto label() const -> QLabel*;
  auto setupLabel() -> void;
  auto addLabeledItem(LayoutBuilder &builder, QWidget *widget) -> void;

  template <class Widget, typename ...Args>
  auto createSubWidget(Args && ...args) -> Widget*
  {
    auto w = new Widget(args...);
    registerSubWidget(w);
    return w;
  }

  auto registerSubWidget(QWidget *widget) -> void;
  static auto saveToMap(QVariantMap &data, const QVariant &value, const QVariant &defaultValue, const QString &key) -> void;

private:
  std::unique_ptr<Internal::BaseAspectPrivate> d;
};

class ORCA_UTILS_EXPORT BoolAspect : public BaseAspect {
  Q_OBJECT

public:
  explicit BoolAspect(const QString &settingsKey = QString());
  ~BoolAspect() override;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto action() -> QAction* override;
  auto volatileValue() const -> QVariant override;
  auto setVolatileValue(const QVariant &val) -> void override;
  auto emitChangedValue() -> void override;
  auto value() const -> bool;
  auto setValue(bool val) -> void;
  auto setDefaultValue(bool val) -> void;

  enum class LabelPlacement {
    AtCheckBox,
    AtCheckBoxWithoutDummyLabel,
    InExtraLabel
  };

  auto setLabel(const QString &labelText, LabelPlacement labelPlacement = LabelPlacement::InExtraLabel) -> void;
  auto setLabelPlacement(LabelPlacement labelPlacement) -> void;
  auto setHandlesGroup(QGroupBox *box) -> void;

signals:
  auto valueChanged(bool newValue) -> void;
  auto volatileValueChanged(bool newValue) -> void;

private:
  std::unique_ptr<Internal::BoolAspectPrivate> d;
};

class ORCA_UTILS_EXPORT SelectionAspect : public BaseAspect {
  Q_OBJECT public:
  SelectionAspect();
  ~SelectionAspect() override;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto volatileValue() const -> QVariant override;
  auto setVolatileValue(const QVariant &val) -> void override;
  auto finish() -> void override;
  auto value() const -> int;
  auto setValue(int val) -> void;
  auto setStringValue(const QString &val) -> void;
  auto setDefaultValue(int val) -> void;
  auto setDefaultValue(const QString &val) -> void;
  auto stringValue() const -> QString;
  auto itemValue() const -> QVariant;

  enum class DisplayStyle {
    RadioButtons,
    ComboBox
  };

  auto setDisplayStyle(DisplayStyle style) -> void;

  class Option {
  public:
    Option(const QString &displayName, const QString &toolTip, const QVariant &itemData) : displayName(displayName), tooltip(toolTip), itemData(itemData) {}
    QString displayName;
    QString tooltip;
    QVariant itemData;
    bool enabled = true;
  };

  auto addOption(const QString &displayName, const QString &toolTip = {}) -> void;
  auto addOption(const Option &option) -> void;
  auto indexForDisplay(const QString &displayName) const -> int;
  auto displayForIndex(int index) const -> QString;
  auto indexForItemValue(const QVariant &value) const -> int;
  auto itemValueForIndex(int index) const -> QVariant;

signals:
  auto volatileValueChanged(int newValue) -> void;

private:
  std::unique_ptr<Internal::SelectionAspectPrivate> d;
};

class ORCA_UTILS_EXPORT MultiSelectionAspect : public BaseAspect {
  Q_OBJECT

public:
  MultiSelectionAspect();
  ~MultiSelectionAspect() override;

  enum class DisplayStyle {
    ListView
  };

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto setDisplayStyle(DisplayStyle style) -> void;
  auto value() const -> QStringList;
  auto setValue(const QStringList &val) -> void;
  auto allValues() const -> QStringList;
  auto setAllValues(const QStringList &val) -> void;

private:
  std::unique_ptr<Internal::MultiSelectionAspectPrivate> d;
};

class ORCA_UTILS_EXPORT StringAspect : public BaseAspect {
  Q_OBJECT

public:
  StringAspect();
  ~StringAspect() override;

  using ValueAcceptor = std::function<Utils::optional<QString>(const QString &, const QString &)>;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto volatileValue() const -> QVariant override;
  auto setVolatileValue(const QVariant &val) -> void override;
  auto emitChangedValue() -> void override;

  // Hook between UI and StringAspect:
  auto setValueAcceptor(ValueAcceptor &&acceptor) -> void;
  auto value() const -> QString;
  auto setValue(const QString &val) -> void;
  auto setDefaultValue(const QString &val) -> void;
  auto setShowToolTipOnLabel(bool show) -> void;
  auto setDisplayFilter(const std::function<QString (const QString &)> &displayFilter) -> void;
  auto setPlaceHolderText(const QString &placeHolderText) -> void;
  auto setHistoryCompleter(const QString &historyCompleterKey) -> void;
  auto setExpectedKind(Utils::PathChooser::Kind expectedKind) -> void;
  auto setEnvironmentChange(const Utils::EnvironmentChange &change) -> void;
  auto setBaseFileName(const Utils::FilePath &baseFileName) -> void;
  auto setUndoRedoEnabled(bool readOnly) -> void;
  auto setAcceptRichText(bool acceptRichText) -> void;
  auto setMacroExpanderProvider(const Utils::MacroExpanderProvider &expanderProvider) -> void;
  auto setUseGlobalMacroExpander() -> void;
  auto setUseResetButton() -> void;
  auto setValidationFunction(const Utils::FancyLineEdit::ValidationFunction &validator) -> void;
  auto setOpenTerminalHandler(const std::function<void()> &openTerminal) -> void;
  auto setAutoApplyOnEditingFinished(bool applyOnEditingFinished) -> void;
  auto setElideMode(Qt::TextElideMode elideMode) -> void;
  auto validateInput() -> void;

  enum class UncheckedSemantics {
    Disabled,
    ReadOnly
  };

  enum class CheckBoxPlacement {
    Top,
    Right
  };

  auto setUncheckedSemantics(UncheckedSemantics semantics) -> void;
  auto isChecked() const -> bool;
  auto setChecked(bool checked) -> void;
  auto makeCheckable(CheckBoxPlacement checkBoxPlacement, const QString &optionalLabel, const QString &optionalBaseKey) -> void;

  enum DisplayStyle {
    LabelDisplay,
    LineEditDisplay,
    TextEditDisplay,
    PathChooserDisplay
  };

  auto setDisplayStyle(DisplayStyle style) -> void;
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &map) const -> void override;
  auto filePath() const -> Utils::FilePath;
  auto setFilePath(const Utils::FilePath &value) -> void;
  auto pathChooser() const -> PathChooser*; // Avoid to use.

signals:
  auto checkedChanged() -> void;
  auto valueChanged(const QString &newValue) -> void;

protected:
  auto update() -> void;

  std::unique_ptr<Internal::StringAspectPrivate> d;
};

class ORCA_UTILS_EXPORT IntegerAspect : public BaseAspect {
  Q_OBJECT

public:
  IntegerAspect();
  ~IntegerAspect() override;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto volatileValue() const -> QVariant override;
  auto setVolatileValue(const QVariant &val) -> void override;
  auto value() const -> qint64;
  auto setValue(qint64 val) -> void;
  auto setDefaultValue(qint64 defaultValue) -> void;
  auto setRange(qint64 min, qint64 max) -> void;
  auto setLabel(const QString &label) -> void; // FIXME: Use setLabelText
  auto setPrefix(const QString &prefix) -> void;
  auto setSuffix(const QString &suffix) -> void;
  auto setDisplayIntegerBase(int base) -> void;
  auto setDisplayScaleFactor(qint64 factor) -> void;
  auto setSpecialValueText(const QString &specialText) -> void;
  auto setSingleStep(qint64 step) -> void;

private:
  std::unique_ptr<Internal::IntegerAspectPrivate> d;
};

class ORCA_UTILS_EXPORT DoubleAspect : public BaseAspect {
  Q_OBJECT

public:
  DoubleAspect();
  ~DoubleAspect() override;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto volatileValue() const -> QVariant override;
  auto setVolatileValue(const QVariant &val) -> void override;
  auto value() const -> double;
  auto setValue(double val) -> void;
  auto setDefaultValue(double defaultValue) -> void;
  auto setRange(double min, double max) -> void;
  auto setPrefix(const QString &prefix) -> void;
  auto setSuffix(const QString &suffix) -> void;
  auto setSpecialValueText(const QString &specialText) -> void;
  auto setSingleStep(double step) -> void;

private:
  std::unique_ptr<Internal::DoubleAspectPrivate> d;
};

class ORCA_UTILS_EXPORT TriState {
  enum Value {
    EnabledValue,
    DisabledValue,
    DefaultValue
  };

  explicit TriState(Value v) : m_value(v) {}

public:
  TriState() = default;

  auto toVariant() const -> QVariant { return int(m_value); }
  static auto fromVariant(const QVariant &variant) -> TriState;

  static const TriState Enabled;
  static const TriState Disabled;
  static const TriState Default;

  friend auto operator==(TriState a, TriState b) -> bool { return a.m_value == b.m_value; }
  friend auto operator!=(TriState a, TriState b) -> bool { return a.m_value != b.m_value; }

private:
  Value m_value = DefaultValue;
};

class ORCA_UTILS_EXPORT TriStateAspect : public SelectionAspect {
  Q_OBJECT

public:
  TriStateAspect(QString onString = tr("Enable"), const QString &offString = tr("Disable"), const QString &defaultString = tr("Leave at Default"));

  auto value() const -> TriState;
  auto setValue(TriState setting) -> void;
  auto setDefaultValue(TriState setting) -> void;
};

class ORCA_UTILS_EXPORT StringListAspect : public BaseAspect {
  Q_OBJECT

public:
  StringListAspect();
  ~StringListAspect() override;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto value() const -> QStringList;
  auto setValue(const QStringList &val) -> void;
  auto appendValue(const QString &value, bool allowDuplicates = true) -> void;
  auto removeValue(const QString &value) -> void;
  auto appendValues(const QStringList &values, bool allowDuplicates = true) -> void;
  auto removeValues(const QStringList &values) -> void;

private:
  std::unique_ptr<Internal::StringListAspectPrivate> d;
};

class ORCA_UTILS_EXPORT IntegersAspect : public BaseAspect {
  Q_OBJECT

public:
  IntegersAspect();
  ~IntegersAspect() override;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto emitChangedValue() -> void override;
  auto value() const -> QList<int>;
  auto setValue(const QList<int> &value) -> void;
  auto setDefaultValue(const QList<int> &value) -> void;

signals:
  auto valueChanged(const QList<int> &values) -> void;
};

class ORCA_UTILS_EXPORT TextDisplay : public BaseAspect {
  Q_OBJECT

public:
  TextDisplay(const QString &message = {}, Utils::InfoLabel::InfoType type = Utils::InfoLabel::None);
  ~TextDisplay() override;

  auto addToLayout(LayoutBuilder &builder) -> void override;
  auto setIconType(Utils::InfoLabel::InfoType t) -> void;
  auto setText(const QString &message) -> void;

private:
  std::unique_ptr<Internal::TextDisplayPrivate> d;
};

class ORCA_UTILS_EXPORT AspectContainer : public QObject {
  Q_OBJECT

public:
  AspectContainer(QObject *parent = nullptr);
  ~AspectContainer() override;
  AspectContainer(const AspectContainer &) = delete;

  using const_iterator = QList<BaseAspect*>::const_iterator;
  using value_type = QList<BaseAspect*>::value_type;

  auto operator=(const AspectContainer &) -> AspectContainer& = delete;
  auto registerAspect(BaseAspect *aspect) -> void;
  auto registerAspects(const AspectContainer &aspects) -> void;

  template <class Aspect, typename ...Args>
  auto addAspect(Args && ...args) -> Aspect*
  {
    auto aspect = new Aspect(args...);
    registerAspect(aspect);
    return aspect;
  }

  auto fromMap(const QVariantMap &map) -> void;
  auto toMap(QVariantMap &map) const -> void;
  auto readSettings(QSettings *settings) -> void;
  auto writeSettings(QSettings *settings) const -> void;
  auto setSettingsGroup(const QString &groupKey) -> void;
  auto setSettingsGroups(const QString &groupKey, const QString &subGroupKey) -> void;
  auto apply() -> void;
  auto cancel() -> void;
  auto finish() -> void;
  auto reset() -> void;
  auto equals(const AspectContainer &other) const -> bool;
  auto copyFrom(const AspectContainer &other) -> void;
  auto setAutoApply(bool on) -> void;
  auto setOwnsSubAspects(bool on) -> void;
  auto isDirty() const -> bool;

  template <typename T>
  auto aspect() const -> T*
  {
    for (BaseAspect *aspect : aspects())
      if (T *result = qobject_cast<T*>(aspect))
        return result;
    return nullptr;
  }

  auto aspect(Utils::Id id) const -> BaseAspect*;

  template <typename T>
  auto aspect(Utils::Id id) const -> T*
  {
    return qobject_cast<T*>(aspect(id));
  }

  auto forEachAspect(const std::function<void(BaseAspect *)> &run) const -> void;
  auto aspects() const -> const QList<BaseAspect*>&;
  auto begin() const -> const_iterator;
  auto end() const -> const_iterator;

signals:
  auto applied() -> void;
  auto fromMapFinished() -> void;

private:
  std::unique_ptr<Internal::AspectContainerPrivate> d;
};

} // namespace Utils
