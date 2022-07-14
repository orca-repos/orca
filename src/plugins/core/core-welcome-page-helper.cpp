// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-welcome-page-helper.hpp"

#include <utils/algorithm.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/stylehelper.hpp>
#include <utils/theme/theme.hpp>

#include <QEasingCurve>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHoverEvent>
#include <QLayout>
#include <QPainter>
#include <QPixmapCache>
#include <QTimer>

#include <qdrawutil.h>

namespace Orca::Plugin::Core {

using namespace Utils;

static auto themeColor(const Theme::Color role) -> QColor
{
  return orcaTheme()->color(role);
}

static auto sizedFont(const int size, const QWidget *widget) -> QFont
{
  auto f = widget->font();
  f.setPixelSize(size);
  return f;
}

auto brandFont() -> QFont
{
  const static auto f = [] {
    const auto id = QFontDatabase::addApplicationFont(":/studiofonts/TitilliumWeb-Regular.ttf");
    QFont result;
    result.setPixelSize(16);
    if (id >= 0) {
      const auto font_families = QFontDatabase::applicationFontFamilies(id);
      result.setFamilies(font_families);
    }
    return result;
  }();
  return f;
}

auto panelBar(QWidget *parent) -> QWidget*
{
  const auto frame = new QWidget(parent);
  frame->setAutoFillBackground(true);
  frame->setMinimumWidth(G_H_SPACING);
  QPalette pal;
  pal.setBrush(QPalette::Window, {});
  pal.setColor(QPalette::Window, themeColor(Theme::Welcome_BackgroundPrimaryColor));
  frame->setPalette(pal);
  return frame;
}

SearchBox::SearchBox(QWidget *parent) : WelcomePageFrame(parent)
{
  setAutoFillBackground(true);

  m_line_edit = new FancyLineEdit;
  m_line_edit->setFiltering(true);
  m_line_edit->setFrame(false);
  m_line_edit->setFont(brandFont());
  m_line_edit->setMinimumHeight(33);
  m_line_edit->setAttribute(Qt::WA_MacShowFocusRect, false);

  auto pal = buttonPalette(false, false, true);
  // for the margins
  pal.setColor(QPalette::Window, m_line_edit->palette().color(QPalette::Base));
  // for macOS dark mode
  pal.setColor(QPalette::WindowText, themeColor(Theme::Welcome_ForegroundPrimaryColor));
  pal.setColor(QPalette::Text, themeColor(Theme::Welcome_TextColor));
  setPalette(pal);

  const auto box = new QHBoxLayout(this);
  box->setContentsMargins(10, 0, 1, 0);
  box->addWidget(m_line_edit);
}

GridView::GridView(QWidget *parent) : QListView(parent)
{
  setResizeMode(Adjust);
  setMouseTracking(true); // To enable hover.
  setSelectionMode(NoSelection);
  setFrameShape(NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setViewMode(IconMode);
  setUniformItemSizes(true);

  QPalette pal;
  pal.setColor(QPalette::Base, themeColor(Theme::Welcome_BackgroundSecondaryColor));
  setPalette(pal); // Makes a difference on Mac.
}

auto GridView::leaveEvent(QEvent *) -> void
{
  QHoverEvent hev(QEvent::HoverLeave, QPointF(), QPointF());
  viewportEvent(&hev); // Seemingly needed to kill the hover paint.
}

constexpr QSize ListModel::default_image_size(214, 160);

ListModel::ListModel(QObject *parent) : QAbstractListModel(parent) {}

ListModel::~ListModel()
{
  qDeleteAll(m_items);
  m_items.clear();
}

auto ListModel::rowCount(const QModelIndex &) const -> int
{
  return static_cast<int>(m_items.size());
}

auto ListModel::data(const QModelIndex &index, const int role) const -> QVariant
{
  if (!index.isValid() || index.row() >= m_items.count())
    return {};

  const auto item = m_items.at(index.row());

  switch (role) {
  case Qt::DisplayRole: // for search only
    return QString(item->name + ' ' + item->tags.join(' '));
  case ItemRole:
    return QVariant::fromValue(item);
  case ItemImageRole: {
    QPixmap pixmap;
    if (QPixmapCache::find(item->image_url, &pixmap))
      return pixmap;
    if (pixmap.isNull())
      pixmap = fetchPixmapAndUpdatePixmapCache(item->image_url);
    return pixmap;
  }
  case ItemTagsRole:
    return item->tags;
  default:
    return {};
  }
}

ListModelFilter::ListModelFilter(ListModel *source_model, QObject *parent) : QSortFilterProxyModel(parent)
{
  QSortFilterProxyModel::setSourceModel(source_model);
  setDynamicSortFilter(true);
  setFilterCaseSensitivity(Qt::CaseInsensitive);
  QSortFilterProxyModel::sort(0);
}

auto ListModelFilter::filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const -> bool
{
  const ListItem *item = sourceModel()->index(source_row, 0, source_parent).data(ListModel::ItemRole).value<ListItem*>();

  if (!item)
    return false;

  bool early_exit_result;

  if (leaveFilterAcceptsRowBeforeFiltering(item, &early_exit_result))
    return early_exit_result;

  if (!m_filter_tags.isEmpty()) {
    return allOf(m_filter_tags, [&item](const QString &filter_tag) {
      return item->tags.contains(filter_tag, Qt::CaseInsensitive);
    });
  }

  if (!m_filter_strings.isEmpty()) {
    for (const auto &sub_string : m_filter_strings) {
      auto word_match = false;
      word_match |= item->name.contains(sub_string, Qt::CaseInsensitive);
      if (word_match)
        continue;
      const auto sub_match = [&sub_string](const QString &elem) {
        return elem.contains(sub_string, Qt::CaseInsensitive);
      };
      word_match |= contains(item->tags, sub_match);
      if (word_match)
        continue;
      word_match |= item->description.contains(sub_string, Qt::CaseInsensitive);
      if (!word_match)
        return false;
    }
  }

  return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

auto ListModelFilter::delayedUpdateFilter() -> void
{
  if (m_timer_id != 0)
    killTimer(m_timer_id);
  m_timer_id = startTimer(320);
}

auto ListModelFilter::timerEvent(QTimerEvent *timer_event) -> void
{
  if (m_timer_id == timer_event->timerId()) {
    invalidateFilter();
    emit layoutChanged();
    killTimer(m_timer_id);
    m_timer_id = 0;
  }
}

struct SearchStringLexer {
  QString code;
  const QChar *code_ptr;
  QChar yychar;
  QString yytext;

  enum TokenKind {
    END_OF_STRING = 0,
    TAG,
    STRING_LITERAL,
    UNKNOWN
  };

  auto yyinp() -> void { yychar = *code_ptr++; }

  explicit SearchStringLexer(const QString &code) : code(code), code_ptr(code.unicode()), yychar(QLatin1Char(' ')) { }

  auto operator()() -> int { return yylex(); }

  auto yylex() -> int
  {
    while (yychar.isSpace())
      yyinp(); // skip all the spaces

    yytext.clear();

    if (yychar.isNull())
      return END_OF_STRING;

    auto ch = yychar;
    yyinp();

    switch (ch.unicode()) {
    case '"':
    case '\'': {
      const auto quote = ch;
      yytext.clear();
      while (!yychar.isNull()) {
        if (yychar == quote) {
          yyinp();
          break;
        }
        if (yychar == QLatin1Char('\\')) {
          yyinp();
          switch (yychar.unicode()) {
          case '"':
            yytext += QLatin1Char('"');
            yyinp();
            break;
          case '\'':
            yytext += QLatin1Char('\'');
            yyinp();
            break;
          case '\\':
            yytext += QLatin1Char('\\');
            yyinp();
            break;
          }
        } else {
          yytext += yychar;
          yyinp();
        }
      }
      return STRING_LITERAL;
    }
    default:
      if (ch.isLetterOrNumber() || ch == QLatin1Char('_')) {
        yytext.clear();
        yytext += ch;
        while (yychar.isLetterOrNumber() || yychar == QLatin1Char('_')) {
          yytext += yychar;
          yyinp();
        }
        if (yychar == QLatin1Char(':') && yytext == QLatin1String("tag")) {
          yyinp();
          return TAG;
        }
        return STRING_LITERAL;
      }
    }
    yytext += ch;
    return UNKNOWN;
  }
};

auto ListModelFilter::setSearchString(const QString &arg) -> void
{
  if (m_search_string == arg)
    return;

  m_search_string = arg;
  m_filter_tags.clear();
  m_filter_strings.clear();

  // parse and update
  SearchStringLexer lex(arg);
  auto is_tag = false;

  while (const auto tk = lex()) {
    if (tk == SearchStringLexer::TAG) {
      is_tag = true;
      m_filter_strings.append(lex.yytext);
    }
    if (tk == SearchStringLexer::STRING_LITERAL) {
      if (is_tag) {
        m_filter_strings.pop_back();
        m_filter_tags.append(lex.yytext);
        is_tag = false;
      } else {
        m_filter_strings.append(lex.yytext);
      }
    }
  }

  delayedUpdateFilter();
}

auto ListModelFilter::leaveFilterAcceptsRowBeforeFiltering(const ListItem *, bool *) const -> bool
{
  return false;
}

ListItemDelegate::ListItemDelegate() : background_primary_color(themeColor(Theme::Welcome_BackgroundPrimaryColor)), background_secondary_color(themeColor(Theme::Welcome_BackgroundSecondaryColor)), foreground_primary_color(themeColor(Theme::Welcome_ForegroundPrimaryColor)), hover_color(themeColor(Theme::Welcome_HoverColor)), text_color(themeColor(Theme::Welcome_TextColor)) {}

auto ListItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void
{
  const ListItem *item = index.data(ListModel::ItemRole).value<ListItem*>();
  const auto rc = option.rect;
  const QRect tile_rect(0, 0, rc.width() - grid_item_gap, rc.height() - grid_item_gap);
  constexpr auto thumbnail_bg_size = ListModel::default_image_size.grownBy(QMargins(1, 1, 1, 1));
  const QRect thumbnail_bg_rect((tile_rect.width() - thumbnail_bg_size.width()) / 2, grid_item_gap, thumbnail_bg_size.width(), thumbnail_bg_size.height());
  const auto text_area = tile_rect.adjusted(grid_item_gap, grid_item_gap, -grid_item_gap, -grid_item_gap);
  const bool hovered = option.state & QStyle::State_MouseOver;
  constexpr auto tags_base = tags_separator_y + 17;
  constexpr auto shift_y = tags_separator_y - 16;
  constexpr auto name_y = tags_separator_y - 20;
  const auto text_rect = text_area.translated(0, name_y);
  const auto description_font = sizedFont(11, option.widget);

  painter->save();
  painter->translate(rc.topLeft());
  painter->fillRect(tile_rect, hovered ? hover_color : background_primary_color);

  QTextOption wrapped;
  wrapped.setWrapMode(QTextOption::WordWrap);
  auto offset = 0;
  float animation_progress = 0; // Linear increase from 0.0 to 1.0 during hover animation

  if (hovered) {
    if (index != m_previous_index) {
      m_previous_index = index;
      m_current_tag_rects.clear();
      m_blurred_thumbnail = QPixmap();
      m_start_time.start();
      m_current_widget = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(option.widget));
    }
    constexpr float hover_animation_duration = 260;
    animation_progress = m_start_time.elapsed() / hover_animation_duration;
    static const QEasingCurve animation_curve(QEasingCurve::OutCubic);
    offset = animation_curve.valueForProgress(animation_progress) * shift_y;
    if (offset < shift_y)
      QTimer::singleShot(10, this, &ListItemDelegate::goon);
  } else if (index == m_previous_index) {
    m_previous_index = QModelIndex();
  }

  const auto shifted_text_rect = text_rect.adjusted(0, -offset, 0, -offset);

  // The pixmap.
  const auto pm = index.data(ListModel::ItemImageRole).value<QPixmap>();
  auto thumbnail_pos = thumbnail_bg_rect.center();

  if (!pm.isNull()) {
    painter->fillRect(thumbnail_bg_rect, background_secondary_color);
    thumbnail_pos.rx() -= pm.width() / pm.devicePixelRatio() / 2 - 1;
    thumbnail_pos.ry() -= pm.height() / pm.devicePixelRatio() / 2 - 1;
    painter->drawPixmap(thumbnail_pos, pm);
    painter->setPen(foreground_primary_color);
    drawPixmapOverlay(item, painter, option, thumbnail_bg_rect);
  } else {
    // The description text as fallback.
    painter->setPen(text_color);
    painter->setFont(description_font);
    painter->drawText(text_area, item->description, wrapped);
  }

  // The description background
  if (offset) {
    auto background_portion_rect = tile_rect;
    background_portion_rect.setTop(shift_y - offset);
    if (!pm.isNull()) {
      if (m_blurred_thumbnail.isNull()) {
        constexpr auto blur_radius = 50;
        QImage thumbnail(tile_rect.size() + QSize(blur_radius, blur_radius) * 2, QImage::Format_ARGB32_Premultiplied);
        thumbnail.fill(hover_color);
        QPainter thumbnail_painter(&thumbnail);
        thumbnail_painter.translate(blur_radius, blur_radius);
        thumbnail_painter.fillRect(thumbnail_bg_rect, background_secondary_color);
        thumbnail_painter.drawPixmap(thumbnail_pos, pm);
        thumbnail_painter.setPen(foreground_primary_color);
        drawPixmapOverlay(item, &thumbnail_painter, option, thumbnail_bg_rect);
        thumbnail_painter.end();
        m_blurred_thumbnail = QPixmap(tile_rect.size());
        QPainter blurred_thumbnail_painter(&m_blurred_thumbnail);
        blurred_thumbnail_painter.translate(-blur_radius, -blur_radius);
        qt_blurImage(&blurred_thumbnail_painter, thumbnail, blur_radius, false, false);
        blurred_thumbnail_painter.setOpacity(0.825);
        blurred_thumbnail_painter.fillRect(tile_rect, hover_color);
      }
      const auto thumbnail_portion_pm = m_blurred_thumbnail.copy(background_portion_rect);
      painter->drawPixmap(background_portion_rect.topLeft(), thumbnail_portion_pm);
    } else {
      painter->fillRect(background_portion_rect, hover_color);
    }
  }

  // The description Text (unhovered or hovered)
  painter->setPen(text_color);
  painter->setFont(sizedFont(13, option.widget)); // Title font
  if (offset) {
    // The title of the example
    const auto name_rect = painter->boundingRect(shifted_text_rect, item->name, wrapped);
    painter->drawText(name_rect, item->name, wrapped);

    // The separator line below the example title.
    const int ll = name_rect.height() + 3;
    const auto line = QLine(0, ll, text_area.width(), ll).translated(shifted_text_rect.topLeft());
    painter->setPen(foreground_primary_color);
    painter->setOpacity(animation_progress); // "fade in" separator line and description
    painter->drawLine(line);

    // The description text.
    const auto dd = ll + 5;
    const auto desc_rect = shifted_text_rect.adjusted(0, dd, 0, dd);
    painter->setPen(text_color);
    painter->setFont(description_font);
    painter->drawText(desc_rect, item->description, wrapped);
    painter->setOpacity(1);
  } else {
    // The title of the example
    const auto elided_name = painter->fontMetrics().elidedText(item->name, Qt::ElideRight, text_rect.width());
    painter->drawText(text_rect, elided_name);
  }

  // Separator line between text and 'Tags:' section
  painter->setPen(foreground_primary_color);
  painter->drawLine(QLineF(text_area.topLeft(), text_area.topRight()).translated(0, tags_separator_y));

  // The 'Tags:' section
  painter->setPen(foreground_primary_color);
  const auto tags_font = sizedFont(10, option.widget);
  painter->setFont(tags_font);
  const auto fm = painter->fontMetrics();
  const auto tags_label_text = tr("Tags:");
  constexpr auto tags_hor_spacing = 5;
  const auto tags_label_rect = QRect(0, 0, fm.horizontalAdvance(tags_label_text) + tags_hor_spacing, fm.height()).translated(text_area.x(), tags_base);
  painter->drawText(tags_label_rect, tags_label_text);

  painter->setPen(themeColor(Theme::Welcome_LinkColor));
  auto empty_tag_rows_left = 2;
  auto xx = 0;
  auto yy = 0;
  const auto populate_tags_rects = m_current_tag_rects.empty();

  for (const auto &tag : item->tags) {
    const auto ww = fm.horizontalAdvance(tag) + tags_hor_spacing;
    if (xx + ww > text_area.width() - tags_label_rect.width()) {
      if (--empty_tag_rows_left == 0)
        break;
      yy += fm.lineSpacing();
      xx = 0;
    }
    const auto tag_rect = QRect(xx, yy, ww, tags_label_rect.height()).translated(tags_label_rect.topRight());
    painter->drawText(tag_rect, tag);
    if (populate_tags_rects)
      m_current_tag_rects.append({tag, tag_rect});
    xx += ww;
  }

  painter->restore();
}

auto ListItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) -> bool
{
  if (event->type() == QEvent::MouseButtonRelease) {
    const ListItem *item = index.data(ListModel::ItemRole).value<ListItem*>();

    if (!item)
      return false;

    const auto mev = dynamic_cast<QMouseEvent*>(event);

    if (mev->button() != Qt::LeftButton) // do not react on right click
      return false;

    if (index.isValid()) {
      const auto mouse_pos = mev->pos() - option.rect.topLeft();
      if (const auto tag_under_mouse = findOrDefault(m_current_tag_rects, [&mouse_pos](const QPair<QString, QRect> &tag) {
        return tag.second.contains(mouse_pos);
      }); !tag_under_mouse.first.isEmpty()) emit tagClicked(tag_under_mouse.first);
      else
        clickAction(item);
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

auto ListItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const -> QSize
{
  return {grid_item_width, grid_item_height};
}

auto ListItemDelegate::drawPixmapOverlay(const ListItem *, QPainter *, const QStyleOptionViewItem &, const QRect &) const -> void {}

auto ListItemDelegate::clickAction(const ListItem *) const -> void {}

auto ListItemDelegate::goon() const -> void
{
  if (m_current_widget)
    m_current_widget->update(m_previous_index);
}

} // namespace Orca::Plugin::Core
