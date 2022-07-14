// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-version-dialog.hpp"

#include "core-icons.hpp"
#include "core-interface.hpp"

#include <app/app_version.hpp>

#include <utils/algorithm.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>

namespace Orca::Plugin::Core {

VersionDialog::VersionDialog(QWidget *parent) : QDialog(parent)
{
  // We need to set the window icon explicitly here since for some reason the
  // application icon isn't used when the size of the dialog is fixed (at least not on X11/GNOME)
  if constexpr (Utils::HostOsInfo::isLinuxHost())
    setWindowIcon(ORCALOGO_BIG.icon());

  setWindowTitle(tr("About %1").arg(Core::IDE_DISPLAY_NAME));

  const auto layout = new QGridLayout(this);
  layout->setSizeConstraint(QLayout::SetFixedSize);

  QString ide_rev;
  #ifdef IDE_REVISION
  const QString revUrl = QString::fromLatin1(IDE_REVISION_URL);
  const QString rev = QString::fromLatin1(IDE_REVISION_STR);
  ideRev = tr("<br/>From revision %1<br/>").arg(revUrl.isEmpty() ? rev : QString::fromLatin1("<a href=\"%1\">%2</a>").arg(revUrl, rev));
  #endif

  QString build_date_info;

  const QString br = QLatin1String("<br/>");
  const auto additional_info_lines = ICore::additionalAboutInformation();
  const auto additional_info = QStringList(Utils::transform(additional_info_lines, &QString::toHtmlEscaped)).join(br);

  const QString description = tr("<h3>%1</h3>" "%2<br/>" "%3" "%4" "%5" "<br/>" "Copyright 2008-%6 %7. All rights reserved.<br/>" "<br/>" "The program is provided AS IS with NO WARRANTY OF ANY KIND, " "INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A " "PARTICULAR PURPOSE.<br/>").arg(ICore::versionString(), ICore::buildCompatibilityString(), build_date_info, ide_rev, additional_info.isEmpty() ? QString() : br + additional_info + br, QLatin1String(IDE_YEAR), QLatin1String(IDE_AUTHOR)) + "<br/>" + tr("The Qt logo as well as Qt®, Qt Quick®, Built with Qt®, Boot to Qt®, " "Qt Quick Compiler®, Qt Enterprise®, Qt Mobile® and Qt Embedded® are " "registered trademarks of The Qt Company Ltd.");

  const auto copy_right_label = new QLabel(description);
  copy_right_label->setWordWrap(true);
  copy_right_label->setOpenExternalLinks(true);
  copy_right_label->setTextInteractionFlags(Qt::TextBrowserInteraction);

  const auto button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  const auto close_button = button_box->button(QDialogButtonBox::Close);
  QTC_CHECK(close_button);
  button_box->addButton(close_button, static_cast<QDialogButtonBox::ButtonRole>(QDialogButtonBox::RejectRole | QDialogButtonBox::AcceptRole));
  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  const auto logo_label = new QLabel;
  logo_label->setPixmap(ORCALOGO_BIG.pixmap());
  layout->addWidget(logo_label, 0, 0, 1, 1);
  layout->addWidget(copy_right_label, 0, 1, 4, 4);
  layout->addWidget(button_box, 4, 0, 1, 5);
}

auto VersionDialog::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::ShortcutOverride) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key() == Qt::Key_Escape && !ke->modifiers()) {
      ke->accept();
      return true;
    }
  }
  return QDialog::event(event);
}

} // namespace Orca::Plugin::Core
