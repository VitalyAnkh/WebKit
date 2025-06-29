/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteWebInspectorUIProxy.h"

#include "APIDebuggableInfo.h"
#include "APINavigation.h"
#include "MessageSenderInlines.h"
#include "RemoteWebInspectorUIMessages.h"
#include "RemoteWebInspectorUIProxyMessages.h"
#include "WebInspectorUIProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/NotImplemented.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "WebInspectorUIExtensionControllerProxy.h"
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteWebInspectorUIProxyClient);
WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteWebInspectorUIProxy);

RemoteWebInspectorUIProxy::RemoteWebInspectorUIProxy()
    : m_debuggableInfo(API::DebuggableInfo::create(DebuggableInfoData::empty()))
{
}

RemoteWebInspectorUIProxy::~RemoteWebInspectorUIProxy()
{
    ASSERT(!m_inspectorPage);
}

RefPtr<WebPageProxy> RemoteWebInspectorUIProxy::protectedInspectorPage()
{
    return m_inspectorPage.get();
}

#if ENABLE(INSPECTOR_EXTENSIONS)
RefPtr<WebInspectorUIExtensionControllerProxy> RemoteWebInspectorUIProxy::protectedExtensionController()
{
    return m_extensionController;
}
#endif

void RemoteWebInspectorUIProxy::invalidate()
{
    closeFrontendPageAndWindow();
}

void RemoteWebInspectorUIProxy::setDiagnosticLoggingAvailable(bool available)
{
#if ENABLE(INSPECTOR_TELEMETRY)
    if (RefPtr page = m_inspectorPage.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::RemoteWebInspectorUI::SetDiagnosticLoggingAvailable(available), page->webPageIDInMainFrameProcess());
#else
    UNUSED_PARAM(available);
#endif
}

void RemoteWebInspectorUIProxy::initialize(Ref<API::DebuggableInfo>&& debuggableInfo, const String& backendCommandsURL)
{
    m_debuggableInfo = WTFMove(debuggableInfo);
    m_backendCommandsURL = backendCommandsURL;

    createFrontendPageAndWindow();

    RefPtr inspectorPage = m_inspectorPage.get();
    inspectorPage->protectedLegacyMainFrameProcess()->send(Messages::RemoteWebInspectorUI::Initialize(m_debuggableInfo->debuggableInfoData(), backendCommandsURL), m_inspectorPage->webPageIDInMainFrameProcess());
    inspectorPage->loadRequest(URL { WebInspectorUIProxy::inspectorPageURL() });
}

void RemoteWebInspectorUIProxy::closeFromBackend()
{
    closeFrontendPageAndWindow();
}

void RemoteWebInspectorUIProxy::closeFromCrash()
{
    // Behave as if the frontend just closed, so clients are informed the frontend is gone.
    frontendDidClose();
}

void RemoteWebInspectorUIProxy::show()
{
    bringToFront();
}

void RemoteWebInspectorUIProxy::showConsole()
{
    if (RefPtr page = m_inspectorPage.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::RemoteWebInspectorUI::ShowConsole { }, page->webPageIDInMainFrameProcess());
}

void RemoteWebInspectorUIProxy::showResources()
{
    if (RefPtr page = m_inspectorPage.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::RemoteWebInspectorUI::ShowResources { }, page->webPageIDInMainFrameProcess());
}

void RemoteWebInspectorUIProxy::sendMessageToFrontend(const String& message)
{
    if (RefPtr page = m_inspectorPage.get())
        page->protectedLegacyMainFrameProcess()->send(Messages::RemoteWebInspectorUI::SendMessageToFrontend(message), page->webPageIDInMainFrameProcess());
}

void RemoteWebInspectorUIProxy::frontendLoaded()
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    protectedExtensionController()->inspectorFrontendLoaded();
#endif
}

void RemoteWebInspectorUIProxy::frontendDidClose()
{
    Ref<RemoteWebInspectorUIProxy> protect(*this);

    if (CheckedPtr client = m_client.get())
        client->closeFromFrontend();

    closeFrontendPageAndWindow();
}

void RemoteWebInspectorUIProxy::reopen()
{
    ASSERT(!m_backendCommandsURL.isEmpty());

    closeFrontendPageAndWindow();
    initialize(m_debuggableInfo.copyRef(), m_backendCommandsURL);
}

void RemoteWebInspectorUIProxy::resetState()
{
    platformResetState();
}

void RemoteWebInspectorUIProxy::bringToFront()
{
    platformBringToFront();
}

void RemoteWebInspectorUIProxy::save(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    platformSave(WTFMove(saveDatas), forceSaveAs);
}

void RemoteWebInspectorUIProxy::load(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    platformLoad(path, WTFMove(completionHandler));
}

void RemoteWebInspectorUIProxy::pickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    platformPickColorFromScreen(WTFMove(completionHandler));
}

void RemoteWebInspectorUIProxy::setSheetRect(const FloatRect& rect)
{
    platformSetSheetRect(rect);
}

void RemoteWebInspectorUIProxy::setForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    platformSetForcedAppearance(appearance);
}

void RemoteWebInspectorUIProxy::startWindowDrag()
{
    platformStartWindowDrag();
}

void RemoteWebInspectorUIProxy::openURLExternally(const String& url)
{
    platformOpenURLExternally(url);
}

void RemoteWebInspectorUIProxy::revealFileExternally(const String& path)
{
    platformRevealFileExternally(path);
}

void RemoteWebInspectorUIProxy::showCertificate(const CertificateInfo& certificateInfo)
{
    platformShowCertificate(certificateInfo);
}

void RemoteWebInspectorUIProxy::setInspectorPageDeveloperExtrasEnabled(bool enabled)
{
    RefPtr inspectorPage = m_inspectorPage.get();
    if (!inspectorPage)
        return;

    inspectorPage->protectedPreferences()->setDeveloperExtrasEnabled(enabled);
}

void RemoteWebInspectorUIProxy::sendMessageToBackend(const String& message)
{
    if (CheckedPtr client = m_client.get())
        client->sendMessageToBackend(message);
}

void RemoteWebInspectorUIProxy::createFrontendPageAndWindow()
{
    if (m_inspectorPage)
        return;

    m_inspectorPage = platformCreateFrontendPageAndWindow();
    RefPtr inspectorPage = m_inspectorPage.get();

    trackInspectorPage(inspectorPage.get(), nullptr);

    inspectorPage->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::RemoteWebInspectorUIProxy::messageReceiverName(), inspectorPage->webPageIDInMainFrameProcess(), *this);

#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController = WebInspectorUIExtensionControllerProxy::create(*inspectorPage);
#endif
}

void RemoteWebInspectorUIProxy::closeFrontendPageAndWindow()
{
    RefPtr inspectorPage = m_inspectorPage.get();
    if (!inspectorPage)
        return;

    inspectorPage->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::RemoteWebInspectorUIProxy::messageReceiverName(), inspectorPage->webPageIDInMainFrameProcess());

    untrackInspectorPage(inspectorPage.get());

#if ENABLE(INSPECTOR_EXTENSIONS)
    // This extension controller may be kept alive by the IPC dispatcher beyond the point
    // when m_inspectorPage is cleared below. Notify the controller so it can clean up before then.
    protectedExtensionController()->inspectorFrontendWillClose();
    m_extensionController = nullptr;
#endif

    m_inspectorPage = nullptr;

    platformCloseFrontendPageAndWindow();
}

#if !ENABLE(REMOTE_INSPECTOR) || (!PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN))
WebPageProxy* RemoteWebInspectorUIProxy::platformCreateFrontendPageAndWindow()
{
    notImplemented();
    return nullptr;
}

void RemoteWebInspectorUIProxy::platformResetState() { }
void RemoteWebInspectorUIProxy::platformBringToFront() { }
void RemoteWebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&&, bool /* forceSaveAs */) { }
void RemoteWebInspectorUIProxy::platformLoad(const String&, CompletionHandler<void(const String&)>&& completionHandler) { completionHandler(nullString()); }
void RemoteWebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler) { completionHandler({ }); }
void RemoteWebInspectorUIProxy::platformSetSheetRect(const FloatRect&) { }
void RemoteWebInspectorUIProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance) { }
void RemoteWebInspectorUIProxy::platformStartWindowDrag() { }
void RemoteWebInspectorUIProxy::platformOpenURLExternally(const String&) { }
void RemoteWebInspectorUIProxy::platformRevealFileExternally(const String&) { }
void RemoteWebInspectorUIProxy::platformShowCertificate(const CertificateInfo&) { }
void RemoteWebInspectorUIProxy::platformCloseFrontendPageAndWindow() { }
#endif // !ENABLE(REMOTE_INSPECTOR) || (!PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN))

} // namespace WebKit
