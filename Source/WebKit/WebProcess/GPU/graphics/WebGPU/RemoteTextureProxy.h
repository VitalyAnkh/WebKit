/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteDeviceProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUIntegralTypes.h>
#include <WebCore/WebGPUTexture.h>
#include <WebCore/WebGPUTextureDimension.h>
#include <WebCore/WebGPUTextureFormat.h>
#include <WebCore/WebGPUTextureViewDescriptor.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteTextureProxy final : public WebCore::WebGPU::Texture {
    WTF_MAKE_TZONE_ALLOCATED(RemoteTextureProxy);
public:
    static Ref<RemoteTextureProxy> create(Ref<RemoteGPUProxy>&& root, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, bool isCanvasBacking = false)
    {
        return adoptRef(*new RemoteTextureProxy(WTFMove(root), convertToBackingContext, identifier, isCanvasBacking));
    }

    virtual ~RemoteTextureProxy();

    RemoteGPUProxy& root() const { return m_root; }
    void undestroy() final;

private:
    friend class DowncastConvertToBackingContext;

    RemoteTextureProxy(Ref<RemoteGPUProxy>&&, ConvertToBackingContext&, WebGPUIdentifier, bool isCanvasBacking);

    RemoteTextureProxy(const RemoteTextureProxy&) = delete;
    RemoteTextureProxy(RemoteTextureProxy&&) = delete;
    RemoteTextureProxy& operator=(const RemoteTextureProxy&) = delete;
    RemoteTextureProxy& operator=(RemoteTextureProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(WTFMove(message), backing());
    }

    RefPtr<WebCore::WebGPU::TextureView> createView(const std::optional<WebCore::WebGPU::TextureViewDescriptor>&) final;

    void destroy() final;
    void setLabelInternal(const String&) final;

    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_root;

    RefPtr<WebCore::WebGPU::TextureView> m_lastCreatedView;
    std::optional<WebCore::WebGPU::TextureViewDescriptor> m_lastCreatedViewDescriptor;
    bool m_isCanvasBacking { false };
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
