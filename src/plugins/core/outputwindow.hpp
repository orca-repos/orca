// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"
#include "icontext.hpp"

#include <utils/outputformat.hpp>

#include <QPlainTextEdit>

namespace Utils {
class OutputFormatter;
class OutputLineParser;
}

namespace Core {

namespace Internal {
class OutputWindowPrivate;
}

class CORE_EXPORT OutputWindow final : public QPlainTextEdit {
  Q_OBJECT

public:
  enum class FilterModeFlag {
    Default = 0x00,
    // Plain text, non case sensitive, for initialization
    RegExp = 0x01,
    CaseSensitive = 0x02,
    Inverted = 0x04,
  };

  Q_DECLARE_FLAGS(FilterModeFlags, FilterModeFlag)

  OutputWindow(const Context& context, const QString &settings_key, QWidget *parent = nullptr);
  ~OutputWindow() override;

  auto setLineParsers(const QList<Utils::OutputLineParser*> &parsers) -> void;
  auto outputFormatter() const -> Utils::OutputFormatter*;
  auto appendMessage(const QString &out, Utils::OutputFormat format) const -> void;
  auto registerPositionOf(unsigned task_id, int linked_output_lines, int skip_lines, int offset = 0) const -> void;
  auto knowsPositionOf(unsigned task_id) const -> bool;
  auto showPositionOf(unsigned task_id) -> void;
  auto grayOutOldContent() const -> void;
  auto clear() const -> void;
  auto flush() -> void;
  auto reset() -> void;
  auto scrollToBottom() const -> void;
  auto setMaxCharCount(int count) -> void;
  auto maxCharCount() const -> int;
  auto setBaseFont(const QFont &new_font) -> void;
  auto fontZoom() const -> float;
  auto setFontZoom(float zoom) -> void;
  auto resetZoom() -> void { setFontZoom(0); }
  auto setWheelZoomEnabled(bool enabled) const -> void;
  auto updateFilterProperties(const QString &filter_text, Qt::CaseSensitivity case_sensitivity, bool regexp, bool is_inverted) -> void;

signals:
  auto wheelZoom() -> void;

public slots:
  auto setWordWrapEnabled(bool wrap) -> void;

protected:
  auto handleLink(const QPoint &pos) -> void;

private:
  using QPlainTextEdit::setFont; // call setBaseFont instead, which respects the zoom factor

  auto createMimeDataFromSelection() const -> QMimeData* override;
  auto keyPressEvent(QKeyEvent *ev) -> void override;
  auto mousePressEvent(QMouseEvent *e) -> void override;
  auto mouseReleaseEvent(QMouseEvent *e) -> void override;
  auto mouseMoveEvent(QMouseEvent *e) -> void override;
  auto resizeEvent(QResizeEvent *e) -> void override;
  auto showEvent(QShowEvent *) -> void override;
  auto wheelEvent(QWheelEvent *e) -> void override;
  auto enableUndoRedo() -> void;
  auto filterNewContent() -> void;
  auto handleNextOutputChunk() -> void;
  auto handleOutputChunk(const QString &output, Utils::OutputFormat format) -> void;
  auto updateAutoScroll() const -> void;

  Internal::OutputWindowPrivate *d = nullptr;
};

} // namespace Core
