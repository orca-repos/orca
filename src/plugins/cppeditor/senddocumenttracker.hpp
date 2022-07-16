// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QObject>

#include <limits>

namespace CppEditor {

class CPPEDITOR_EXPORT SendDocumentTracker {
public:
  auto setLastSentRevision(int lastSentRevision) -> void;
  auto lastSentRevision() const -> int;
  auto setLastCompletionPosition(int lastCompletionPosition) -> void;
  auto lastCompletionPosition() const -> int;
  auto applyContentChange(int startPosition) -> void;
  auto shouldSendCompletion(int newCompletionPosition) const -> bool;
  auto shouldSendRevision(uint newRevision) const -> bool;
  auto shouldSendRevisionWithCompletionPosition(int newRevision, int newCompletionPosition) const -> bool;

private:
  auto changedBeforeCompletionPosition(int newCompletionPosition) const -> bool;
  
  int m_lastSentRevision = -1;
  int m_lastCompletionPosition = -1;
  int m_contentChangeStartPosition = std::numeric_limits<int>::max();
};

#ifdef WITH_TESTS
namespace Internal {
class DocumentTrackerTest : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultLastSentRevision();
    void testSetRevision();
    void testSetLastCompletionPosition();
    void testApplyContentChange();
    void testDontSendCompletionIfPositionIsEqual();
    void testSendCompletionIfPositionIsDifferent();
    void testSendCompletionIfChangeIsBeforeCompletionPositionAndPositionIsEqual();
    void testDontSendCompletionIfChangeIsAfterCompletionPositionAndPositionIsEqual();
    void testDontSendRevisionIfRevisionIsEqual();
    void testSendRevisionIfRevisionIsDifferent();
    void testDontSendRevisionWithDefaults();
    void testDontSendIfRevisionIsDifferentAndCompletionPositionIsEqualAndNoContentChange();
    void testDontSendIfRevisionIsDifferentAndCompletionPositionIsDifferentAndNoContentChange();
    void testDontSendIfRevisionIsEqualAndCompletionPositionIsDifferentAndNoContentChange();
    void testSendIfChangeIsBeforeCompletionAndPositionIsEqualAndRevisionIsDifferent();
    void testDontSendIfChangeIsAfterCompletionPositionAndRevisionIsDifferent();
    void testSendIfChangeIsBeforeCompletionPositionAndRevisionIsDifferent();
    void testResetChangedContentStartPositionIfLastRevisionIsSet();
};
} // namespace Internal
#endif // WITH_TESTS

} // namespace CppEditor
