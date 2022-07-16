// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtversionfactory.hpp"

namespace QtSupport {
namespace Internal {

class DesktopQtVersionFactory : public QtVersionFactory {
public:
  DesktopQtVersionFactory();
};

class EmbeddedLinuxQtVersionFactory : public QtVersionFactory {
public:
  EmbeddedLinuxQtVersionFactory();
};

} // Internal
} // QtSupport
