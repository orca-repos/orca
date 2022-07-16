// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textmark.hpp"

#include "fontsettings.hpp"
#include "textdocument.hpp"
#include "texteditor.hpp"
#include "texteditorplugin.hpp"

#include <core/core-editor-manager.hpp>
#include <core/core-document-manager.hpp>
#include <core/core-interface.hpp>

#include <utils/qtcassert.hpp>
#include <utils/tooltip/tooltip.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QGridLayout>
#include <QPainter>
#include <QToolButton>

using namespace Orca::Plugin::Core;
using namespace Utils;
using namespace TextEditor::Internal;

namespace TextEditor {

class TextMarkRegistry : public QObject {
  Q_OBJECT

public:
  static auto add(TextMark *mark) -> void;
  static auto remove(TextMark *mark) -> bool;

private:
  TextMarkRegistry(QObject *parent);

  static auto instance() -> TextMarkRegistry*;
  auto editorOpened(IEditor *editor) -> void;
  auto documentRenamed(IDocument *document, const FilePath &oldPath, const FilePath &newPath) -> void;
  auto allDocumentsRenamed(const FilePath &oldPath, const FilePath &newPath) -> void;

  QHash<FilePath, QSet<TextMark*>> m_marks;
};

class AnnotationColors {
public:
  static auto getAnnotationColors(const QColor &markColor, const QColor &backgroundColor) -> AnnotationColors&;
  
  using SourceColors = QPair<QColor, QColor>;
  QColor rectColor;
  QColor textColor;

private:
  static QHash<SourceColors, AnnotationColors> m_colorCache;
};

TextMarkRegistry *m_instance = nullptr;

TextMark::TextMark(const FilePath &fileName, int lineNumber, Id category, double widthFactor) : m_fileName(fileName), m_lineNumber(lineNumber), m_visible(true), m_category(category), m_widthFactor(widthFactor)
{
  if (!m_fileName.isEmpty())
    TextMarkRegistry::add(this);
}

TextMark::~TextMark()
{
  qDeleteAll(m_actions);
  m_actions.clear();
  delete m_settingsAction;
  if (!m_fileName.isEmpty())
    TextMarkRegistry::remove(this);
  if (m_baseTextDocument)
    m_baseTextDocument->removeMark(this);
  m_baseTextDocument = nullptr;
}

auto TextMark::fileName() const -> FilePath
{
  return m_fileName;
}

auto TextMark::updateFileName(const FilePath &fileName) -> void
{
  if (fileName == m_fileName)
    return;
  if (!m_fileName.isEmpty())
    TextMarkRegistry::remove(this);
  m_fileName = fileName;
  if (!m_fileName.isEmpty())
    TextMarkRegistry::add(this);
}

auto TextMark::lineNumber() const -> int
{
  return m_lineNumber;
}

auto TextMark::paintIcon(QPainter *painter, const QRect &rect) const -> void
{
  icon().paint(painter, rect, Qt::AlignCenter);
}

auto TextMark::paintAnnotation(QPainter &painter, QRectF *annotationRect, const qreal fadeInOffset, const qreal fadeOutOffset, const QPointF &contentOffset) const -> void
{
  const auto text = lineAnnotation();
  if (text.isEmpty())
    return;

  const auto &rects = annotationRects(*annotationRect, painter.fontMetrics(), fadeInOffset, fadeOutOffset);
  const QColor &markColor = m_color.has_value() ? Utils::orcaTheme()->color(m_color.value()).toHsl() : painter.pen().color();

  const auto &fontSettings = m_baseTextDocument->fontSettings();
  const AnnotationColors &colors = AnnotationColors::getAnnotationColors(markColor, fontSettings.toTextCharFormat(C_TEXT).background().color());

  painter.save();
  QLinearGradient grad(rects.fadeInRect.topLeft() - contentOffset, rects.fadeInRect.topRight() - contentOffset);
  grad.setColorAt(0.0, Qt::transparent);
  grad.setColorAt(1.0, colors.rectColor);
  painter.fillRect(rects.fadeInRect, grad);
  painter.fillRect(rects.annotationRect, colors.rectColor);
  painter.setPen(colors.textColor);
  paintIcon(&painter, rects.iconRect.toAlignedRect());
  painter.drawText(rects.textRect, Qt::AlignLeft, rects.text);
  if (rects.fadeOutRect.isValid()) {
    grad = QLinearGradient(rects.fadeOutRect.topLeft() - contentOffset, rects.fadeOutRect.topRight() - contentOffset);
    grad.setColorAt(0.0, colors.rectColor);
    grad.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rects.fadeOutRect, grad);
  }
  painter.restore();
  annotationRect->setRight(rects.fadeOutRect.right());
}

auto TextMark::annotationRects(const QRectF &boundingRect, const QFontMetrics &fm, const qreal fadeInOffset, const qreal fadeOutOffset) const -> AnnotationRects
{
  AnnotationRects rects;
  rects.text = lineAnnotation();
  if (rects.text.isEmpty())
    return rects;
  rects.fadeInRect = boundingRect;
  rects.fadeInRect.setWidth(fadeInOffset);
  rects.annotationRect = boundingRect;
  rects.annotationRect.setLeft(rects.fadeInRect.right());
  const auto drawIcon = !icon().isNull();
  constexpr qreal margin = 1;
  rects.iconRect = QRectF(rects.annotationRect.left(), boundingRect.top(), 0, boundingRect.height());
  if (drawIcon)
    rects.iconRect.setWidth(rects.iconRect.height() * m_widthFactor);
  rects.textRect = QRectF(rects.iconRect.right() + margin, boundingRect.top(), qreal(fm.horizontalAdvance(rects.text)), boundingRect.height());
  rects.annotationRect.setRight(rects.textRect.right() + margin);
  if (rects.annotationRect.right() > boundingRect.right()) {
    rects.textRect.setRight(boundingRect.right() - margin);
    rects.text = fm.elidedText(rects.text, Qt::ElideRight, int(rects.textRect.width()));
    rects.annotationRect.setRight(boundingRect.right());
    rects.fadeOutRect = QRectF(rects.annotationRect.topRight(), rects.annotationRect.bottomRight());
  } else {
    rects.fadeOutRect = boundingRect;
    rects.fadeOutRect.setLeft(rects.annotationRect.right());
    rects.fadeOutRect.setWidth(fadeOutOffset);
  }
  return rects;
}

auto TextMark::updateLineNumber(int lineNumber) -> void
{
  m_lineNumber = lineNumber;
}

auto TextMark::move(int line) -> void
{
  if (line == m_lineNumber)
    return;
  const auto previousLine = m_lineNumber;
  m_lineNumber = line;
  if (m_baseTextDocument)
    m_baseTextDocument->moveMark(this, previousLine);
}

auto TextMark::updateBlock(const QTextBlock &) -> void {}

auto TextMark::removedFromEditor() -> void {}

auto TextMark::updateMarker() -> void
{
  if (m_baseTextDocument)
    m_baseTextDocument->updateMark(this);
}

auto TextMark::setPriority(Priority prioriy) -> void
{
  m_priority = prioriy;
  updateMarker();
}

auto TextMark::isVisible() const -> bool
{
  return m_visible;
}

auto TextMark::setVisible(bool visible) -> void
{
  m_visible = visible;
  updateMarker();
}

auto TextMark::widthFactor() const -> double
{
  return m_widthFactor;
}

auto TextMark::setWidthFactor(double factor) -> void
{
  m_widthFactor = factor;
}

auto TextMark::isClickable() const -> bool
{
  return false;
}

auto TextMark::clicked() -> void {}

auto TextMark::isDraggable() const -> bool
{
  return false;
}

auto TextMark::dragToLine(int lineNumber) -> void
{
  Q_UNUSED(lineNumber)
}

auto TextMark::addToToolTipLayout(QGridLayout *target) const -> void
{
  const auto contentLayout = new QVBoxLayout;
  addToolTipContent(contentLayout);
  if (contentLayout->count() <= 0)
    return;

  // Left column: text mark icon
  const auto row = target->rowCount();
  const auto icon = this->icon();
  if (!icon.isNull()) {
    const auto iconLabel = new QLabel;
    iconLabel->setPixmap(icon.pixmap(16, 16));
    target->addWidget(iconLabel, row, 0, Qt::AlignTop | Qt::AlignHCenter);
  }

  // Middle column: tooltip content
  target->addLayout(contentLayout, row, 1);

  // Right column: action icons/button
  auto actions = m_actions;
  if (m_settingsAction)
    actions << m_settingsAction;
  if (!actions.isEmpty()) {
    const auto actionsLayout = new QHBoxLayout;
    auto margins = actionsLayout->contentsMargins();
    margins.setLeft(margins.left() + 5);
    actionsLayout->setContentsMargins(margins);
    for (const auto action : qAsConst(actions)) {
      QTC_ASSERT(!action->icon().isNull(), continue);
      const auto button = new QToolButton;
      button->setIcon(action->icon());
      button->setToolTip(action->toolTip());
      QObject::connect(button, &QToolButton::clicked, action, &QAction::triggered);
      QObject::connect(button, &QToolButton::clicked, []() {
        ToolTip::hideImmediately();
      });
      actionsLayout->addWidget(button, 0, Qt::AlignTop | Qt::AlignRight);
    }
    target->addLayout(actionsLayout, row, 2);
  }
}

auto TextMark::addToolTipContent(QLayout *target) const -> bool
{
  auto useDefaultToolTip = false;
  auto text = toolTip();
  if (text.isEmpty()) {
    useDefaultToolTip = true;
    text = m_defaultToolTip;
    if (text.isEmpty())
      return false;
  }

  const auto textLabel = new QLabel;
  textLabel->setOpenExternalLinks(true);
  textLabel->setText(text);
  // Differentiate between tool tips that where explicitly set and default tool tips.
  textLabel->setDisabled(useDefaultToolTip);
  target->addWidget(textLabel);

  return true;
}

auto TextMark::setIcon(const QIcon &icon) -> void
{
  m_icon = icon;
  m_iconProvider = std::function<QIcon()>();
}

auto TextMark::setIconProvider(const std::function<QIcon ()> &iconProvider) -> void
{
  m_iconProvider = iconProvider;
}

auto TextMark::icon() const -> const QIcon
{
  return m_iconProvider ? m_iconProvider() : m_icon;
}

auto TextMark::color() const -> optional<Theme::Color>
{
  return m_color;
}

auto TextMark::setColor(const Theme::Color &color) -> void
{
  m_color = color;
}

auto TextMark::setToolTipProvider(const std::function<QString()> &toolTipProvider) -> void
{
  m_toolTipProvider = toolTipProvider;
}

auto TextMark::toolTip() const -> QString
{
  return m_toolTipProvider ? m_toolTipProvider() : m_toolTip;
}

auto TextMark::setToolTip(const QString &toolTip) -> void
{
  m_toolTip = toolTip;
  m_toolTipProvider = std::function<QString()>();
}

auto TextMark::actions() const -> QVector<QAction*>
{
  return m_actions;
}

auto TextMark::setActions(const QVector<QAction*> &actions) -> void
{
  m_actions = actions;
}

auto TextMark::setSettingsPage(Id settingsPage) -> void
{
  delete m_settingsAction;
  m_settingsAction = new QAction;
  m_settingsAction->setIcon(Icons::SETTINGS_TOOLBAR.icon());
  m_settingsAction->setToolTip(tr("Show Diagnostic Settings"));
  QObject::connect(m_settingsAction, &QAction::triggered, Orca::Plugin::Core::ICore::instance(), [settingsPage] { Orca::Plugin::Core::ICore::showOptionsDialog(settingsPage); }, Qt::QueuedConnection);
}

TextMarkRegistry::TextMarkRegistry(QObject *parent) : QObject(parent)
{
  connect(EditorManager::instance(), &EditorManager::editorOpened, this, &TextMarkRegistry::editorOpened);

  connect(DocumentManager::instance(), &DocumentManager::allDocumentsRenamed, this, &TextMarkRegistry::allDocumentsRenamed);
  connect(DocumentManager::instance(), &DocumentManager::documentRenamed, this, &TextMarkRegistry::documentRenamed);
}

auto TextMarkRegistry::add(TextMark *mark) -> void
{
  instance()->m_marks[mark->fileName()].insert(mark);
  if (const auto document = TextDocument::textDocumentForFilePath(mark->fileName()))
    document->addMark(mark);
}

auto TextMarkRegistry::remove(TextMark *mark) -> bool
{
  return instance()->m_marks[mark->fileName()].remove(mark);
}

auto TextMarkRegistry::instance() -> TextMarkRegistry*
{
  if (!m_instance)
    m_instance = new TextMarkRegistry(TextEditorPlugin::instance());
  return m_instance;
}

auto TextMarkRegistry::editorOpened(IEditor *editor) -> void
{
  auto document = qobject_cast<TextDocument*>(editor ? editor->document() : nullptr);
  if (!document)
    return;
  if (!m_marks.contains(document->filePath()))
    return;

  foreach(TextMark *mark, m_marks.value(document->filePath()))
    document->addMark(mark);
}

auto TextMarkRegistry::documentRenamed(IDocument *document, const FilePath &oldPath, const FilePath &newPath) -> void
{
  auto baseTextDocument = qobject_cast<TextDocument*>(document);
  if (!baseTextDocument)
    return;
  if (!m_marks.contains(oldPath))
    return;

  QSet<TextMark*> toBeMoved;
  foreach(TextMark *mark, baseTextDocument->marks())
    toBeMoved.insert(mark);

  m_marks[oldPath].subtract(toBeMoved);
  m_marks[newPath].unite(toBeMoved);

  foreach(TextMark *mark, toBeMoved)
    mark->updateFileName(newPath);
}

auto TextMarkRegistry::allDocumentsRenamed(const FilePath &oldPath, const FilePath &newPath) -> void
{
  if (!m_marks.contains(oldPath))
    return;

  auto oldFileNameMarks = m_marks.value(oldPath);

  m_marks[newPath].unite(oldFileNameMarks);
  m_marks[oldPath].clear();

  foreach(TextMark *mark, oldFileNameMarks)
    mark->updateFileName(newPath);
}

QHash<AnnotationColors::SourceColors, AnnotationColors> AnnotationColors::m_colorCache;

auto AnnotationColors::getAnnotationColors(const QColor &markColor, const QColor &backgroundColor) -> AnnotationColors&
{
  auto highClipHsl = [](qreal value) {
    return std::max(0.7, std::min(0.9, value));
  };
  auto lowClipHsl = [](qreal value) {
    return std::max(0.1, std::min(0.3, value));
  };
  auto &colors = m_colorCache[{markColor, backgroundColor}];
  if (!colors.rectColor.isValid() || !colors.textColor.isValid()) {
    const double backgroundLightness = backgroundColor.lightnessF();
    const auto foregroundLightness = backgroundLightness > 0.5 ? lowClipHsl(backgroundLightness - 0.5) : highClipHsl(backgroundLightness + 0.5);

    colors.rectColor = markColor;
    colors.rectColor.setAlphaF(0.15f);

    colors.textColor.setHslF(markColor.hslHueF(), markColor.hslSaturationF(), foregroundLightness);
  }
  return colors;
}

} // namespace TextEditor

#include "textmark.moc"
