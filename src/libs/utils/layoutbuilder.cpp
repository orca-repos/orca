// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "layoutbuilder.hpp"

#include <utils/algorithm.hpp>
#include <utils/aspects.hpp>
#include <utils/qtcassert.hpp>

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QStyle>
#include <QWidget>

namespace Utils {

/*!
    \enum Utils::LayoutBuilder::LayoutType
    \inmodule Orca

    The LayoutType enum describes the type of \c QLayout a layout builder
    operates on.

    \value Form
    \value Grid
    \value HBox
    \value VBox
*/

/*!
    \class Utils::LayoutBuilder::LayoutItem
    \inmodule Orca

    \brief The LayoutItem class represents widgets, layouts, and aggregate
    items for use in conjunction with layout builders.

    Layout items are typically implicitly constructed when adding items to a
    \c LayoutBuilder instance using \c LayoutBuilder::addItem() or
    \c LayoutBuilder::addItems() and never stored in user code.
*/

/*!
    Constructs a layout item instance representing an empty cell.
 */
LayoutBuilder::LayoutItem::LayoutItem() {}

/*!
    Constructs a layout item proxy for \a layout.
 */
LayoutBuilder::LayoutItem::LayoutItem(QLayout *layout) : layout(layout) {}

/*!
    Constructs a layout item proxy for \a widget.
 */
LayoutBuilder::LayoutItem::LayoutItem(QWidget *widget) : widget(widget) {}

/*!
    Constructs a layout item representing a \c BaseAspect.

    This ultimately uses the \a aspect's \c addToLayout(LayoutBuilder &) function,
    which in turn can add one or more layout items to the target layout.

    \sa BaseAspect::addToLayout()
 */
LayoutBuilder::LayoutItem::LayoutItem(BaseAspect &aspect) : aspect(&aspect) {}

LayoutBuilder::LayoutItem::LayoutItem(BaseAspect *aspect) : aspect(aspect) {}

/*!
    Constructs a layout item containing some static \a text.
 */
LayoutBuilder::LayoutItem::LayoutItem(const QString &text) : text(text) {}

auto LayoutBuilder::createLayout() const -> QLayout*
{
  QLayout *layout = nullptr;
  switch (m_layoutType) {
  case LayoutBuilder::FormLayout: {
    auto formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout = formLayout;
    break;
  }
  case LayoutBuilder::GridLayout: {
    auto gridLayout = new QGridLayout;
    layout = gridLayout;
    break;
  }
  case LayoutBuilder::HBoxLayout: {
    auto hboxLayout = new QHBoxLayout;
    layout = hboxLayout;
    break;
  }
  case LayoutBuilder::VBoxLayout: {
    auto vboxLayout = new QVBoxLayout;
    layout = vboxLayout;
    break;
  }
  }
  QTC_ASSERT(layout, return nullptr);
  if (m_spacing)
    layout->setSpacing(*m_spacing);
  return layout;
}

static auto setMargins(bool on, QLayout *layout) -> void
{
  const int d = on ? 9 : 0;
  layout->setContentsMargins(d, d, d, d);
}

static auto flushPendingFormItems(QFormLayout *formLayout, LayoutBuilder::LayoutItems &pendingFormItems) -> void
{
  QTC_ASSERT(formLayout, return);

  if (pendingFormItems.empty())
    return;

  // If there are more than two items, we cram the last ones in one hbox.
  if (pendingFormItems.size() > 2) {
    auto hbox = new QHBoxLayout;
    setMargins(false, hbox);
    for (int i = 1; i < pendingFormItems.size(); ++i) {
      if (QWidget *w = pendingFormItems.at(i).widget)
        hbox->addWidget(w);
      else if (QLayout *l = pendingFormItems.at(i).layout)
        hbox->addLayout(l);
      else
      QTC_CHECK(false);
    }
    while (pendingFormItems.size() >= 2)
      pendingFormItems.pop_back();
    pendingFormItems.append(LayoutBuilder::LayoutItem(hbox));
  }

  if (pendingFormItems.size() == 1) {
    // One one item given, so this spans both columns.
    if (auto layout = pendingFormItems.at(0).layout)
      formLayout->addRow(layout);
    else if (auto widget = pendingFormItems.at(0).widget)
      formLayout->addRow(widget);
  } else if (pendingFormItems.size() == 2) {
    // Normal case, both columns used.
    if (auto label = pendingFormItems.at(0).widget) {
      if (auto layout = pendingFormItems.at(1).layout)
        formLayout->addRow(label, layout);
      else if (auto widget = pendingFormItems.at(1).widget)
        formLayout->addRow(label, widget);
    } else {
      if (auto layout = pendingFormItems.at(1).layout)
        formLayout->addRow(pendingFormItems.at(0).text, layout);
      else if (auto widget = pendingFormItems.at(1).widget)
        formLayout->addRow(pendingFormItems.at(0).text, widget);
    }
  } else {
    QTC_CHECK(false);
  }

  pendingFormItems.clear();
}

static auto doLayoutHelper(QLayout *layout, const LayoutBuilder::LayoutItems &items, int currentGridRow = 0) -> void
{
  int currentGridColumn = 0;
  LayoutBuilder::LayoutItems pendingFormItems;

  auto formLayout = qobject_cast<QFormLayout*>(layout);
  auto gridLayout = qobject_cast<QGridLayout*>(layout);
  auto boxLayout = qobject_cast<QBoxLayout*>(layout);

  for (const LayoutBuilder::LayoutItem &item : items) {
    if (item.specialType == LayoutBuilder::SpecialType::Break) {
      if (formLayout)
        flushPendingFormItems(formLayout, pendingFormItems);
      else if (gridLayout) {
        if (currentGridColumn != 0) {
          ++currentGridRow;
          currentGridColumn = 0;
        }
      }
      continue;
    }

    QWidget *widget = item.widget;

    if (gridLayout) {
      Qt::Alignment align;
      if (item.align == LayoutBuilder::AlignmentType::AlignAsFormLabel)
        align = Qt::Alignment(widget->style()->styleHint(QStyle::SH_FormLayoutLabelAlignment));
      if (widget)
        gridLayout->addWidget(widget, currentGridRow, currentGridColumn, 1, item.span, align);
      else if (item.layout)
        gridLayout->addLayout(item.layout, currentGridRow, currentGridColumn, 1, item.span, align);
      currentGridColumn += item.span;
    } else if (boxLayout) {
      if (widget) {
        boxLayout->addWidget(widget);
      } else if (item.layout) {
        boxLayout->addLayout(item.layout);
      } else if (item.specialType == LayoutBuilder::SpecialType::Stretch) {
        boxLayout->addStretch(item.specialValue.toInt());
      } else if (item.specialType == LayoutBuilder::SpecialType::Space) {
        boxLayout->addSpacing(item.specialValue.toInt());
      }
    } else {
      pendingFormItems.append(item);
    }
  }

  if (formLayout)
    flushPendingFormItems(formLayout, pendingFormItems);
}

/*!
    Constructs a layout item from the contents of another LayoutBuilder
 */
LayoutBuilder::LayoutItem::LayoutItem(const LayoutBuilder &builder)
{
  layout = builder.createLayout();
  doLayoutHelper(layout, builder.m_items);
  setMargins(builder.m_withMargins, layout);
}

/*!
    \class Utils::LayoutBuilder::Space
    \inmodule Orca

    \brief The LayoutBuilder::Space class represents some empty space in a layout.
 */

/*!
    \class Utils::LayoutBuilder::Stretch
    \inmodule Orca

    \brief The LayoutBuilder::Stretch class represents some stretch in a layout.
 */

/*!
    \class Utils::LayoutBuilder
    \inmodule Orca

    \brief The LayoutBuilder class provides a convenient way to fill \c QFormLayout
    and \c QGridLayouts with contents.

    Filling a layout with items happens item-by-item, row-by-row.

    A LayoutBuilder instance is typically used locally within a function and never stored.

    \sa addItem(), addItems(), addRow(), finishRow()
*/

LayoutBuilder::LayoutBuilder(LayoutType layoutType, const LayoutItems &items) : m_layoutType(layoutType)
{
  m_items.reserve(items.size() * 2);
  for (const LayoutItem &item : items)
    addItem(item);
}

auto LayoutBuilder::setSpacing(int spacing) -> LayoutBuilder&
{
  m_spacing = spacing;
  return *this;
}

LayoutBuilder::LayoutBuilder() = default;

/*!
    Destructs a layout builder.
 */
LayoutBuilder::~LayoutBuilder() = default;

/*!
    Instructs a layout builder to finish the current row.
    This is implicitly called by LayoutBuilder's destructor.
 */
auto LayoutBuilder::finishRow() -> LayoutBuilder&
{
  addItem(Break());
  return *this;
}

/*!
    This starts a new row containing the \a item. The row can be further extended by
    other items using \c addItem() or \c addItems().

    \sa finishRow(), addItem(), addItems()
 */
auto LayoutBuilder::addRow(const LayoutItem &item) -> LayoutBuilder&
{
  return finishRow().addItem(item);
}

/*!
    This starts a new row containing \a items. The row can be further extended by
    other items using \c addItem() or \c addItems().

    \sa finishRow(), addItem(), addItems()
 */
auto LayoutBuilder::addRow(const LayoutItems &items) -> LayoutBuilder&
{
  return finishRow().addItems(items);
}

/*!
    Adds the layout item \a item to the current row.
 */
auto LayoutBuilder::addItem(const LayoutItem &item) -> LayoutBuilder&
{
  if (item.aspect) {
    item.aspect->addToLayout(*this);
    if (m_layoutType == FormLayout || m_layoutType == VBoxLayout)
      finishRow();
  } else {
    m_items.push_back(item);
  }
  return *this;
}

auto LayoutBuilder::doLayout(QWidget *parent) -> void
{
  QLayout *layout = createLayout();
  parent->setLayout(layout);

  doLayoutHelper(layout, m_items);
  setMargins(m_withMargins, layout);
}

/*!
    Adds the layout item \a items to the current row.
 */
auto LayoutBuilder::addItems(const LayoutItems &items) -> LayoutBuilder&
{
  for (const LayoutItem &item : items)
    addItem(item);
  return *this;
}

/*!
    Attach the constructed layout to the provided \c QWidget \a parent.

    This operation can only be performed once per LayoutBuilder instance.
 */
auto LayoutBuilder::attachTo(QWidget *w, bool withMargins) -> void
{
  m_withMargins = withMargins;
  doLayout(w);
}

auto LayoutBuilder::emerge(bool withMargins) -> QWidget*
{
  m_withMargins = withMargins;
  auto w = new QWidget;
  doLayout(w);
  return w;
}

/*!
    Constructs a layout extender to extend an existing \a layout.

    This constructor can be used to continue the work of previous layout building.
    The type of the underlying layout and previous contents will be retained,
    new items will be added below existing ones.
 */

LayoutExtender::LayoutExtender(QLayout *layout) : m_layout(layout) {}

LayoutExtender::~LayoutExtender()
{
  QTC_ASSERT(m_layout, return);
  int currentGridRow = 0;
  if (auto gridLayout = qobject_cast<QGridLayout*>(m_layout))
    currentGridRow = gridLayout->rowCount();
  doLayoutHelper(m_layout, m_items, currentGridRow);
}

// Special items

LayoutBuilder::Break::Break()
{
  specialType = SpecialType::Break;
}

LayoutBuilder::Stretch::Stretch(int stretch)
{
  specialType = SpecialType::Stretch;
  specialValue = stretch;
}

LayoutBuilder::Space::Space(int space)
{
  specialType = SpecialType::Space;
  specialValue = space;
}

LayoutBuilder::Title::Title(const QString &title, BoolAspect *check)
{
  specialType = SpecialType::Title;
  specialValue = title;
  aspect = check;
}

LayoutBuilder::Span::Span(int span_, const LayoutItem &item)
{
  LayoutBuilder::LayoutItem::operator=(item);
  span = span_;
}

LayoutBuilder::AlignAsFormLabel::AlignAsFormLabel(const LayoutItem &item)
{
  LayoutBuilder::LayoutItem::operator=(item);
  align = AlignmentType::AlignAsFormLabel;
}

namespace Layouting {

Group::Group(std::initializer_list<LayoutItem> items)
{
  auto box = new QGroupBox;
  Column builder;
  bool innerMargins = true;
  for (const LayoutItem &item : items) {
    if (item.specialType == LayoutBuilder::SpecialType::Title) {
      box->setTitle(item.specialValue.toString());
      box->setObjectName(item.specialValue.toString());
      if (auto check = qobject_cast<BoolAspect*>(item.aspect)) {
        box->setCheckable(true);
        box->setChecked(check->value());
        check->setHandlesGroup(box);
      }
    } else {
      builder.addItem(item);
    }
  }
  builder.attachTo(box, innerMargins);
  widget = box;
}

} // Layouting
} // Utils
