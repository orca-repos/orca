// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <utils/fileutils.hpp>
#include <utils/id.hpp>
#include <utils/optional.hpp>
#include <utils/theme/theme.hpp>

#include <QCoreApplication>
#include <QIcon>

QT_BEGIN_NAMESPACE
class QAction;
class QGridLayout;
class QLayout;
class QPainter;
class QRect;
class QTextBlock;
QT_END_NAMESPACE

namespace TextEditor {

class TextDocument;

class TEXTEDITOR_EXPORT TextMark {
  Q_DECLARE_TR_FUNCTIONS(TextEditor::TextMark)

public:
  TextMark(const Utils::FilePath &fileName, int lineNumber, Utils::Id category, double widthFactor = 1.0);
  TextMark() = delete;
  virtual ~TextMark();

  // determine order on markers on the same line.
  enum Priority {
    LowPriority,
    NormalPriority,
    HighPriority // shown on top.
  };

  auto fileName() const -> Utils::FilePath;
  auto lineNumber() const -> int;

  virtual auto paintIcon(QPainter *painter, const QRect &rect) const -> void;
  virtual auto paintAnnotation(QPainter &painter, QRectF *annotationRect, const qreal fadeInOffset, const qreal fadeOutOffset, const QPointF &contentOffset) const -> void;

  struct AnnotationRects {
    QRectF fadeInRect;
    QRectF annotationRect;
    QRectF iconRect;
    QRectF textRect;
    QRectF fadeOutRect;
    QString text;
  };

  auto annotationRects(const QRectF &boundingRect, const QFontMetrics &fm, const qreal fadeInOffset, const qreal fadeOutOffset) const -> AnnotationRects;
  /// called if the filename of the document changed
  virtual auto updateFileName(const Utils::FilePath &fileName) -> void;
  virtual auto updateLineNumber(int lineNumber) -> void;
  virtual auto updateBlock(const QTextBlock &block) -> void;
  virtual auto move(int line) -> void;
  virtual auto removedFromEditor() -> void;
  virtual auto isClickable() const -> bool;
  virtual auto clicked() -> void;
  virtual auto isDraggable() const -> bool;
  virtual auto dragToLine(int lineNumber) -> void;
  auto addToToolTipLayout(QGridLayout *target) const -> void;
  virtual auto addToolTipContent(QLayout *target) const -> bool;

  auto setIcon(const QIcon &icon) -> void;
  auto setIconProvider(const std::function<QIcon()> &iconProvider) -> void;
  auto icon() const -> const QIcon;
  // call this if the icon has changed.
  auto updateMarker() -> void;
  auto priority() const -> Priority { return m_priority; }
  auto setPriority(Priority prioriy) -> void;
  auto isVisible() const -> bool;
  auto setVisible(bool isVisible) -> void;
  auto category() const -> Utils::Id { return m_category; }
  auto widthFactor() const -> double;
  auto setWidthFactor(double factor) -> void;
  auto color() const -> Utils::optional<Utils::Theme::Color>;
  auto setColor(const Utils::Theme::Color &color) -> void;
  auto defaultToolTip() const -> QString { return m_defaultToolTip; }
  auto setDefaultToolTip(const QString &toolTip) -> void { m_defaultToolTip = toolTip; }
  auto baseTextDocument() const -> TextDocument* { return m_baseTextDocument; }
  auto setBaseTextDocument(TextDocument *baseTextDocument) -> void { m_baseTextDocument = baseTextDocument; }
  auto lineAnnotation() const -> QString { return m_lineAnnotation; }
  auto setLineAnnotation(const QString &lineAnnotation) -> void { m_lineAnnotation = lineAnnotation; }
  auto toolTip() const -> QString;
  auto setToolTip(const QString &toolTip) -> void;
  auto setToolTipProvider(const std::function<QString ()> &toolTipProvider) -> void;
  auto actions() const -> QVector<QAction*>;
  auto setActions(const QVector<QAction*> &actions) -> void; // Takes ownership

protected:
  auto setSettingsPage(Utils::Id settingsPage) -> void;

private:
  Q_DISABLE_COPY(TextMark)

  TextDocument *m_baseTextDocument = nullptr;
  Utils::FilePath m_fileName;
  int m_lineNumber = 0;
  Priority m_priority = LowPriority;
  QIcon m_icon;
  std::function<QIcon()> m_iconProvider;
  Utils::optional<Utils::Theme::Color> m_color;
  bool m_visible = false;
  Utils::Id m_category;
  double m_widthFactor = 1.0;
  QString m_lineAnnotation;
  QString m_toolTip;
  std::function<QString()> m_toolTipProvider;
  QString m_defaultToolTip;
  QVector<QAction*> m_actions;
  QAction *m_settingsAction = nullptr;
};

} // namespace TextEditor
