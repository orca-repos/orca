// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-codec-selector.hpp"
#include "core-text-document.hpp"

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/itemviews.hpp>

#include <QPushButton>
#include <QScrollBar>
#include <QTextCodec>
#include <QVBoxLayout>

namespace Orca::Plugin::Core {

/* custom class to make sure the width is wide enough for the
 * contents. Should be easier with Qt. */
class CodecListWidget final : public Utils::ListWidget {
public:
  explicit CodecListWidget(QWidget *parent) : ListWidget(parent) { }

  auto sizeHint() const -> QSize override
  {
    return QListWidget::sizeHint().expandedTo(QSize(sizeHintForColumn(0) + verticalScrollBar()->sizeHint().width() + 4, 0));
  }
};

CodecSelector::CodecSelector(QWidget *parent, const BaseTextDocument *doc) : QDialog(parent)
{
  m_has_decoding_error = doc->hasDecodingError();
  m_is_modified = doc->isModified();

  QByteArray buf;
  if (m_has_decoding_error)
    buf = doc->decodingErrorSample();

  setWindowTitle(tr("Text Encoding"));
  m_label = new QLabel(this);

  QString decoding_error_hint;
  if (m_has_decoding_error)
    decoding_error_hint = QLatin1Char('\n') + tr("The following encodings are likely to fit:");
  m_label->setText(tr("Select encoding for \"%1\".%2").arg(doc->filePath().fileName()).arg(decoding_error_hint));

  m_list_widget = new CodecListWidget(this);
  m_list_widget->setActivationMode(Utils::DoubleClickActivation);

  QStringList encodings;
  auto mibs = QTextCodec::availableMibs();
  Utils::sort(mibs);
  QList<int> sorted_mibs;
  for (const auto mib : qAsConst(mibs))
    if (mib >= 0)
      sorted_mibs += mib;
  for (const auto mib : qAsConst(mibs))
    if (mib < 0)
      sorted_mibs += mib;

  auto current_index = -1;
  for (const auto mib : qAsConst(sorted_mibs)) {
    const auto c = QTextCodec::codecForMib(mib);
    if (!doc->supportsCodec(c))
      continue;
    if (!buf.isEmpty()) {
      // slow, should use a feature from QTextCodec or QTextDecoder (but those are broken currently)
      auto verify_buf = c->fromUnicode(c->toUnicode(buf));
      // the minSize trick lets us ignore unicode headers
      if (const auto min_size = qMin(verify_buf.size(), buf.size()); min_size < buf.size() - 4 || memcmp(verify_buf.constData() + verify_buf.size() - min_size, buf.constData() + buf.size() - min_size, min_size))
        continue;
    }
    auto names = QString::fromLatin1(c->name());
    for (const auto aliases = c->aliases(); const auto &alias : aliases)
      names += QLatin1String(" / ") + QString::fromLatin1(alias);
    if (doc->codec() == c)
      current_index = static_cast<int>(encodings.count());
    encodings << names;
  }
  m_list_widget->addItems(encodings);
  if (current_index >= 0)
    m_list_widget->setCurrentRow(current_index);

  connect(m_list_widget, &QListWidget::itemSelectionChanged, this, &CodecSelector::updateButtons);

  m_dialog_button_box = new QDialogButtonBox(this);
  m_reload_button = m_dialog_button_box->addButton(tr("Reload with Encoding"), QDialogButtonBox::DestructiveRole);
  m_save_button = m_dialog_button_box->addButton(tr("Save with Encoding"), QDialogButtonBox::DestructiveRole);
  m_dialog_button_box->addButton(QDialogButtonBox::Cancel);

  connect(m_dialog_button_box, &QDialogButtonBox::clicked, this, &CodecSelector::buttonClicked);
  connect(m_list_widget, &QAbstractItemView::activated, m_reload_button, &QAbstractButton::click);

  const auto vbox = new QVBoxLayout(this);
  vbox->addWidget(m_label);
  vbox->addWidget(m_list_widget);
  vbox->addWidget(m_dialog_button_box);

  updateButtons();
}

CodecSelector::~CodecSelector() = default;

auto CodecSelector::updateButtons() const -> void
{
  const auto has_codec = selectedCodec() != nullptr;
  m_reload_button->setEnabled(!m_is_modified && has_codec);
  m_save_button->setEnabled(!m_has_decoding_error && has_codec);
}

auto CodecSelector::selectedCodec() const -> QTextCodec*
{
  if (const auto item = m_list_widget->currentItem()) {
    if (!item->isSelected())
      return nullptr;
    auto codec_name = item->text();
    if (codec_name.contains(QLatin1String(" / ")))
      codec_name = codec_name.left(codec_name.indexOf(QLatin1String(" / ")));
    return QTextCodec::codecForName(codec_name.toLatin1());
  }
  return nullptr;
}

auto CodecSelector::buttonClicked(const QAbstractButton *button) -> void
{
  auto result = Result::Cancel;
  if (button == m_reload_button)
    result = Result::Reload;
  if (button == m_save_button)
    result = Result::Save;
  done(static_cast<int>(result));
}

} // namespace Orca::Plugin::Core
