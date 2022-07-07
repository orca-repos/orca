// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>

#include <utils/fileutils.hpp>
#include <utils/id.hpp>
#include <utils/optional.hpp>

#include <QFutureInterface>
#include <QIcon>
#include <QMetaType>
#include <QVariant>

namespace Core {

class ILocatorFilter;

struct LocatorFilterEntry {
  struct HighlightInfo {
    enum DataType {
      DisplayName,
      ExtraInfo
    };

    HighlightInfo(const int start_index, const int length, const DataType type = DisplayName) : starts{start_index}, lengths{length}, dataType(type) {}
    HighlightInfo(QVector<int> start_index, QVector<int> length, const DataType type = DisplayName) : starts(std::move(start_index)), lengths(std::move(length)), dataType(type) {}

    QVector<int> starts;
    QVector<int> lengths;
    DataType dataType;
  };

  LocatorFilterEntry() = default;

  LocatorFilterEntry(ILocatorFilter *from_filter, QString name, QVariant data, Utils::optional<QIcon> icon = Utils::nullopt) : filter(from_filter), display_name(std::move(name)), internal_data(std::move(data)), display_icon(std::move(icon)) {}

  /* backpointer to creating filter */
  ILocatorFilter *filter = nullptr;
  /* displayed string */
  QString display_name;
  /* extra information displayed in light-gray in a second column (optional) */
  QString extra_info;
  /* additional tooltip */
  QString tool_tip;
  /* can be used by the filter to save more information about the entry */
  QVariant internal_data;
  /* icon to display along with the entry */
  Utils::optional<QIcon> display_icon;
  /* file path, if the entry is related to a file, is used e.g. for resolving a file icon */
  Utils::FilePath file_path;
  /* highlighting support */
  HighlightInfo highlight_info{0, 0};

  static auto compareLexigraphically(const LocatorFilterEntry &lhs, const LocatorFilterEntry &rhs) -> bool
  {
    const auto cmp = lhs.display_name.compare(rhs.display_name);

    if (cmp < 0)
      return true;

    if (cmp > 0)
      return false;

    return lhs.extra_info < rhs.extra_info;
  }
};

class CORE_EXPORT ILocatorFilter : public QObject {
  Q_OBJECT

public:
  enum class MatchLevel {
    Best = 0,
    Better,
    Good,
    Normal,
    Count
  };

  enum Priority {
    Highest = 0,
    High = 1,
    Medium = 2,
    Low = 3
  };

  ILocatorFilter(QObject *parent = nullptr);
  ~ILocatorFilter() override;

  static auto allLocatorFilters() -> QList<ILocatorFilter*>;
  auto id() const -> Utils::Id;
  auto actionId() const -> Utils::Id;
  auto displayName() const -> QString;
  auto setDisplayName(const QString &display_string) -> void;
  auto description() const -> QString;
  auto setDescription(const QString &description) -> void;
  auto priority() const -> Priority;
  auto shortcutString() const -> QString;
  auto setDefaultShortcutString(const QString &shortcut) -> void;
  auto setShortcutString(const QString &shortcut) -> void;
  virtual auto prepareSearch(const QString &entry) -> void;
  virtual auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> = 0;
  virtual auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void = 0;

  virtual auto refresh(QFutureInterface<void> &future) -> void
  {
    Q_UNUSED(future)
  }

  virtual auto saveState() const -> QByteArray;
  virtual auto restoreState(const QByteArray &state) -> void;
  virtual auto openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool;
  auto isConfigurable() const -> bool;
  auto isIncludedByDefault() const -> bool;
  auto setDefaultIncludedByDefault(bool included_by_default) -> void;
  auto setIncludedByDefault(bool included_by_default) -> void;
  auto isHidden() const -> bool;
  auto isEnabled() const -> bool;
  static auto caseSensitivity(const QString &str) -> Qt::CaseSensitivity;
  static auto createRegExp(const QString &text, Qt::CaseSensitivity case_sensitivity = Qt::CaseInsensitive) -> QRegularExpression;
  static auto highlightInfo(const QRegularExpressionMatch &match, LocatorFilterEntry::HighlightInfo::DataType data_type = LocatorFilterEntry::HighlightInfo::DisplayName) -> LocatorFilterEntry::HighlightInfo;
  static auto msgConfigureDialogTitle() -> QString;
  static auto msgPrefixLabel() -> QString;
  static auto msgPrefixToolTip() -> QString;
  static auto msgIncludeByDefault() -> QString;
  static auto msgIncludeByDefaultToolTip() -> QString;

public slots:
  auto setEnabled(bool enabled) -> void;

protected:
  auto setHidden(bool hidden) -> void;
  auto setId(Utils::Id id) -> void;
  auto setPriority(Priority priority) -> void;
  auto setConfigurable(bool configurable) -> void;
  auto openConfigDialog(QWidget *parent, QWidget *additional_widget) -> bool;
  virtual auto saveState(QJsonObject &object) const -> void;
  virtual auto restoreState(const QJsonObject &object) -> void;
  static auto isOldSetting(const QByteArray &state) -> bool;

private:
  Utils::Id m_id;
  QString m_shortcut;
  Priority m_priority = Medium;
  QString m_display_name;
  QString m_description;
  QString m_default_shortcut;
  bool m_default_included_by_default = false;
  bool m_included_by_default = m_default_included_by_default;
  bool m_hidden = false;
  bool m_enabled = true;
  bool m_is_configurable = true;
};

} // namespace Core
