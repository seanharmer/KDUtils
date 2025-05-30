/*
  This file is part of KDUtils.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: BogDan Vatra <bogdan@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGui/abstract_gui_platform_integration.h>
#include <KDGui/kdgui_global.h>
#include <KDUtils/logging.h>

#include <unordered_set>

#ifdef __cplusplus
extern "C" {
#endif
struct android_app;
#ifdef __cplusplus
}
#endif

namespace KDGui {

class KDGUI_API AndroidPlatformIntegration : public AbstractGuiPlatformIntegration
{
public:
    AndroidPlatformIntegration();
    void handleWindowResize();
    void registerPlatformWindow(AbstractPlatformWindow *window);
    void unregisterPlatformWindow(AbstractPlatformWindow *window);
    void handleKeyEvent(int32_t action, int32_t code, int32_t meta, int64_t time);
    void handleTouchEvent(int32_t action, int64_t xPos, int64_t yPos, int64_t time);

    AbstractClipboard *clipboard() override
    {
        // TODO(android): Implement clipboard
        return nullptr;
    }

    KDUtils::Dir standardDir(const KDFoundation::CoreApplication &app, KDFoundation::StandardDir type) const override;

    static android_app *s_androidApp;

private:
    KDFoundation::AbstractPlatformEventLoop *createPlatformEventLoopImpl() override;
    AbstractPlatformWindow *createPlatformWindowImpl(Window *window) override;

private:
    std::unordered_set<AbstractPlatformWindow *> m_windows;
};

} // namespace KDGui
