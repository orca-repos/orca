// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-interface.hpp"
#include "core-progress-manager-private.hpp"

#include <qpa/qplatformnativeinterface.h>

#include <utils/utilsicons.hpp>

#include <QFont>
#include <QGuiApplication>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

// for windows progress bar
#ifndef __GNUC__
#    define CALLBACK WINAPI
#    include <ShObjIdl.h>
#endif

// Windows 7 SDK required
#ifdef __ITaskbarList3_INTERFACE_DEFINED__

QT_BEGIN_NAMESPACE
Q_GUI_EXPORT auto qt_pixmapToWinHICON(const QPixmap &p) -> HICON;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {
namespace {
int total = 0;
ITaskbarList3 *p_i_task = nullptr;
} // namespace

static auto windowOfWidget(const QWidget *widget) -> QWindow*
{
  if (const auto window = widget->windowHandle())
    return window;

  if (const auto top_level = widget->nativeParentWidget())
    return top_level->windowHandle();

  return nullptr;
}

static auto hwndOfWidget(const QWidget *w) -> HWND
{
  void *result = nullptr;

  if (const auto window = windowOfWidget(w))
    result = QGuiApplication::platformNativeInterface()->nativeResourceForWindow("handle", window);

  return static_cast<HWND>(result);
}

auto ProgressManagerPrivate::initInternal() -> void
{
  CoInitialize(nullptr);

  if (const auto h_res = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, reinterpret_cast<LPVOID*>(&p_i_task)); FAILED(h_res)) {
    p_i_task = nullptr;
    CoUninitialize();
    return;
  }

  p_i_task->HrInit();
}

auto ProgressManagerPrivate::cleanup() -> void
{
  if (p_i_task) {
    p_i_task->Release();
    p_i_task = nullptr;
    CoUninitialize();
  }
}

auto ProgressManagerPrivate::doSetApplicationLabel(const QString &text) -> void
{
  if (!p_i_task)
    return;

  const auto win_id = hwndOfWidget(Core::ICore::mainWindow());

  if (text.isEmpty()) {
    p_i_task->SetOverlayIcon(win_id, NULL, nullptr);
  } else {
    auto pix = Utils::Icons::ERROR_TASKBAR.pixmap();
    pix.setDevicePixelRatio(1); // We want device-pixel sized font depending on the pix.height
    QPainter p(&pix);
    p.setPen(Qt::white);
    auto font = p.font();
    font.setPixelSize(static_cast<int>(pix.height() * 0.5));
    p.setFont(font);
    p.drawText(pix.rect(), Qt::AlignCenter, text);
    const auto icon = qt_pixmapToWinHICON(pix);
    p_i_task->SetOverlayIcon(win_id, icon, (wchar_t*)text.utf16());
    DestroyIcon(icon);
  }
}

auto ProgressManagerPrivate::setApplicationProgressRange(int min, int max) -> void
{
  total = max - min;
}

auto ProgressManagerPrivate::setApplicationProgressValue(int value) -> void
{
  if (p_i_task) {
    const auto winId = hwndOfWidget(Core::ICore::mainWindow());
    p_i_task->SetProgressValue(winId, value, total);
  }
}

auto ProgressManagerPrivate::setApplicationProgressVisible(bool visible) -> void
{
  if (!p_i_task)
    return;

  const auto winId = hwndOfWidget(Core::ICore::mainWindow());
  if (visible)
    p_i_task->SetProgressState(winId, TBPF_NORMAL);
  else
    p_i_task->SetProgressState(winId, TBPF_NOPROGRESS);
}

#else

void Core::ProgressManagerPrivate::initInternal() {}

void Core::ProgressManagerPrivate::cleanup() {}

void Core::ProgressManagerPrivate::doSetApplicationLabel(const QString &text)
{
  Q_UNUSED(text)
}

void Core::ProgressManagerPrivate::setApplicationProgressRange(int min, int max)
{
  Q_UNUSED(min)
  Q_UNUSED(max)
}

void Core::ProgressManagerPrivate::setApplicationProgressValue(int value)
{
  Q_UNUSED(value)
}

void Core::ProgressManagerPrivate::setApplicationProgressVisible(bool visible)
{
  Q_UNUSED(visible)
}


#endif // __ITaskbarList2_INTERFACE_DEFINED__

} // namespace Orca::Plugin::Core
