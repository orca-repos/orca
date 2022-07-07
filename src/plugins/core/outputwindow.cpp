// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "outputwindow.hpp"
#include "coreconstants.hpp"
#include "coreplugin.hpp"
#include "icore.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/find/basetextfind.hpp>

#include <utils/outputformatter.hpp>
#include <utils/qtcassert.hpp>

#include <aggregation/aggregate.hpp>

#include <QAction>
#include <QElapsedTimer>
#include <QHash>
#include <QMimeData>
#include <QPair>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTimer>

#ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
#include <QtTest>
#endif

#include <numeric>

constexpr int g_chunk_size = 10000;

using namespace Utils;

namespace Core {
namespace Internal {

class OutputWindowPrivate {
public:
  explicit OutputWindowPrivate(QTextDocument *document) : cursor(document) { }

  QString settings_key;
  OutputFormatter formatter;
  QList<QPair<QString, OutputFormat>> queued_output;
  QTimer queue_timer;
  bool flush_requested = false;
  bool scroll_to_bottom = true;
  bool links_active = true;
  bool zoom_enabled = false;
  float original_font_size = 0.;
  bool original_read_only = false;
  int max_char_count = Constants::DEFAULT_MAX_CHAR_COUNT;
  Qt::MouseButton mouse_button_pressed = Qt::NoButton;
  QTextCursor cursor;
  QString filter_text;
  int last_filtered_block_number = -1;
  QPalette original_palette;
  OutputWindow::FilterModeFlags filter_mode = OutputWindow::FilterModeFlag::Default;
  QTimer scroll_timer;
  QElapsedTimer last_message;
  QHash<unsigned int, QPair<int, int>> task_positions;
};

} // namespace Internal

/*******************/

OutputWindow::OutputWindow(const Context& context, const QString &settings_key, QWidget *parent) : QPlainTextEdit(parent), d(new Internal::OutputWindowPrivate(document()))
{
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setFrameShape(NoFrame);
  setMouseTracking(true);
  setUndoRedoEnabled(false);

  d->formatter.setPlainTextEdit(this);
  d->queue_timer.setSingleShot(true);
  d->queue_timer.setInterval(10);

  connect(&d->queue_timer, &QTimer::timeout, this, &OutputWindow::handleNextOutputChunk);

  d->settings_key = settings_key;

  const auto output_window_context = new IContext(this);
  output_window_context->setContext(context);
  output_window_context->setWidget(this);

  ICore::addContextObject(output_window_context);

  const auto undo_action = new QAction(this);
  const auto redo_action = new QAction(this);
  const auto cut_action = new QAction(this);
  const auto copy_action = new QAction(this);
  const auto paste_action = new QAction(this);
  const auto select_all_action = new QAction(this);

  ActionManager::registerAction(undo_action, Constants::UNDO, context);
  ActionManager::registerAction(redo_action, Constants::REDO, context);
  ActionManager::registerAction(cut_action, Constants::CUT, context);
  ActionManager::registerAction(copy_action, Constants::COPY, context);
  ActionManager::registerAction(paste_action, Constants::PASTE, context);
  ActionManager::registerAction(select_all_action, Constants::SELECTALL, context);

  connect(undo_action, &QAction::triggered, this, &QPlainTextEdit::undo);
  connect(redo_action, &QAction::triggered, this, &QPlainTextEdit::redo);
  connect(cut_action, &QAction::triggered, this, &QPlainTextEdit::cut);
  connect(copy_action, &QAction::triggered, this, &QPlainTextEdit::copy);
  connect(paste_action, &QAction::triggered, this, &QPlainTextEdit::paste);
  connect(select_all_action, &QAction::triggered, this, &QPlainTextEdit::selectAll);
  connect(this, &QPlainTextEdit::blockCountChanged, this, [this] {
    if (!d->filter_text.isEmpty())
      filterNewContent();
  });
  connect(this, &QPlainTextEdit::undoAvailable, undo_action, &QAction::setEnabled);
  connect(this, &QPlainTextEdit::redoAvailable, redo_action, &QAction::setEnabled);
  connect(this, &QPlainTextEdit::copyAvailable, cut_action, &QAction::setEnabled); // OutputWindow never read-only
  connect(this, &QPlainTextEdit::copyAvailable, copy_action, &QAction::setEnabled);
  connect(ICore::instance(), &ICore::saveSettingsRequested, this, [this] {
    if (!d->settings_key.isEmpty())
      ICore::settings()->setValueWithDefault(d->settings_key, fontZoom(), 0.f);
  });
  connect(outputFormatter(), &OutputFormatter::openInEditorRequested, this, [](const Link &link) {
    EditorManager::openEditorAt(link);
  });
  connect(verticalScrollBar(), &QAbstractSlider::actionTriggered, this, &OutputWindow::updateAutoScroll);
  // For when "Find" changes the position; see ORCABUG-26100.
  connect(this, &QPlainTextEdit::selectionChanged, this, &OutputWindow::updateAutoScroll, Qt::QueuedConnection);

  undo_action->setEnabled(false);
  redo_action->setEnabled(false);
  cut_action->setEnabled(false);
  copy_action->setEnabled(false);

  d->scroll_timer.setInterval(10);
  d->scroll_timer.setSingleShot(true);
  connect(&d->scroll_timer, &QTimer::timeout, this, &OutputWindow::scrollToBottom);
  d->last_message.start();
  d->original_font_size = static_cast<float>(font().pointSizeF());

  if (!d->settings_key.isEmpty()) {
    const auto zoom = ICore::settings()->value(d->settings_key).toFloat();
    setFontZoom(zoom);
  }

  // Let selected text be colored as if the text edit was editable,
  // otherwise the highlight for searching is too light
  auto p = palette();
  const auto active_highlight = p.color(QPalette::Active, QPalette::Highlight);
  p.setColor(QPalette::Highlight, active_highlight);
  const auto active_highlighted_text = p.color(QPalette::Active, QPalette::HighlightedText);
  p.setColor(QPalette::HighlightedText, active_highlighted_text);
  setPalette(p);
  const auto agg = new Aggregation::Aggregate;
  agg->add(this);
  agg->add(new BaseTextFind(this));
}

OutputWindow::~OutputWindow()
{
  delete d;
}

auto OutputWindow::mousePressEvent(QMouseEvent *e) -> void
{
  d->mouse_button_pressed = e->button();
  QPlainTextEdit::mousePressEvent(e);
}

auto OutputWindow::handleLink(const QPoint &pos) -> void
{
  if (const auto href = anchorAt(pos); !href.isEmpty())
    d->formatter.handleLink(href);
}

auto OutputWindow::mouseReleaseEvent(QMouseEvent *e) -> void
{
  if (d->links_active && d->mouse_button_pressed == Qt::LeftButton)
    handleLink(e->pos());

  // Mouse was released, activate links again
  d->links_active = true;
  d->mouse_button_pressed = Qt::NoButton;

  QPlainTextEdit::mouseReleaseEvent(e);
}

auto OutputWindow::mouseMoveEvent(QMouseEvent *e) -> void
{
  // Cursor was dragged to make a selection, deactivate links
  if (d->mouse_button_pressed != Qt::NoButton && textCursor().hasSelection())
    d->links_active = false;

  if (!d->links_active || anchorAt(e->pos()).isEmpty())
    viewport()->setCursor(Qt::IBeamCursor);
  else
    viewport()->setCursor(Qt::PointingHandCursor);

  QPlainTextEdit::mouseMoveEvent(e);
}

auto OutputWindow::resizeEvent(QResizeEvent *e) -> void
{
  //Keep scrollbar at bottom of window while resizing, to ensure we keep scrolling
  //This can happen if window is resized while building, or if the horizontal scrollbar appears
  QPlainTextEdit::resizeEvent(e);
  if (d->scroll_to_bottom)
    scrollToBottom();
}

auto OutputWindow::keyPressEvent(QKeyEvent *ev) -> void
{
  QPlainTextEdit::keyPressEvent(ev);

  //Ensure we scroll also on Ctrl+Home or Ctrl+End
  if (ev->matches(QKeySequence::MoveToStartOfDocument))
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
  else if (ev->matches(QKeySequence::MoveToEndOfDocument))
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMaximum);
}

auto OutputWindow::setLineParsers(const QList<OutputLineParser*> &parsers) -> void
{
  reset();
  d->formatter.setLineParsers(parsers);
}

auto OutputWindow::outputFormatter() const -> OutputFormatter*
{
  return &d->formatter;
}

auto OutputWindow::showEvent(QShowEvent *e) -> void
{
  QPlainTextEdit::showEvent(e);
  if (d->scroll_to_bottom)
    scrollToBottom();
}

auto OutputWindow::wheelEvent(QWheelEvent *e) -> void
{
  if (d->zoom_enabled) {
    if (e->modifiers() & Qt::ControlModifier) {
      const auto delta = static_cast<float>(e->angleDelta().y()) / 120.f;

      // Workaround for ORCABUG-22721, remove when properly fixed in Qt
      if (const auto new_size = static_cast<float>(font().pointSizeF()) + delta; delta < 0.f && new_size < 4.f)
        return;

      zoomInF(delta);
      emit wheelZoom();
      return;
    }
  }
  QPlainTextEdit::wheelEvent(e);
  updateAutoScroll();
  updateMicroFocus();
}

auto OutputWindow::setBaseFont(const QFont &new_font) -> void
{
  const auto zoom = fontZoom();
  d->original_font_size = static_cast<float>(new_font.pointSizeF());
  auto tmp = new_font;
  const auto new_zoom = qMax(d->original_font_size + zoom, 4.0f);
  tmp.setPointSizeF(new_zoom);
  setFont(tmp);
}

auto OutputWindow::fontZoom() const -> float
{
  return static_cast<float>(font().pointSizeF()) - d->original_font_size;
}

auto OutputWindow::setFontZoom(const float zoom) -> void
{
  auto f = font();

  if (static_cast<float>(f.pointSizeF()) == d->original_font_size + zoom)
    return;

  const auto new_zoom = qMax(d->original_font_size + zoom, 4.0f);
  f.setPointSizeF(new_zoom);
  setFont(f);
}

auto OutputWindow::setWheelZoomEnabled(const bool enabled) const -> void
{
  d->zoom_enabled = enabled;
}

auto OutputWindow::updateFilterProperties(const QString &filter_text, const Qt::CaseSensitivity case_sensitivity, const bool regexp, const bool is_inverted) -> void
{
  FilterModeFlags flags;
  flags.setFlag(FilterModeFlag::CaseSensitive, case_sensitivity == Qt::CaseSensitive).setFlag(FilterModeFlag::RegExp, regexp).setFlag(FilterModeFlag::Inverted, is_inverted);

  if (d->filter_mode == flags && d->filter_text == filter_text)
    return;

  d->last_filtered_block_number = -1;

  if (d->filter_text != filter_text) {
    const auto filter_text_was_empty = d->filter_text.isEmpty();
    d->filter_text = filter_text;

    // Update textedit's background color
    if (filter_text.isEmpty() && !filter_text_was_empty) {
      setPalette(d->original_palette);
      setReadOnly(d->original_read_only);
    }

    if (!filter_text.isEmpty() && filter_text_was_empty) {
      d->original_read_only = isReadOnly();
      setReadOnly(true);
      const auto new_bg_color = [this] {
        const auto& current_color = palette().color(QPalette::Base);
        constexpr auto factor = 120;
        return current_color.value() < 128 ? current_color.lighter(factor) : current_color.darker(factor);
      };
      auto p = palette();
      p.setColor(QPalette::Base, new_bg_color());
      setPalette(p);
    }
  }

  d->filter_mode = flags;
  filterNewContent();
}

auto OutputWindow::filterNewContent() -> void
{
  auto last_block = document()->findBlockByNumber(d->last_filtered_block_number);

  if (!last_block.isValid())
    last_block = document()->begin();

  const auto invert = d->filter_mode.testFlag(FilterModeFlag::Inverted);

  if (d->filter_mode.testFlag(FilterModeFlag::RegExp)) {
    QRegularExpression reg_exp(d->filter_text);
    if (!d->filter_mode.testFlag(FilterModeFlag::CaseSensitive))
      reg_exp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    for (; last_block != document()->end(); last_block = last_block.next())
      last_block.setVisible(d->filter_text.isEmpty() || reg_exp.match(last_block.text()).hasMatch() != invert);
  } else {
    if (d->filter_mode.testFlag(FilterModeFlag::CaseSensitive)) {
      for (; last_block != document()->end(); last_block = last_block.next())
        last_block.setVisible(d->filter_text.isEmpty() || last_block.text().contains(d->filter_text) != invert);
    } else {
      for (; last_block != document()->end(); last_block = last_block.next()) {
        last_block.setVisible(d->filter_text.isEmpty() || last_block.text().toLower().contains(d->filter_text.toLower()) != invert);
      }
    }
  }

  d->last_filtered_block_number = document()->lastBlock().blockNumber();

  // FIXME: Why on earth is this necessary? We should probably do something else instead...
  setDocument(document());

  if (d->scroll_to_bottom)
    scrollToBottom();
}

auto OutputWindow::handleNextOutputChunk() -> void
{
  QTC_ASSERT(!d->queued_output.isEmpty(), return);

  if (auto &[fst, snd] = d->queued_output.first(); fst.size() <= g_chunk_size) {
    handleOutputChunk(fst, snd);
    d->queued_output.removeFirst();
  } else {
    handleOutputChunk(fst.left(g_chunk_size), snd);
    fst.remove(0, g_chunk_size);
  }

  if (!d->queued_output.isEmpty())
    d->queue_timer.start();
  else if (d->flush_requested) {
    d->formatter.flush();
    d->flush_requested = false;
  }
}

auto OutputWindow::handleOutputChunk(const QString &output, const OutputFormat format) -> void
{
  auto out = output;

  if (out.size() > d->max_char_count) {
    // Current chunk alone exceeds limit, we need to cut it.
    const auto elided = static_cast<int>(out.size() - d->max_char_count);
    out = out.left(d->max_char_count / 2) + "[[[... " + tr("Elided %n characters due to Application Output settings", nullptr, elided) + " ...]]]" + out.right(d->max_char_count / 2);
    setMaximumBlockCount(static_cast<int>(out.count('\n')) + 1);
  } else {
    if (auto planned_chars =document()->characterCount() + static_cast<int>(out.size()); planned_chars > d->max_char_count) {
      auto planned_block_count = document()->blockCount();
      auto tb = document()->firstBlock();
      while (tb.isValid() && planned_chars > d->max_char_count && planned_block_count > 1) {
        planned_chars -= tb.length();
        planned_block_count -= 1;
        tb = tb.next();
      }
      setMaximumBlockCount(planned_block_count);
    } else {
      setMaximumBlockCount(-1);
    }
  }

  d->formatter.appendMessage(out, format);

  if (d->scroll_to_bottom) {
    if (d->last_message.elapsed() < 5) {
      d->scroll_timer.start();
    } else {
      d->scroll_timer.stop();
      scrollToBottom();
    }
  }

  d->last_message.start();
  enableUndoRedo();
}

auto OutputWindow::updateAutoScroll() const -> void
{
  d->scroll_to_bottom = verticalScrollBar()->sliderPosition() >= verticalScrollBar()->maximum() - 1;
}

auto OutputWindow::setMaxCharCount(const int count) -> void
{
  d->max_char_count = count;
  setMaximumBlockCount(count / 100);
}

auto OutputWindow::maxCharCount() const -> int
{
  return d->max_char_count;
}

auto OutputWindow::appendMessage(const QString &out, OutputFormat format) const -> void
{
  if (d->queued_output.isEmpty() || d->queued_output.last().second != format)
    d->queued_output << qMakePair(out, format);
  else
    d->queued_output.last().first.append(out);

  if (!d->queue_timer.isActive())
    d->queue_timer.start();
}

auto OutputWindow::registerPositionOf(const unsigned task_id, const int linked_output_lines, const int skip_lines, const int offset) const -> void
{
  if (linked_output_lines <= 0)
    return;

  const auto blocknumber = document()->blockCount() - offset;
  const auto first_line = blocknumber - linked_output_lines - skip_lines;
  const auto last_line = first_line + linked_output_lines - 1;

  d->task_positions.insert(task_id, qMakePair(first_line, last_line));
}

auto OutputWindow::knowsPositionOf(const unsigned task_id) const -> bool
{
  return d->task_positions.contains(task_id);
}

auto OutputWindow::showPositionOf(const unsigned task_id) -> void
{
  const auto [fst, snd] = d->task_positions.value(task_id);
  QTextCursor new_cursor(document()->findBlockByNumber(snd));

  // Move cursor to end of last line of interest:
  new_cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
  setTextCursor(new_cursor);

  // Move cursor and select lines:
  new_cursor.setPosition(document()->findBlockByNumber(fst).position(), QTextCursor::KeepAnchor);
  setTextCursor(new_cursor);

  // Center cursor now:
  centerCursor();
}

auto OutputWindow::createMimeDataFromSelection() const -> QMimeData*
{
  const auto mime_data = new QMimeData;
  QString content;
  const auto sel_start = textCursor().selectionStart();
  const auto sel_end = textCursor().selectionEnd();
  const auto first_block = document()->findBlock(sel_start);
  const auto last_block = document()->findBlock(sel_end);

  for (auto cur_block = first_block; cur_block != last_block; cur_block = cur_block.next()) {
    if (!cur_block.isVisible())
      continue;
    if (cur_block == first_block)
      content += cur_block.text().mid(sel_start - first_block.position());
    else
      content += cur_block.text();
    content += '\n';
  }

  if (last_block.isValid() && last_block.isVisible()) {
    if (first_block == last_block)
      content = textCursor().selectedText();
    else
      content += last_block.text().mid(0, sel_end - last_block.position());
  }

  mime_data->setText(content);
  return mime_data;
}

auto OutputWindow::clear() const -> void
{
  d->formatter.clear();
  d->scroll_to_bottom = true;
  d->task_positions.clear();
}

auto OutputWindow::flush() -> void
{
  if (const auto total_queued_size = std::accumulate(d->queued_output.cbegin(), d->queued_output.cend(), 0, [](const int val, const QPair<QString, OutputFormat> &c) { return val + c.first.size(); }); total_queued_size > 5 * g_chunk_size) {
    d->flush_requested = true;
    return;
  }

  d->queue_timer.stop();

  for (const auto &[fst, snd] : qAsConst(d->queued_output))
    handleOutputChunk(fst, snd);

  d->queued_output.clear();
  d->formatter.flush();
}

auto OutputWindow::reset() -> void
{
  flush();

  d->queue_timer.stop();
  d->formatter.reset();
  d->scroll_to_bottom = true;

  if (!d->queued_output.isEmpty()) {
    d->queued_output.clear();
    d->formatter.appendMessage(tr("[Discarding excessive amount of pending output.]\n"), ErrorMessageFormat);
  }

  d->flush_requested = false;
}

auto OutputWindow::scrollToBottom() const -> void
{
  verticalScrollBar()->setValue(verticalScrollBar()->maximum());
  // QPlainTextEdit destroys the first calls value in case of multiline
  // text, so make sure that the scroll bar actually gets the value set.
  // Is a noop if the first call succeeded.
  verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

auto OutputWindow::grayOutOldContent() const -> void
{
  if (!d->cursor.atEnd())
    d->cursor.movePosition(QTextCursor::End);

  const auto end_format = d->cursor.charFormat();

  d->cursor.select(QTextCursor::Document);

  QTextCharFormat format;
  const auto &bkg_color = palette().base().color();
  const auto &fgd_color = palette().text().color();
  constexpr auto bkg_factor = 0.50;
  constexpr auto fgd_factor = 1. - bkg_factor;
  format.setForeground(QColor(static_cast<int>(bkg_factor) * bkg_color.red() + static_cast<int>(fgd_factor) * fgd_color.red(), static_cast<int>(bkg_factor) * bkg_color.green() + static_cast<int>(fgd_factor) * fgd_color.green(), static_cast<int>(bkg_factor) * bkg_color.blue() + static_cast<int>(fgd_factor) * fgd_color.blue()));

  d->cursor.mergeCharFormat(format);
  d->cursor.movePosition(QTextCursor::End);
  d->cursor.setCharFormat(end_format);
  d->cursor.insertBlock(QTextBlockFormat());
}

auto OutputWindow::enableUndoRedo() -> void
{
  setMaximumBlockCount(0);
  setUndoRedoEnabled(true);
}

auto OutputWindow::setWordWrapEnabled(const bool wrap) -> void
{
  if (wrap)
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  else
    setWordWrapMode(QTextOption::NoWrap);
}

#ifdef ORCA_BUILD_WITH_PLUGINS_TESTS

// Handles all lines starting with "A" and the following ones up to and including the next
// one starting with "A".
class TestFormatterA : public OutputLineParser
{
private:
    Result handleLine(const QString &text, OutputFormat) override
    {
        static const QString replacement = "handled by A";
        if (m_handling) {
            if (text.startsWith("A")) {
                m_handling = false;
                return {Status::Done, {}, replacement};
            }
            return {Status::InProgress, {}, replacement};
        }
        if (text.startsWith("A")) {
            m_handling = true;
            return {Status::InProgress, {}, replacement};
        }
        return Status::NotHandled;
    }

    bool m_handling = false;
};

// Handles all lines starting with "B". No continuation logic.
class TestFormatterB : public OutputLineParser
{
private:
    Result handleLine(const QString &text, OutputFormat) override
    {
        if (text.startsWith("B"))
            return {Status::Done, {}, QString("handled by B")};
        return Status::NotHandled;
    }
};

void Internal::CorePlugin::testOutputFormatter()
{
    const QString input =
            "B to be handled by B\r\r\n"
            "not to be handled\n\n\n\n"
            "A to be handled by A\n"
            "continuation for A\r\n"
            "B looks like B, but still continuation for A\r\n"
            "A end of A\n"
            "A next A\n"
            "A end of next A\n"
            " A trick\r\n"
            "line with \r embedded carriage return\n"
            "B to be handled by B\n";
    const QString output =
            "handled by B\n"
            "not to be handled\n\n\n\n"
            "handled by A\n"
            "handled by A\n"
            "handled by A\n"
            "handled by A\n"
            "handled by A\n"
            "handled by A\n"
            " A trick\n"
            " embedded carriage return\n"
            "handled by B\n";

    // Stress-test the implementation by providing the input in chunks, splitting at all possible
    // offsets.
    for (int i = 0; i < input.length(); ++i) {
        OutputFormatter formatter;
        QPlainTextEdit textEdit;
        formatter.setPlainTextEdit(&textEdit);
        formatter.setLineParsers({new TestFormatterB, new TestFormatterA});
        formatter.appendMessage(input.left(i), StdOutFormat);
        formatter.appendMessage(input.mid(i), StdOutFormat);
        formatter.flush();
        QCOMPARE(textEdit.toPlainText(), output);
    }
}
#endif // ORCA_BUILD_WITH_PLUGINS_TESTS
} // namespace Core
