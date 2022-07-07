// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "id.hpp"
#include "utils_global.hpp"

#include <QFrame>
#include <QObject>
#include <QSet>

#include <functional>

QT_BEGIN_NAMESPACE
class QBoxLayout;
class QSettings;
QT_END_NAMESPACE

namespace Utils {

class InfoBar;
class InfoBarDisplay;
class Theme;

class ORCA_UTILS_EXPORT InfoBarEntry {
public:
  enum class GlobalSuppression {
    Disabled,
    Enabled
  };

  InfoBarEntry(Id _id, const QString &_infoText, GlobalSuppression _globalSuppression = GlobalSuppression::Disabled);

  using CallBack = std::function<void()>;
  auto addCustomButton(const QString &_buttonText, CallBack callBack) -> void;
  auto setCancelButtonInfo(CallBack callBack) -> void;
  auto setCancelButtonInfo(const QString &_cancelButtonText, CallBack callBack) -> void;
  using ComboCallBack = std::function<void(const QString &)>;
  auto setComboInfo(const QStringList &list, ComboCallBack callBack) -> void;
  auto removeCancelButton() -> void;

  using DetailsWidgetCreator = std::function<QWidget*()>;
  auto setDetailsWidgetCreator(const DetailsWidgetCreator &creator) -> void;

private:
  struct Button {
    QString text;
    CallBack callback;
  };

  Id m_id;
  QString m_infoText;
  QList<Button> m_buttons;
  QString m_cancelButtonText;
  CallBack m_cancelButtonCallBack;
  GlobalSuppression m_globalSuppression;
  DetailsWidgetCreator m_detailsWidgetCreator;
  bool m_useCancelButton = true;
  ComboCallBack m_comboCallBack;
  QStringList m_comboInfo;
  friend class InfoBar;
  friend class InfoBarDisplay;
};

class ORCA_UTILS_EXPORT InfoBar : public QObject {
  Q_OBJECT

public:
  auto addInfo(const InfoBarEntry &info) -> void;
  auto removeInfo(Id id) -> void;
  auto containsInfo(Id id) const -> bool;
  auto suppressInfo(Id id) -> void;
  auto canInfoBeAdded(Id id) const -> bool;
  auto unsuppressInfo(Id id) -> void;
  auto clear() -> void;
  static auto globallySuppressInfo(Id id) -> void;
  static auto globallyUnsuppressInfo(Id id) -> void;
  static auto clearGloballySuppressed() -> void;
  static auto anyGloballySuppressed() -> bool;
  static auto initialize(QSettings *settings) -> void;

signals:
  auto changed() -> void;

private:
  static auto writeGloballySuppressedToSettings() -> void;

private:
  QList<InfoBarEntry> m_infoBarEntries;
  QSet<Id> m_suppressed;

  static QSet<Id> globallySuppressed;
  static QSettings *m_settings;

  friend class InfoBarDisplay;
};

class ORCA_UTILS_EXPORT InfoBarDisplay : public QObject {
  Q_OBJECT

public:
  InfoBarDisplay(QObject *parent = nullptr);
  auto setTarget(QBoxLayout *layout, int index) -> void;
  auto setInfoBar(InfoBar *infoBar) -> void;
  auto setEdge(Qt::Edge edge) -> void;

  auto infoBar() const -> InfoBar*;

private:
  auto update() -> void;
  auto infoBarDestroyed() -> void;
  auto widgetDestroyed() -> void;

  QList<QWidget*> m_infoWidgets;
  InfoBar *m_infoBar = nullptr;
  QBoxLayout *m_boxLayout = nullptr;
  Qt::Edge m_edge = Qt::TopEdge;
  int m_boxIndex = 0;
  bool m_isShowingDetailsWidget = false;
};

} // namespace Utils
