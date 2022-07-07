// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QtGlobal>

#include <type_traits>

namespace Utils {

/// A class useful for the implementation of the -> operator of proxy iterators.
template <typename Reference>
struct ArrowProxy {
  Reference r;

  auto operator->() -> Reference*
  {
    return &r;
  }
};

/// Random-access const iterator over elements of a container providing an overloaded operator[]
/// (which may return a proxy object, like std::vector<bool>, rather than a reference).
template <typename Container>
class IndexedContainerProxyConstIterator {
public:
  using iterator_category = std::random_access_iterator_tag;
  using difference_type = typename std::make_signed<typename Container::size_type>::type;
  using value_type = typename Container::value_type;
  using reference = value_type;
  using pointer = ArrowProxy<reference>;
  using size_type = typename Container::size_type;

  IndexedContainerProxyConstIterator() : m_container(nullptr), m_index(0) {}

  IndexedContainerProxyConstIterator(const Container &container, size_type index) : m_container(&container), m_index(index) {}

  auto operator*() const -> reference
  {
    Q_ASSERT(m_container);
    return (*m_container)[m_index];
  }

  auto operator->() const -> pointer
  {
    Q_ASSERT(m_container);
    return pointer{(*m_container)[m_index]};
  }

  auto operator[](difference_type j) const -> reference
  {
    Q_ASSERT(m_container);
    return (*m_container)[m_index + j];
  }

  auto operator==(const IndexedContainerProxyConstIterator &other) const -> bool
  {
    Q_ASSERT(m_container == other.m_container);
    return m_index == other.m_index;
  }

  auto operator!=(const IndexedContainerProxyConstIterator &other) const -> bool
  {
    Q_ASSERT(m_container == other.m_container);
    return m_index != other.m_index;
  }

  auto operator<(const IndexedContainerProxyConstIterator &other) const -> bool
  {
    Q_ASSERT(m_container == other.m_container);
    return m_index < other.m_index;
  }

  auto operator<=(const IndexedContainerProxyConstIterator &other) const -> bool
  {
    Q_ASSERT(m_container == other.m_container);
    return m_index <= other.m_index;
  }

  auto operator>(const IndexedContainerProxyConstIterator &other) const -> bool
  {
    Q_ASSERT(m_container == other.m_container);
    return m_index > other.m_index;
  }

  auto operator>=(const IndexedContainerProxyConstIterator &other) const -> bool
  {
    Q_ASSERT(m_container == other.m_container);
    return m_index >= other.m_index;
  }

  auto operator++() -> IndexedContainerProxyConstIterator&
  {
    ++m_index;
    return *this;
  }

  auto operator++(int) -> IndexedContainerProxyConstIterator
  {
    IndexedContainerProxyConstIterator copy(*this);
    ++m_index;
    return copy;
  }

  auto operator--() -> IndexedContainerProxyConstIterator&
  {
    --m_index;
    return *this;
  }

  auto operator--(int) -> IndexedContainerProxyConstIterator
  {
    IndexedContainerProxyConstIterator copy(*this);
    --m_index;
    return copy;
  }

  auto operator+=(difference_type j) -> IndexedContainerProxyConstIterator&
  {
    m_index += j;
    return *this;
  }

  auto operator-=(difference_type j) -> IndexedContainerProxyConstIterator&
  {
    m_index -= j;
    return *this;
  }

  auto operator+(difference_type j) const -> IndexedContainerProxyConstIterator
  {
    IndexedContainerProxyConstIterator result(*this);
    result += j;
    return result;
  }

  auto operator-(difference_type j) const -> IndexedContainerProxyConstIterator
  {
    IndexedContainerProxyConstIterator result(*this);
    result -= j;
    return result;
  }

  friend auto operator+(difference_type j, const IndexedContainerProxyConstIterator &other) -> IndexedContainerProxyConstIterator
  {
    return other + j;
  }

  auto operator-(const IndexedContainerProxyConstIterator &other) const -> difference_type
  {
    return static_cast<difference_type>(m_index) - static_cast<difference_type>(other.m_index);
  }

private:
  const Container *m_container;
  size_type m_index;
};

} // namespace Utils
