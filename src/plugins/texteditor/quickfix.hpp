// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QList>
#include <QString>
#include <QMetaType>
#include <QSharedPointer>

namespace TextEditor {

class AssistInterface;

/*!
    Class to perform a single quick-fix.

    Quick-fix operations cannot be copied, and must be passed around as explicitly
    shared pointers ( QuickFixOperation::Ptr ).

    Subclasses should make sure that they copy parts of, or the whole QuickFixState ,
    which are needed to perform the quick-fix operation.
 */
class TEXTEDITOR_EXPORT QuickFixOperation {
  Q_DISABLE_COPY(QuickFixOperation)

public:
  using Ptr = QSharedPointer<QuickFixOperation>;
  
  QuickFixOperation(int priority = -1);
  virtual ~QuickFixOperation();

  /*!
      \returns The priority for this quick-fix. See the QuickFixCollector for more
               information.
   */
  virtual auto priority() const -> int;

  /// Sets the priority for this quick-fix operation.
  auto setPriority(int priority) -> void;

  /*!
      \returns The description for this quick-fix. This description is shown to the
               user.
   */
  virtual auto description() const -> QString;

  /// Sets the description for this quick-fix, which will be shown to the user.
  auto setDescription(const QString &description) -> void;

  /*!
      Perform this quick-fix's operation.

      Subclasses should implement this function to do the actual changes.
   */
  virtual auto perform() -> void = 0;

private:
  int _priority;
  QString _description;
};

using QuickFixOperations = QList<QuickFixOperation::Ptr>;

inline auto operator<<(QuickFixOperations &list, QuickFixOperation *op) -> QuickFixOperations&
{
  list.append(QuickFixOperation::Ptr(op));
  return list;
}

using QuickFixInterface = QSharedPointer<const AssistInterface>;

} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::QuickFixOperation::Ptr)
