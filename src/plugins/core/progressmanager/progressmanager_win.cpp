// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "progressmanager_p.hpp"

#include <core/icore.hpp>

#include <utils/utilsicons.hpp>

#include <QGuiApplication>
#include <QMainWindow>
#include <QFont>
#include <QPixmap>
#include <QPainter>
#include <QWindow>
#include <QLabel>

#include <qpa/qplatformnativeinterface.h>

// for windows progress bar
#ifndef __GNUC__
#    define CALLBACK WINAPI
#    include <ShObjIdl.h>
#endif

// Windows 7 SDK required
#ifdef __ITaskbarList3_INTERFACE_DEFINED__

namespace {
int total = 0;
ITaskbarList3 *p_i_task = nullptr;
}

QT_BEGIN_NAMESPACE Q_GUI_EXPORT auto qt_pixmapToWinHICON(const QPixmap &p) -> HICON;
QT_END_NAMESPACE

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

auto Core::Internal::ProgressManagerPrivate::initInternal() -> void
{
  CoInitialize(nullptr);

  if (const auto h_res = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, reinterpret_cast<LPVOID*>(&p_i_task)); FAILED(h_res)) {
    p_i_task = nullptr;
    CoUninitialize();
    return;
  }

  p_i_task->HrInit();
}

auto Core::Internal::ProgressManagerPrivate::cleanup() -> void
{
  if (p_i_task) {
    p_i_task->Release();
    p_i_task = nullptr;
    CoUninitialize();
  }
}

auto Core::Internal::ProgressManagerPrivate::doSetApplicationLabel(const QString &text) -> void
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

auto Core::Internal::ProgressManagerPrivate::setApplicationProgressRange(int min, int max) -> void
{
  total = max - min;
}

auto Core::Internal::ProgressManagerPrivate::setApplicationProgressValue(int value) -> void
{
  if (p_i_task) {
    const auto winId = hwndOfWidget(Core::ICore::mainWindow());
    p_i_task->SetProgressValue(winId, value, total);
  }
}

auto Core::Internal::ProgressManagerPrivate::setApplicationProgressVisible(bool visible) -> void
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

void Core::Internal::ProgressManagerPrivate::initInternal() {}

void Core::Internal::ProgressManagerPrivate::cleanup() {}

void Core::Internal::ProgressManagerPrivate::doSetApplicationLabel(const QString &text)
{
  Q_UNUSED(text)
}

void Core::Internal::ProgressManagerPrivate::setApplicationProgressRange(int min, int max)
{
  Q_UNUSED(min)
  Q_UNUSED(max)
}

void Core::Internal::ProgressManagerPrivate::setApplicationProgressValue(int value)
{
  Q_UNUSED(value)
}

void Core::Internal::ProgressManagerPrivate::setApplicationProgressVisible(bool visible)
{
  Q_UNUSED(visible)
}


#endif // __ITaskbarList2_INTERFACE_DEFINED__
