// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "checkablemessagebox.hpp"

#include "qtcassert.hpp"
#include "qtcsettings.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTextEdit>

/*!
    \class Utils::CheckableMessageBox

    \brief The CheckableMessageBox class implements a message box suitable for
    questions with a
     "Do not ask me again" checkbox.

    Emulates the QMessageBox API with
    static conveniences. The message label can open external URLs.
*/

static const char kDoNotAskAgainKey[] = "DoNotAskAgain";

namespace Utils {

class CheckableMessageBoxPrivate {
public:
  CheckableMessageBoxPrivate(QDialog *q)
  {
    QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    pixmapLabel = new QLabel(q);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(pixmapLabel->sizePolicy().hasHeightForWidth());
    pixmapLabel->setSizePolicy(sizePolicy);
    pixmapLabel->setVisible(false);
    pixmapLabel->setFocusPolicy(Qt::NoFocus);

    auto pixmapSpacer = new QSpacerItem(0, 5, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);

    messageLabel = new QLabel(q);
    messageLabel->setMinimumSize(QSize(300, 0));
    messageLabel->setWordWrap(true);
    messageLabel->setOpenExternalLinks(true);
    messageLabel->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard | Qt::LinksAccessibleByMouse);
    messageLabel->setFocusPolicy(Qt::NoFocus);
    messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    checkBox = new QCheckBox(q);
    checkBox->setText(CheckableMessageBox::tr("Do not ask again"));

    const QString showText = CheckableMessageBox::tr("Show Details...");
    detailsButton = new QPushButton(showText, q);
    detailsButton->setAutoDefault(false);
    detailsButton->hide();
    detailsText = new QTextEdit(q);
    detailsText->hide();
    QObject::connect(detailsButton, &QPushButton::clicked, detailsText, [this, showText] {
      detailsText->setVisible(!detailsText->isVisible());
      detailsButton->setText(detailsText->isVisible() ? CheckableMessageBox::tr("Hide Details...") : showText);
    });

    buttonBox = new QDialogButtonBox(q);
    buttonBox->setOrientation(Qt::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

    auto verticalLayout = new QVBoxLayout();
    verticalLayout->addWidget(pixmapLabel);
    verticalLayout->addItem(pixmapSpacer);

    auto horizontalLayout_2 = new QHBoxLayout();
    horizontalLayout_2->addLayout(verticalLayout);
    horizontalLayout_2->addWidget(messageLabel, 10);

    auto horizontalLayout = new QHBoxLayout();
    horizontalLayout->addWidget(checkBox);
    horizontalLayout->addStretch(10);

    auto detailsButtonLayout = new QHBoxLayout;
    detailsButtonLayout->addWidget(detailsButton);
    detailsButtonLayout->addStretch(10);

    auto verticalLayout_2 = new QVBoxLayout(q);
    verticalLayout_2->addLayout(horizontalLayout_2);
    verticalLayout_2->addLayout(horizontalLayout);
    verticalLayout_2->addLayout(detailsButtonLayout);
    verticalLayout_2->addWidget(detailsText, 10);
    verticalLayout_2->addStretch(1);
    verticalLayout_2->addWidget(buttonBox);
  }

  QLabel *pixmapLabel = nullptr;
  QLabel *messageLabel = nullptr;
  QCheckBox *checkBox = nullptr;
  QDialogButtonBox *buttonBox = nullptr;
  QAbstractButton *clickedButton = nullptr;
  QPushButton *detailsButton = nullptr;
  QTextEdit *detailsText = nullptr;
  QMessageBox::Icon icon = QMessageBox::NoIcon;
};

CheckableMessageBox::CheckableMessageBox(QWidget *parent) : QDialog(parent), d(new CheckableMessageBoxPrivate(this))
{
  setModal(true);
  connect(d->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(d->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(d->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *b) { d->clickedButton = b; });
}

CheckableMessageBox::~CheckableMessageBox()
{
  delete d;
}

auto CheckableMessageBox::clickedButton() const -> QAbstractButton*
{
  return d->clickedButton;
}

auto CheckableMessageBox::clickedStandardButton() const -> QDialogButtonBox::StandardButton
{
  if (d->clickedButton)
    return d->buttonBox->standardButton(d->clickedButton);
  return QDialogButtonBox::NoButton;
}

auto CheckableMessageBox::text() const -> QString
{
  return d->messageLabel->text();
}

auto CheckableMessageBox::setText(const QString &t) -> void
{
  d->messageLabel->setText(t);
}

auto CheckableMessageBox::icon() const -> QMessageBox::Icon
{
  return d->icon;
}

// See QMessageBoxPrivate::standardIcon
static auto pixmapForIcon(QMessageBox::Icon icon, QWidget *w) -> QPixmap
{
  const QStyle *style = w ? w->style() : QApplication::style();
  const int iconSize = style->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, w);
  QIcon tmpIcon;
  switch (icon) {
  case QMessageBox::Information:
    tmpIcon = style->standardIcon(QStyle::SP_MessageBoxInformation, nullptr, w);
    break;
  case QMessageBox::Warning:
    tmpIcon = style->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, w);
    break;
  case QMessageBox::Critical:
    tmpIcon = style->standardIcon(QStyle::SP_MessageBoxCritical, nullptr, w);
    break;
  case QMessageBox::Question:
    tmpIcon = style->standardIcon(QStyle::SP_MessageBoxQuestion, nullptr, w);
    break;
  default:
    break;
  }
  if (!tmpIcon.isNull()) {
    QWindow *window = nullptr;
    if (w) {
      window = w->windowHandle();
      if (!window) {
        if (const QWidget *nativeParent = w->nativeParentWidget())
          window = nativeParent->windowHandle();
      }
    }
    return tmpIcon.pixmap(window, QSize(iconSize, iconSize));
  }
  return QPixmap();
}

auto CheckableMessageBox::setIcon(QMessageBox::Icon icon) -> void
{
  d->icon = icon;
  const QPixmap pixmap = pixmapForIcon(icon, this);
  d->pixmapLabel->setPixmap(pixmap);
  d->pixmapLabel->setVisible(!pixmap.isNull());
}

auto CheckableMessageBox::isChecked() const -> bool
{
  return d->checkBox->isChecked();
}

auto CheckableMessageBox::setChecked(bool s) -> void
{
  d->checkBox->setChecked(s);
}

auto CheckableMessageBox::checkBoxText() const -> QString
{
  return d->checkBox->text();
}

auto CheckableMessageBox::setCheckBoxText(const QString &t) -> void
{
  d->checkBox->setText(t);
}

auto CheckableMessageBox::isCheckBoxVisible() const -> bool
{
  return d->checkBox->isVisible();
}

auto CheckableMessageBox::setCheckBoxVisible(bool v) -> void
{
  d->checkBox->setVisible(v);
}

auto CheckableMessageBox::detailedText() const -> QString
{
  return d->detailsText->toPlainText();
}

auto CheckableMessageBox::setDetailedText(const QString &text) -> void
{
  d->detailsText->setText(text);
  if (!text.isEmpty())
    d->detailsButton->setVisible(true);
}

auto CheckableMessageBox::standardButtons() const -> QDialogButtonBox::StandardButtons
{
  return d->buttonBox->standardButtons();
}

auto CheckableMessageBox::setStandardButtons(QDialogButtonBox::StandardButtons s) -> void
{
  d->buttonBox->setStandardButtons(s);
}

auto CheckableMessageBox::button(QDialogButtonBox::StandardButton b) const -> QPushButton*
{
  return d->buttonBox->button(b);
}

auto CheckableMessageBox::addButton(const QString &text, QDialogButtonBox::ButtonRole role) -> QPushButton*
{
  return d->buttonBox->addButton(text, role);
}

auto CheckableMessageBox::defaultButton() const -> QDialogButtonBox::StandardButton
{
  const QList<QAbstractButton*> buttons = d->buttonBox->buttons();
  for (QAbstractButton *b : buttons)
    if (auto *pb = qobject_cast<QPushButton*>(b))
      if (pb->isDefault())
        return d->buttonBox->standardButton(pb);
  return QDialogButtonBox::NoButton;
}

auto CheckableMessageBox::setDefaultButton(QDialogButtonBox::StandardButton s) -> void
{
  if (QPushButton *b = d->buttonBox->button(s)) {
    b->setDefault(true);
    b->setFocus();
  }
}

auto CheckableMessageBox::question(QWidget *parent, const QString &title, const QString &question, const QString &checkBoxText, bool *checkBoxSetting, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton) -> QDialogButtonBox::StandardButton
{
  CheckableMessageBox mb(parent);
  mb.setWindowTitle(title);
  mb.setIcon(QMessageBox::Question);
  mb.setText(question);
  mb.setCheckBoxText(checkBoxText);
  mb.setChecked(*checkBoxSetting);
  mb.setStandardButtons(buttons);
  mb.setDefaultButton(defaultButton);
  mb.exec();
  *checkBoxSetting = mb.isChecked();
  return mb.clickedStandardButton();
}

auto CheckableMessageBox::information(QWidget *parent, const QString &title, const QString &text, const QString &checkBoxText, bool *checkBoxSetting, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton) -> QDialogButtonBox::StandardButton
{
  CheckableMessageBox mb(parent);
  mb.setWindowTitle(title);
  mb.setIcon(QMessageBox::Information);
  mb.setText(text);
  mb.setCheckBoxText(checkBoxText);
  mb.setChecked(*checkBoxSetting);
  mb.setStandardButtons(buttons);
  mb.setDefaultButton(defaultButton);
  mb.exec();
  *checkBoxSetting = mb.isChecked();
  return mb.clickedStandardButton();
}

auto CheckableMessageBox::dialogButtonBoxToMessageBoxButton(QDialogButtonBox::StandardButton db) -> QMessageBox::StandardButton
{
  return static_cast<QMessageBox::StandardButton>(int(db));
}

auto CheckableMessageBox::shouldAskAgain(QSettings *settings, const QString &settingsSubKey) -> bool
{
  if (QTC_GUARD(settings)) {
    settings->beginGroup(QLatin1String(kDoNotAskAgainKey));
    bool shouldNotAsk = settings->value(settingsSubKey, false).toBool();
    settings->endGroup();
    if (shouldNotAsk)
      return false;
  }
  return true;
}

enum DoNotAskAgainType {
  Question,
  Information
};

auto initDoNotAskAgainMessageBox(CheckableMessageBox &messageBox, const QString &title, const QString &text, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton, DoNotAskAgainType type) -> void
{
  messageBox.setWindowTitle(title);
  messageBox.setIcon(type == Information ? QMessageBox::Information : QMessageBox::Question);
  messageBox.setText(text);
  messageBox.setCheckBoxVisible(true);
  messageBox.setCheckBoxText(type == Information ? CheckableMessageBox::msgDoNotShowAgain() : CheckableMessageBox::msgDoNotAskAgain());
  messageBox.setChecked(false);
  messageBox.setStandardButtons(buttons);
  messageBox.setDefaultButton(defaultButton);
}

auto CheckableMessageBox::doNotAskAgain(QSettings *settings, const QString &settingsSubKey) -> void
{
  if (!settings)
    return;

  settings->beginGroup(QLatin1String(kDoNotAskAgainKey));
  settings->setValue(settingsSubKey, true);
  settings->endGroup();
}

/*!
    Shows a message box with given \a title and \a text, and a \gui {Do not ask again} check box.
    If the user checks the check box and accepts the dialog with the \a acceptButton,
    further invocations of this function with the same \a settings and \a settingsSubKey will not
    show the dialog, but instantly return \a acceptButton.

    Returns the clicked button, or QDialogButtonBox::NoButton if the user rejects the dialog
    with the escape key, or \a acceptButton if the dialog is suppressed.
*/
auto CheckableMessageBox::doNotAskAgainQuestion(QWidget *parent, const QString &title, const QString &text, QSettings *settings, const QString &settingsSubKey, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton, QDialogButtonBox::StandardButton acceptButton) -> QDialogButtonBox::StandardButton
{
  if (!shouldAskAgain(settings, settingsSubKey))
    return acceptButton;

  CheckableMessageBox messageBox(parent);
  initDoNotAskAgainMessageBox(messageBox, title, text, buttons, defaultButton, Question);
  messageBox.exec();
  if (messageBox.isChecked() && (messageBox.clickedStandardButton() == acceptButton))
    doNotAskAgain(settings, settingsSubKey);

  return messageBox.clickedStandardButton();
}

/*!
    Shows a message box with given \a title and \a text, and a \gui {Do not show again} check box.
    If the user checks the check box and quits the dialog, further invocations of this
    function with the same \a settings and \a settingsSubKey will not show the dialog, but instantly return.

    Returns the clicked button, or QDialogButtonBox::NoButton if the user rejects the dialog
    with the escape key, or \a defaultButton if the dialog is suppressed.
*/
auto CheckableMessageBox::doNotShowAgainInformation(QWidget *parent, const QString &title, const QString &text, QSettings *settings, const QString &settingsSubKey, QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton) -> QDialogButtonBox::StandardButton
{
  if (!shouldAskAgain(settings, settingsSubKey))
    return defaultButton;

  CheckableMessageBox messageBox(parent);
  initDoNotAskAgainMessageBox(messageBox, title, text, buttons, defaultButton, Information);
  messageBox.exec();
  if (messageBox.isChecked())
    doNotAskAgain(settings, settingsSubKey);

  return messageBox.clickedStandardButton();
}

/*!
    Resets all suppression settings for doNotAskAgainQuestion() found in \a settings,
    so all these message boxes are shown again.
 */
auto CheckableMessageBox::resetAllDoNotAskAgainQuestions(QSettings *settings) -> void
{
  QTC_ASSERT(settings, return);
  settings->beginGroup(QLatin1String(kDoNotAskAgainKey));
  settings->remove(QString());
  settings->endGroup();
}

/*!
    Returns whether any message boxes from doNotAskAgainQuestion() are suppressed
    in the \a settings.
*/
auto CheckableMessageBox::hasSuppressedQuestions(QSettings *settings) -> bool
{
  QTC_ASSERT(settings, return false);
  bool hasSuppressed = false;
  settings->beginGroup(QLatin1String(kDoNotAskAgainKey));
  const QStringList childKeys = settings->childKeys();
  for (const QString &subKey : childKeys) {
    if (settings->value(subKey, false).toBool()) {
      hasSuppressed = true;
      break;
    }
  }
  settings->endGroup();
  return hasSuppressed;
}

/*!
    Returns the standard \gui {Do not ask again} check box text.
    \sa doNotAskAgainQuestion()
*/
auto CheckableMessageBox::msgDoNotAskAgain() -> QString
{
  return QApplication::translate("Utils::CheckableMessageBox", "Do not &ask again");
}

/*!
    Returns the standard \gui {Do not show again} check box text.
    \sa doNotShowAgainInformation()
*/
auto CheckableMessageBox::msgDoNotShowAgain() -> QString
{
  return QApplication::translate("Utils::CheckableMessageBox", "Do not &show again");
}

} // namespace Utils
