// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "abiwidget.hpp"
#include "abi.hpp"

#include <utils/guard.hpp>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

/*!
    \class ProjectExplorer::AbiWidget

    \brief The AbiWidget class is a widget to set an ABI.

    \sa ProjectExplorer::Abi
*/

namespace ProjectExplorer {
namespace Internal {

// --------------------------------------------------------------------------
// AbiWidgetPrivate:
// --------------------------------------------------------------------------

class AbiWidgetPrivate {
public:
  auto isCustom() const -> bool
  {
    return m_abi->currentIndex() == 0;
  }

  Utils::Guard m_ignoreChanges;

  Abi m_currentAbi;

  QComboBox *m_abi;

  QComboBox *m_architectureComboBox;
  QComboBox *m_osComboBox;
  QComboBox *m_osFlavorComboBox;
  QComboBox *m_binaryFormatComboBox;
  QComboBox *m_wordWidthComboBox;
};

} // namespace Internal

// --------------------------------------------------------------------------
// AbiWidget
// --------------------------------------------------------------------------

AbiWidget::AbiWidget(QWidget *parent) : QWidget(parent), d(std::make_unique<Internal::AbiWidgetPrivate>())
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  d->m_abi = new QComboBox(this);
  d->m_abi->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->m_abi->setMinimumContentsLength(4);
  layout->addWidget(d->m_abi);
  connect(d->m_abi, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AbiWidget::mainComboBoxChanged);

  d->m_architectureComboBox = new QComboBox(this);
  layout->addWidget(d->m_architectureComboBox);
  for (auto i = 0; i <= static_cast<int>(Abi::UnknownArchitecture); ++i)
    d->m_architectureComboBox->addItem(Abi::toString(static_cast<Abi::Architecture>(i)), i);
  d->m_architectureComboBox->setCurrentIndex(static_cast<int>(Abi::UnknownArchitecture));
  connect(d->m_architectureComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AbiWidget::customComboBoxesChanged);

  const auto separator1 = new QLabel(this);
  separator1->setText(QLatin1String("-"));
  separator1->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout->addWidget(separator1);

  d->m_osComboBox = new QComboBox(this);
  layout->addWidget(d->m_osComboBox);
  for (auto i = 0; i <= static_cast<int>(Abi::UnknownOS); ++i)
    d->m_osComboBox->addItem(Abi::toString(static_cast<Abi::OS>(i)), i);
  d->m_osComboBox->setCurrentIndex(static_cast<int>(Abi::UnknownOS));
  connect(d->m_osComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AbiWidget::customOsComboBoxChanged);

  const auto separator2 = new QLabel(this);
  separator2->setText(QLatin1String("-"));
  separator2->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout->addWidget(separator2);

  d->m_osFlavorComboBox = new QComboBox(this);
  layout->addWidget(d->m_osFlavorComboBox);
  connect(d->m_osFlavorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AbiWidget::customComboBoxesChanged);

  const auto separator3 = new QLabel(this);
  separator3->setText(QLatin1String("-"));
  separator3->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout->addWidget(separator3);

  d->m_binaryFormatComboBox = new QComboBox(this);
  layout->addWidget(d->m_binaryFormatComboBox);
  for (auto i = 0; i <= static_cast<int>(Abi::UnknownFormat); ++i)
    d->m_binaryFormatComboBox->addItem(Abi::toString(static_cast<Abi::BinaryFormat>(i)), i);
  d->m_binaryFormatComboBox->setCurrentIndex(static_cast<int>(Abi::UnknownFormat));
  connect(d->m_binaryFormatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AbiWidget::customComboBoxesChanged);

  const auto separator4 = new QLabel(this);
  separator4->setText(QLatin1String("-"));
  separator4->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout->addWidget(separator4);

  d->m_wordWidthComboBox = new QComboBox(this);
  layout->addWidget(d->m_wordWidthComboBox);

  d->m_wordWidthComboBox->addItem(Abi::toString(16), 16);
  d->m_wordWidthComboBox->addItem(Abi::toString(32), 32);
  d->m_wordWidthComboBox->addItem(Abi::toString(64), 64);
  d->m_wordWidthComboBox->addItem(Abi::toString(0), 0);
  // Setup current word width of 0 by default.
  d->m_wordWidthComboBox->setCurrentIndex(d->m_wordWidthComboBox->count() - 1);
  connect(d->m_wordWidthComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AbiWidget::customComboBoxesChanged);

  layout->setStretchFactor(d->m_abi, 1);

  setAbis(Abis(), Abi::hostAbi());
}

AbiWidget::~AbiWidget() = default;

static auto selectAbi(const Abi &current, const Abis &abiList) -> Abi
{
  if (!current.isNull())
    return current;
  if (!abiList.isEmpty())
    return abiList.at(0);
  return Abi::hostAbi();
}

auto AbiWidget::setAbis(const Abis &abiList, const Abi &currentAbi) -> void
{
  const auto defaultAbi = selectAbi(currentAbi, abiList);
  {
    const Utils::GuardLocker locker(d->m_ignoreChanges);

    // Initial setup of ABI combobox:
    d->m_abi->clear();
    d->m_abi->addItem(tr("<custom>"), defaultAbi.toString());
    d->m_abi->setCurrentIndex(0);
    d->m_abi->setVisible(!abiList.isEmpty());

    // Add supported ABIs:
    for (const auto &abi : abiList) {
      const auto abiString = abi.toString();

      d->m_abi->addItem(abiString, abiString);
      if (abi == defaultAbi)
        d->m_abi->setCurrentIndex(d->m_abi->count() - 1);
    }

    setCustomAbiComboBoxes(defaultAbi);
  }

  // Update disabled state according to new automatically selected item in main ABI combobox.
  // This will call emitAbiChanged with the actual selected ABI.
  mainComboBoxChanged();
}

auto AbiWidget::supportedAbis() const -> Abis
{
  Abis result;
  result.reserve(d->m_abi->count());
  for (auto i = 1; i < d->m_abi->count(); ++i)
    result << Abi::fromString(d->m_abi->itemData(i).toString());
  return result;
}

auto AbiWidget::isCustomAbi() const -> bool
{
  return d->isCustom();
}

auto AbiWidget::currentAbi() const -> Abi
{
  return d->m_currentAbi;
}

static auto updateOsFlavorCombobox(QComboBox *combo, const Abi::OS os) -> void
{
  const auto flavors = Abi::flavorsForOs(os);
  combo->clear();
  for (const auto &f : flavors)
    combo->addItem(Abi::toString(f), static_cast<int>(f));
  combo->setCurrentIndex(0);
}

auto AbiWidget::customOsComboBoxChanged() -> void
{
  if (d->m_ignoreChanges.isLocked())
    return;

  {
    const Utils::GuardLocker locker(d->m_ignoreChanges);
    d->m_osFlavorComboBox->clear();
    const auto os = static_cast<Abi::OS>(d->m_osComboBox->itemData(d->m_osComboBox->currentIndex()).toInt());
    updateOsFlavorCombobox(d->m_osFlavorComboBox, os);
  }

  customComboBoxesChanged();
}

auto AbiWidget::mainComboBoxChanged() -> void
{
  if (d->m_ignoreChanges.isLocked())
    return;

  const auto newAbi = Abi::fromString(d->m_abi->currentData().toString());
  const auto customMode = d->isCustom();

  d->m_architectureComboBox->setEnabled(customMode);
  d->m_osComboBox->setEnabled(customMode);
  d->m_osFlavorComboBox->setEnabled(customMode);
  d->m_binaryFormatComboBox->setEnabled(customMode);
  d->m_wordWidthComboBox->setEnabled(customMode);

  setCustomAbiComboBoxes(newAbi);

  if (customMode)
    customComboBoxesChanged();
  else
    emitAbiChanged(Abi::fromString(d->m_abi->currentData().toString()));
}

auto AbiWidget::customComboBoxesChanged() -> void
{
  if (d->m_ignoreChanges.isLocked())
    return;

  const Abi current(static_cast<Abi::Architecture>(d->m_architectureComboBox->currentData().toInt()), static_cast<Abi::OS>(d->m_osComboBox->currentData().toInt()), static_cast<Abi::OSFlavor>(d->m_osFlavorComboBox->currentData().toInt()), static_cast<Abi::BinaryFormat>(d->m_binaryFormatComboBox->currentData().toInt()), static_cast<unsigned char>(d->m_wordWidthComboBox->currentData().toInt()));
  d->m_abi->setItemData(0, current.toString()); // Save custom Abi
  emitAbiChanged(current);
}

static auto findIndex(const QComboBox *combo, int data) -> int
{
  for (auto i = 0; i < combo->count(); ++i) {
    if (combo->itemData(i).toInt() == data)
      return i;
  }
  return combo->count() >= 1 ? 0 : -1;
}

static auto setIndex(QComboBox *combo, int data) -> void
{
  combo->setCurrentIndex(findIndex(combo, data));
}

// Sets a custom ABI in the custom abi widgets.
auto AbiWidget::setCustomAbiComboBoxes(const Abi &current) -> void
{
  const Utils::GuardLocker locker(d->m_ignoreChanges);

  setIndex(d->m_architectureComboBox, static_cast<int>(current.architecture()));
  setIndex(d->m_osComboBox, static_cast<int>(current.os()));
  updateOsFlavorCombobox(d->m_osFlavorComboBox, current.os());
  setIndex(d->m_osFlavorComboBox, static_cast<int>(current.osFlavor()));
  setIndex(d->m_binaryFormatComboBox, static_cast<int>(current.binaryFormat()));
  setIndex(d->m_wordWidthComboBox, static_cast<int>(current.wordWidth()));
}

auto AbiWidget::emitAbiChanged(const Abi &current) -> void
{
  if (current == d->m_currentAbi)
    return;

  d->m_currentAbi = current;
  emit abiChanged();
}

} // namespace ProjectExplorer
