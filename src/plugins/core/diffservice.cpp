// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "diffservice.h"

namespace Core {

static DiffService *g_instance = nullptr;

DiffService::DiffService()
{
  g_instance = this;
}

DiffService::~DiffService()
{
  g_instance = nullptr;
}

auto DiffService::instance() -> DiffService*
{
  return g_instance;
}

} // Core
