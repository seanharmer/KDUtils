/*
  This file is part of KDUtils.

  SPDX-FileCopyrightText: 2018 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Paul Lemire <paul.lemire@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <unordered_map>
#include <vector>

#include <KDGui/abstract_gui_platform_integration.h>
#include <KDGui/platform/linux/wayland/linux_wayland_platform_event_loop.h>
#include <KDGui/platform/linux/wayland/linux_wayland_platform_window.h>
#include <KDGui/platform/linux/wayland/linux_wayland_clipboard.h>
#include <KDGui/kdgui_global.h>

#include <KDUtils/logging.h>

struct wl_cursor_theme;
struct wl_display;
struct wl_event_queue;
struct wl_registry;
struct wl_compositor;
struct wl_shm;
struct xdg_wm_base;
struct zwp_relative_pointer_manager_v1;
struct zwp_pointer_constraints_v1;
struct zxdg_decoration_manager_v1;
struct wl_data_device_manager;

namespace KDGui {

class LinuxWaylandPlatformOutput;
class LinuxWaylandPlatformInput;

class KDGUI_API LinuxWaylandPlatformIntegration : public AbstractGuiPlatformIntegration
{
public:
    template<typename T>
    struct Global {
        T *object{ nullptr };
        uint32_t version{ 0 };
        uint32_t id{ 0 };
    };

    LinuxWaylandPlatformIntegration();
    ~LinuxWaylandPlatformIntegration() override;

    static bool checkAvailable();

    AbstractClipboard *clipboard() override;

    std::shared_ptr<spdlog::logger> logger() { return m_logger; }

    wl_display *display() const { return m_display; }
    wl_event_queue *queue() const { return m_queue; }

    const Global<wl_compositor> &compositor() const { return m_compositor; }
    const Global<xdg_wm_base> &xdgShell() const { return m_xdgShell; }
    const Global<zwp_relative_pointer_manager_v1> &relativePointerManagerV1() const { return m_relativePointerV1; }
    const Global<zwp_pointer_constraints_v1> &pointerConstraintsV1() const { return m_pointerConstraintsV1; }
    const Global<zxdg_decoration_manager_v1> &xdgDecoration() const { return m_decorationV1; }
    const Global<wl_data_device_manager> &dataDeviceManager() const { return m_dataDeviceManager; }

    wl_cursor_theme *cursorTheme() const { return m_cursorTheme; }

    void registerWindowForEvents(wl_surface *surface, LinuxWaylandPlatformWindow *window)
    {
        m_windows.insert({ surface, window });
    }
    void unregisterWindowForEvents(wl_surface *surface) { m_windows.erase(surface); }
    LinuxWaylandPlatformWindow *window(wl_surface *surface) const;

    KDUtils::Dir standardDir(const KDFoundation::CoreApplication &app, KDFoundation::StandardDir type) const override;

private:
    LinuxWaylandPlatformEventLoop *createPlatformEventLoopImpl() override;
    LinuxWaylandPlatformWindow *createPlatformWindowImpl(Window *window) override;

    void init() final;
    void global(wl_registry *registry, uint32_t id, const char *name, uint32_t version);
    void globalRemove(wl_registry *registry, uint32_t id);
    void ping(xdg_wm_base *xdgShell, uint32_t serial);

    std::shared_ptr<spdlog::logger> m_logger;
    wl_display *m_display{ nullptr };
    wl_event_queue *m_queue{ nullptr };
    wl_display *m_displayProxy{ nullptr };
    wl_registry *m_registry{ nullptr };
    Global<wl_compositor> m_compositor;
    Global<xdg_wm_base> m_xdgShell;
    Global<wl_shm> m_shm;
    Global<zwp_relative_pointer_manager_v1> m_relativePointerV1;
    Global<zwp_pointer_constraints_v1> m_pointerConstraintsV1;
    Global<zxdg_decoration_manager_v1> m_decorationV1;
    Global<wl_data_device_manager> m_dataDeviceManager;

    std::unordered_map<wl_surface *, LinuxWaylandPlatformWindow *> m_windows;
    std::vector<std::unique_ptr<LinuxWaylandPlatformInput>> m_inputs;
    std::vector<std::unique_ptr<LinuxWaylandPlatformOutput>> m_outputs;
    wl_cursor_theme *m_cursorTheme{ nullptr };

    std::unique_ptr<LinuxWaylandClipboard> m_clipboard;
};

template<typename F>
struct WlCallbackWrapper;

template<class T, class... Args>
struct WlCallbackWrapper<void (T::*)(Args...)> {
    template<void (T::*F)(Args...)>
    static void forward(void *data, Args... args)
    {
        (static_cast<T *>(data)->*F)(std::forward<Args>(args)...);
    }
};

template<auto F>
auto wrapWlCallback = WlCallbackWrapper<decltype(F)>::template forward<F>;

} // namespace KDGui
