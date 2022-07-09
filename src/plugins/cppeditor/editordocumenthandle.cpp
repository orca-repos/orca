// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "editordocumenthandle.hpp"

namespace CppEditor {

/*!
    \class CppEditor::EditorDocumentHandle

    \brief The EditorDocumentHandle class provides an interface to an opened
           C++ editor document.
*/

CppEditorDocumentHandle::~CppEditorDocumentHandle() = default;

auto CppEditorDocumentHandle::sendTracker() -> SendDocumentTracker&
{
  return m_sendTracker;
}

auto CppEditorDocumentHandle::refreshReason() const -> CppEditorDocumentHandle::RefreshReason
{
  return m_refreshReason;
}

auto CppEditorDocumentHandle::setRefreshReason(const RefreshReason &refreshReason) -> void
{
  m_refreshReason = refreshReason;
}

} // namespace CppEditor
