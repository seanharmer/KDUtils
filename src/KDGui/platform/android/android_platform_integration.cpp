/*
  This file is part of KDUtils.

SPDX-FileCopyrightText: 2018-2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
Author: BogDan Vatra <bogdan@kdab.com>

SPDX-License-Identifier: AGPL-3.0-only

Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "android_platform_integration.h"

#include <android_native_app_glue.h>
#include <KDFoundation/core_application.h>
#include <KDGui/window.h>

#include <dlfcn.h>
#include <filesystem>

#include "android_platform_event_loop.h"
#include "android_platform_window.h"

namespace KDGui {

android_app *KDGui::AndroidPlatformIntegration::s_androidApp = nullptr;

KDFoundation::AbstractPlatformEventLoop *AndroidPlatformIntegration::createPlatformEventLoopImpl()
{
    return new AndroidPlatformEventLoop(this);
}

AbstractPlatformWindow *AndroidPlatformIntegration::createPlatformWindowImpl(Window *window)
{
    return new AndroidPlatformWindow(this, window);
}

AndroidPlatformIntegration::AndroidPlatformIntegration()
{
    KDFoundation::CoreApplication::registerFileReader([](const std::string &fileName) -> std::optional<std::vector<uint8_t>> {
        auto path = std::filesystem::path{ fileName }.lexically_normal();
        if (path.empty())
            return {};
        const char *p = *path.c_str() == '/' ? path.c_str() + 1 : path.c_str();
        if (auto afile = AAssetManager_open(s_androidApp->activity->assetManager, p, AASSET_MODE_BUFFER)) {
            size_t fileLength = AAsset_getLength(afile);
            auto res = std::make_optional<std::vector<uint8_t>>();
            res->resize(fileLength);
            AAsset_read(afile, res->data(), fileLength);
            AAsset_close(afile);
            return res;
        }
        return {};
    });
}

void AndroidPlatformIntegration::handleWindowResize()
{
    const int width = ANativeWindow_getWidth(s_androidApp->window);
    const int height = ANativeWindow_getHeight(s_androidApp->window);
    for (auto *platformWindow : m_windows) {
        const auto *w = platformWindow->window();
        if (w->width.get() != width || w->height.get() != height) {
            platformWindow->handleResize(width, height);
        }
    }
}
void AndroidPlatformIntegration::registerPlatformWindow(AbstractPlatformWindow *window)
{
    m_windows.insert(window);
}

void AndroidPlatformIntegration::unregisterPlatformWindow(AbstractPlatformWindow *window)
{
    m_windows.erase(window);
}

} // namespace KDGui

// Process the next main command.
static bool startMain = false;
static void handle_cmd(android_app *app, int32_t cmd)
{
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        // The window is being shown, get it ready.
        startMain = true;
        break;
    case APP_CMD_TERM_WINDOW:
        // The window is being hidden or closed, clean it up.
        startMain = false;
        break;
    default:
        __android_log_print(ANDROID_LOG_INFO, "SerenityNativeApplication",
                            "event not handled: %d", cmd);
    }
}

void android_main(struct android_app *app)
{
    using MainType = int (*)(int, const char **);
    // find "main" function
    const auto main = reinterpret_cast<MainType>(dlsym(RTLD_DEFAULT, "main"));
    // Set the callback to process system events
    app->onAppCmd = handle_cmd;
    KDGui::AndroidPlatformIntegration::s_androidApp = app;
    android_poll_source *source;
    do {
        if (ALooper_pollAll(0, nullptr, nullptr, (void **)&source) >= 0) {
            if (source != nullptr)
                source->process(app, source);
        }
    } while (app->destroyRequested == 0 && !startMain);
    static const char *appStr = "SerenityNativeApplication";
    if (startMain && main)
        exit(main(1, &appStr));
    else
        exit(-1);
}