/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "_WKInspectorInternal.h"

#import "APIInspectorClient.h"
#import "WKError.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebInspectorUIProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKFrameHandleInternal.h"
#import "_WKInspectorDelegate.h"
#import "_WKInspectorPrivateForTesting.h"
#import "_WKRemoteObjectRegistry.h"
#import <WebCore/FrameIdentifier.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/WTFString.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#import "APIInspectorExtension.h"
#import "WebInspectorUIExtensionControllerProxy.h"
#import "_WKInspectorExtensionInternal.h"
#import <wtf/BlockPtr.h>
#endif

class InspectorClient final : public API::InspectorClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InspectorClient);
public:
    explicit InspectorClient(id <_WKInspectorDelegate> delegate)
        : m_delegate(delegate)
        , m_respondsToInspectorOpenURLExternally([delegate respondsToSelector:@selector(inspector:openURLExternally:)])
        , m_respondsToInspectorFrontendLoaded([delegate respondsToSelector:@selector(inspectorFrontendLoaded:)])
    {
    }

    ~InspectorClient() = default;

private:
    // API::InspectorClient
    void openURLExternally(WebKit::WebInspectorUIProxy& inspector, const WTF::String& url) final
    {
        if (!m_delegate || !m_respondsToInspectorOpenURLExternally)
            return;

        [m_delegate inspector:wrapper(inspector) openURLExternally:adoptNS([[NSURL alloc] initWithString:url.createNSString().get()]).get()];
    }

    void frontendLoaded(WebKit::WebInspectorUIProxy& inspector) final
    {
        if (!m_delegate || !m_respondsToInspectorFrontendLoaded)
            return;

        [m_delegate inspectorFrontendLoaded:wrapper(inspector)];
    }

    WeakObjCPtr<id <_WKInspectorDelegate> > m_delegate;

    bool m_respondsToInspectorOpenURLExternally : 1;
    bool m_respondsToInspectorFrontendLoaded : 1;
};

@implementation _WKInspector

// MARK: _WKInspector methods

- (id <_WKInspectorDelegate>)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id<_WKInspectorDelegate>)delegate
{
    _delegate = delegate;
    _inspector->setInspectorClient(WTF::makeUnique<InspectorClient>(delegate));
}

- (WKWebView *)webView
{
    RefPtr page = _inspector->inspectedPage();
    return page ? page->cocoaView().autorelease() : nil;
}

- (BOOL)isConnected
{
    return _inspector->isConnected();
}

- (BOOL)isVisible
{
    return _inspector->isVisible();
}

- (BOOL)isFront
{
    return _inspector->isFront();
}

- (BOOL)isProfilingPage
{
    return _inspector->isProfilingPage();
}

- (BOOL)isElementSelectionActive
{
    return _inspector->isElementSelectionActive();
}

- (void)connect
{
    _inspector->connect();
}

- (void)show
{
    _inspector->show();
}

- (void)hide
{
    _inspector->hide();
}

- (void)close
{
    _inspector->close();
}

- (void)showConsole
{
    _inspector->showConsole();
}

- (void)showResources
{
    _inspector->showResources();
}

- (void)showMainResourceForFrame:(_WKFrameHandle *)handle
{
    if (!handle)
        return;
    auto frameID = handle->_frameHandle->frameID();
    if (!frameID)
        return;
    _inspector->showMainResourceForFrame(*frameID);
}

- (void)attach
{
    _inspector->attach();
}

- (void)detach
{
    _inspector->detach();
}

- (void)togglePageProfiling
{
    _inspector->togglePageProfiling();
}

- (void)toggleElementSelection
{
    _inspector->toggleElementSelection();
}

- (void)printErrorToConsole:(NSString *)error
{
    // FIXME: This should use a new message source rdar://problem/34658378
    [self.webView evaluateJavaScript:adoptNS([[NSString alloc] initWithFormat:@"console.error(\"%@\");", error]).get() completionHandler:nil];
}

// MARK: _WKInspectorPrivate methods

- (void)_setDiagnosticLoggingDelegate:(id<_WKDiagnosticLoggingDelegate>)delegate
{
    auto inspectorWebView = self.inspectorWebView;
    if (!inspectorWebView)
        return;

    inspectorWebView._diagnosticLoggingDelegate = delegate;
    _inspector->setDiagnosticLoggingAvailable(!!delegate);
}

// MARK: _WKInspectorInternal methods

- (API::Object&)_apiObject
{
    return *_inspector;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKInspector.class, self))
        return;
    
    _inspector->~WebInspectorUIProxy();

    [super dealloc];
}

// MARK: _WKInspectorExtensionHost methods

- (WKWebView *)extensionHostWebView
{
    return self.inspectorWebView;
}

- (void)registerExtensionWithID:(NSString *)extensionID extensionBundleIdentifier:(NSString *)extensionBundleIdentifier displayName:(NSString *)displayName completionHandler:(void(^)(NSError *, _WKInspectorExtension *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // It is an error to call this method prior to creating a frontend (i.e., with -connect or -show).
    if (!_inspector->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get(), nil);
        return;
    }

    _inspector->extensionController()->registerExtension(extensionID, extensionBundleIdentifier, displayName, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<RefPtr<API::InspectorExtension>, Inspector::ExtensionError> result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get(), nil);
            return;
        }

        capturedBlock(nil, wrapper(result.value()));
    });
#else
    completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]).get(), nil);
#endif
}

- (void)unregisterExtension:(_WKInspectorExtension *)extension completionHandler:(void(^)(NSError *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // It is an error to call this method prior to creating a frontend (i.e., with -connect or -show).
    if (!_inspector->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get());
        return;
    }

    _inspector->extensionController()->unregisterExtension(extension.extensionID, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<void, Inspector::ExtensionError> result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
#else
    completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]).get());
#endif
}

- (void)showExtensionTabWithIdentifier:(NSString *)extensionTabIdentifier completionHandler:(void(^)(NSError *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // It is an error to call this method prior to creating a frontend (i.e., with -connect or -show).
    if (!_inspector->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get());
        return;
    }

    _inspector->extensionController()->showExtensionTab(extensionTabIdentifier, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<void, Inspector::ExtensionError>&& result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
#endif
}

- (void)navigateExtensionTabWithIdentifier:(NSString *)extensionTabIdentifier toURL:(NSURL *)url completionHandler:(void(^)(NSError *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // It is an error to call this method prior to creating a frontend (i.e., with -connect or -show).
    if (!_inspector->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get());
        return;
    }

    _inspector->extensionController()->navigateTabForExtension(extensionTabIdentifier, url, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (const std::optional<Inspector::ExtensionError>&& result) mutable {
        if (result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.value()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
#endif
}

@end
