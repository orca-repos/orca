// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-document-factory-interface.hpp"

#include <utils/qtcassert.hpp>

namespace Orca::Plugin::Core {

static QList<IDocumentFactory*> g_document_factories;

IDocumentFactory::IDocumentFactory()
{
  g_document_factories.append(this);
}

IDocumentFactory::~IDocumentFactory()
{
  g_document_factories.removeOne(this);
}

auto IDocumentFactory::allDocumentFactories() -> QList<IDocumentFactory*>
{
  return g_document_factories;
}

auto IDocumentFactory::open(const Utils::FilePath &file_path) const -> IDocument*
{
  QTC_ASSERT(m_opener, return nullptr);
  return m_opener(file_path);
}

} // namespace Orca::Plugin::Core
