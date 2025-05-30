/*
  This file is part of KDUtils.

  SPDX-FileCopyrightText: 2018 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Paul Lemire <paul.lemire@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include <KDFoundation/core_application.h>
#include <KDFoundation/platform/abstract_platform_event_loop.h>
#include "postman.h"

#if defined(PLATFORM_LINUX)
#include <KDFoundation/platform/linux/linux_platform_integration.h>
#elif defined(PLATFORM_WIN32)
#include <KDFoundation/platform/win32/win32_platform_integration.h>
#elif defined(PLATFORM_MACOS)
#include <KDFoundation/platform/macos/macos_platform_integration.h>
#endif

#include <cassert>

using namespace KDFoundation;

CoreApplication *CoreApplication::ms_application = nullptr;

CoreApplication::CoreApplication(std::unique_ptr<AbstractPlatformIntegration> &&platformIntegration)
    : m_defaultLogger{ KDUtils::Logger::logger("default_log", spdlog::level::info) }
    , m_platformIntegration{ std::move(platformIntegration) }
    , m_logger{ KDUtils::Logger::logger("core_application") }
{
    assert(ms_application == nullptr);
    ms_application = this;

    spdlog::set_default_logger(m_defaultLogger);

    // Helps with debugging setup on remote hosts
    if (const char *display = std::getenv("DISPLAY")) // NOLINT(concurrency-mt-unsafe)
        SPDLOG_LOGGER_INFO(m_logger, "DISPLAY={}", display);

    // Create a default postman object
    m_postman = std::make_unique<Postman>();

    // Create a platform integration
    if (!m_platformIntegration) {
#if defined(PLATFORM_LINUX)
        m_platformIntegration = std::make_unique<LinuxPlatformIntegration>();
#elif defined(PLATFORM_WIN32)
        m_platformIntegration = std::make_unique<Win32PlatformIntegration>();
#elif defined(PLATFORM_MACOS)
        m_platformIntegration = std::make_unique<MacOSPlatformIntegration>();
#endif
    }

    // Ask the platform integration to create us a suitable event loop
    assert(m_platformIntegration);
    m_platformEventLoop = m_platformIntegration->createPlatformEventLoop();
    m_platformIntegration->init();
    m_platformEventLoop->setPostman(m_postman.get());
}

CoreApplication::~CoreApplication()
{
    // Process pending events in case CoreApplication::quit() was not called before
    // and immediately return after processing events (timeout = 0).
    processEvents(0);

    // Destroy the event loop and platform integration before removing the app instance
    m_platformEventLoop = std::unique_ptr<AbstractPlatformEventLoop>();
    m_platformIntegration = std::unique_ptr<AbstractPlatformIntegration>();
    ms_application = nullptr;
}

std::shared_ptr<KDBindings::ConnectionEvaluator> CoreApplication::connectionEvaluator()
{
    return eventLoop()->connectionEvaluator();
}

void CoreApplication::postEvent(EventReceiver *target, std::unique_ptr<Event> &&event)
{
    assert(target != nullptr);
    assert(event->type() != Event::Type::Invalid);
    m_eventQueue.push(target, std::forward<std::unique_ptr<Event>>(event));
    m_platformEventLoop->wakeUp();
}

void CoreApplication::sendEvent(EventReceiver *target, Event *event)
{
    SPDLOG_LOGGER_TRACE(m_logger, "{}()", __FUNCTION__);
    m_postman->deliverEvent(target, event);
}

void CoreApplication::processEvents(int timeout)
{
    // Deliver any events that have already been posted
    for (size_t eventIndex = 0, eventCount = m_eventQueue.size(); eventIndex < eventCount; ++eventIndex) {
        auto postedEvent = m_eventQueue.tryPop();
        if (!postedEvent)
            break;
        const auto target = postedEvent->target();
        const auto ev = postedEvent->wrappedEvent();

        m_postman->deliverEvent(target, ev);
    }

    // Poll/wait for new events
    if (!m_platformEventLoop)
        return;
    m_platformEventLoop->waitForEvents(timeout);
}

int CoreApplication::exec()
{
    if (!m_platformEventLoop)
        return 1;

    while (!m_quitRequested)
        processEvents(-1);
    m_quitRequested = false;

    return 0;
}

void CoreApplication::quit()
{
    postEvent(this, std::make_unique<QuitEvent>());
}

AbstractPlatformIntegration *CoreApplication::platformIntegration()
{
    return m_platformIntegration.get();
}

void CoreApplication::event(EventReceiver *target, Event *event)
{
    if (event->type() == Event::Type::Quit) {
        // After setting the quit flag to true, we must wake up the event loop once more,
        // because processEvents() goes back into waitForEvents() after processing the
        // queued events. Without the wakeUp() call it would wait until some other event
        // woke it up again. Let's kick it ourselves.
        m_quitRequested = true;
        m_platformEventLoop->wakeUp();
        event->setAccepted(true);
    }

    Object::event(target, event);
}

KDUtils::Dir CoreApplication::standardDir(StandardDir type) const
{
    return m_platformIntegration->standardDir(*this, type);
}
