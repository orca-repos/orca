// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <utils/icon.h>
#include <utils/id.h>

#include <QObject>
#include <QPointer>
#include <QWidget>

#include <functional>

namespace Utils {
class AspectContainer;
};

namespace Core {

class CORE_EXPORT IOptionsPageWidget : public QWidget {
  Q_OBJECT

public:
  virtual auto apply() -> void = 0;
  virtual auto finish() -> void {}
};

class CORE_EXPORT IOptionsPage : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(IOptionsPage)

public:
  explicit IOptionsPage(QObject *parent = nullptr, bool register_globally = true);
  ~IOptionsPage() override;

  using widget_creator = std::function<IOptionsPageWidget *()>;

  static auto allOptionsPages() -> QList<IOptionsPage*>;
  auto id() const -> Utils::Id { return m_id; }
  auto displayName() const -> QString { return m_display_name; }
  auto category() const -> Utils::Id { return m_category; }
  auto displayCategory() const -> QString { return m_display_category; }
  auto categoryIcon() const -> QIcon;
  auto setWidgetCreator(const widget_creator &widget_creator) -> void;
  virtual auto matches(const QRegularExpression &regexp) const -> bool;
  virtual auto widget() -> QWidget*;
  virtual auto apply() -> void;
  virtual auto finish() -> void;

protected:
  auto setId(const Utils::Id id) -> void { m_id = id; }
  auto setDisplayName(const QString &display_name) -> void { m_display_name = display_name; }
  auto setCategory(const Utils::Id category) -> void { m_category = category; }
  auto setDisplayCategory(const QString &display_category) -> void { m_display_category = display_category; }
  auto setCategoryIcon(const Utils::Icon &category_icon) -> void { m_category_icon = category_icon; }
  auto setCategoryIconPath(const Utils::FilePath &category_icon_path) -> void;
  auto setSettings(Utils::AspectContainer *settings) -> void;
  auto setLayouter(const std::function<void(QWidget *w)> &layouter) -> void;

  // Used in FontSettingsPage. FIXME?
  QPointer<QWidget> m_widget; // Used in conjunction with m_widgetCreator

private:
  Utils::Id m_id;
  Utils::Id m_category;
  QString m_display_name;
  QString m_display_category;
  Utils::Icon m_category_icon;
  widget_creator m_widget_creator;
  mutable bool m_keywords_initialized = false;
  mutable QStringList m_keywords;
  Utils::AspectContainer *m_settings = nullptr;
  std::function<void(QWidget *w)> m_layouter;
};

/*
    Alternative way for providing option pages instead of adding IOptionsPage
    objects into the plugin manager pool. Should only be used if creation of the
    actual option pages is not possible or too expensive at Orca startup.
    (Like the designer integration, which needs to initialize designer plugins
    before the options pages get available.)
*/

class CORE_EXPORT IOptionsPageProvider : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(IOptionsPageProvider)

public:
  explicit IOptionsPageProvider(QObject *parent = nullptr);
  ~IOptionsPageProvider() override;

  static auto allOptionsPagesProviders() -> QList<IOptionsPageProvider*>;
  auto category() const -> Utils::Id { return m_category; }
  auto displayCategory() const -> QString { return m_display_category; }
  auto categoryIcon() const -> QIcon;
  virtual auto pages() const -> QList<IOptionsPage*> = 0;
  virtual auto matches(const QRegularExpression &regexp) const -> bool = 0;

protected:
  auto setCategory(const Utils::Id category) -> void { m_category = category; }
  auto setDisplayCategory(const QString &display_category) -> void { m_display_category = display_category; }
  auto setCategoryIcon(const Utils::Icon &category_icon) -> void { m_category_icon = category_icon; }

  Utils::Id m_category;
  QString m_display_category;
  Utils::Icon m_category_icon;
};

} // namespace Core
