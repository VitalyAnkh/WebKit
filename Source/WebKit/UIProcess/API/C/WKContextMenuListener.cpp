/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "WKContextMenuListener.h"

#include "APIArray.h"
#include "WKAPICast.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuListenerProxy.h"

using namespace WebKit;

WKTypeID WKContextMenuListenerGetTypeID()
{
#if ENABLE(CONTEXT_MENUS)
    return toAPI(WebContextMenuListenerProxy::APIType);
#else
    return toAPI(API::Object::Type::Null);
#endif
}

void WKContextMenuListenerUseContextMenuItems(WKContextMenuListenerRef listenerRef, WKArrayRef arrayRef)
{
#if ENABLE(CONTEXT_MENUS)
    RefPtr<API::Array> array = toImpl(arrayRef);
    size_t newSize = array ? array->size() : 0;
    Vector<Ref<WebContextMenuItem>> items;
    items.reserveInitialCapacity(newSize);
    for (size_t i = 0; i < newSize; ++i) {
        RefPtr item = array->at<WebContextMenuItem>(i);
        if (!item)
            continue;
        
        items.append(item.releaseNonNull());
    }

    toProtectedImpl(listenerRef)->useContextMenuItems(WTFMove(items));
#else
    UNUSED_PARAM(listenerRef);
    UNUSED_PARAM(arrayRef);
#endif
}
