/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AccessibilityRegionContext.h"

#include "AXObjectCache.h"
#include "DocumentInlines.h"
#include "RenderBox.h"
#include "RenderBoxModelObject.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderStyleInlines.h"
#include "RenderText.h"
#include "RenderView.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AccessibilityRegionContext);

AccessibilityRegionContext::~AccessibilityRegionContext()
{
    WeakPtr<AXObjectCache> cache;
    // Release all accumulated RenderText frames to the cache.
    for (auto renderTextToFrame : m_accumulatedRenderTextRects) {
        if (!cache) {
            // Use this RenderText to get to the "canonical" AXObjectCache (attached to the page).
            cache = renderTextToFrame.key.document().axObjectCache();
        }

        if (cache) {
            // Every RenderText should reference the same AXObjectCache (the canonical one).
            ASSERT(cache.get() == renderTextToFrame.key.document().axObjectCache());
            cache->onPaint(renderTextToFrame.key, enclosingIntRect(renderTextToFrame.value));
        }
    }
}

void AccessibilityRegionContext::takeBounds(const RenderInline& renderInline, LayoutRect&& paintRect)
{
    takeBoundsInternal(renderInline, enclosingIntRect(mapRect(WTFMove(paintRect))));
}

void AccessibilityRegionContext::takeBounds(const RenderBox& renderBox, LayoutPoint paintOffset)
{
    if (CheckedPtr renderView = dynamicDowncast<RenderView>(renderBox); renderView) [[unlikely]] {
        takeBounds(*renderView, WTFMove(paintOffset));
        return;
    }
    auto mappedPaintRect = enclosingIntRect(mapRect(LayoutRect(paintOffset, renderBox.size())));
    takeBoundsInternal(renderBox, WTFMove(mappedPaintRect));
}

void AccessibilityRegionContext::takeBounds(const RenderView& renderView, LayoutPoint&& paintOffset)
{
    // RenderView::documentRect applies necessary transforms. Intentionally do not apply the clips stored in this context since accessibility
    // expects the size to be that of the full document (not the size of the current viewport).
    auto documentRect = renderView.documentRect();
    documentRect.moveBy(roundedIntPoint(paintOffset));
    takeBoundsInternal(renderView, WTFMove(documentRect));
}

void AccessibilityRegionContext::takeBounds(const RenderBox& renderBox, FloatRect paintRect)
{
    auto mappedPaintRect = enclosingIntRect(mapRect(paintRect));
    takeBoundsInternal(renderBox, WTFMove(mappedPaintRect));
}

void AccessibilityRegionContext::takeBounds(const RenderLineBreak* renderLineBreak, const LayoutPoint& paintOffset)
{
    if (!renderLineBreak)
        return;
    auto mappedPaintRect = renderLineBreak->linesBoundingBox();
    mappedPaintRect.moveBy(roundedIntPoint(paintOffset));
    // We want <br>s to have non-empty rects so that something is drawn when they are navigated to by-line by
    // ATs like VoiceOver. Make them at least 2px so they roughly match the size of a cursor in either horizontal
    // or vertical writing modes (matching CaretRectComputation::caretWidth).
    if (!mappedPaintRect.width())
        mappedPaintRect.setWidth(2);
    if (!mappedPaintRect.height())
        mappedPaintRect.setHeight(2);
    takeBoundsInternal(*renderLineBreak, WTFMove(mappedPaintRect));
}

void AccessibilityRegionContext::takeBoundsInternal(const RenderBoxModelObject& renderObject, IntRect&& paintRect)
{
    if (RefPtr view = renderObject.document().view())
        paintRect = view->contentsToRootView(paintRect);

    if (auto* cache = renderObject.document().axObjectCache())
        cache->onPaint(renderObject, WTFMove(paintRect));
}

// Note that this function takes the bounds of a textbox that is associated with a RenderText, and not the RenderText itself.
// RenderTexts are not painted atomically. Instead, they are painted as multiple `InlineIterator::TextBox`s, where a textbox might represent
// the text on a single line. This method takes the paint rect of a single textbox and unites it with the other textbox rects painted for |renderText|.
void AccessibilityRegionContext::takeBounds(const RenderText& renderText, FloatRect paintRect, size_t lineIndex)
{
    auto mappedPaintRect = enclosingIntRect(mapRect(WTFMove(paintRect)));
    if (RefPtr view = renderText.document().view())
        mappedPaintRect = view->contentsToRootView(mappedPaintRect);

    auto accumulatedRectIterator = m_accumulatedRenderTextRects.find(renderText);
    if (accumulatedRectIterator == m_accumulatedRenderTextRects.end())
        m_accumulatedRenderTextRects.set(renderText, mappedPaintRect);
    else
        accumulatedRectIterator->value.unite(mappedPaintRect);

    if (CheckedPtr cache = renderText.document().axObjectCache()) {
        // Note that the line-index provided here is relative to the containing-block, which could include lines
        // belonging to other RenderTexts. Concretely, this means the first painted line for |renderText| could
        // have a line index greater than zero. We adjust this later in AXObjectCache::onAccessibilityPaintFinished.
        cache->onPaint(renderText, lineIndex);
    }
}

void AccessibilityRegionContext::onPaint(const ScrollView& scrollView)
{
    auto* frameView = dynamicDowncast<LocalFrameView>(scrollView);
    if (!frameView) {
        // We require a frameview here because ScrollView has no existing mechanism to get an associated document,
        // and we need that to make the frame viewport-relative and to get an AXObjectCache.
        ASSERT_NOT_REACHED_WITH_MESSAGE("Tried to paint a scrollview that was not a frameview, so no frame will be computed");
        return;
    }

    auto relativeFrame = frameView->frameRectShrunkByInset();
    // Only normalize the rect to the root view if this scrollview isn't already associated with the root view (i.e. it has a frame owner).
    if (RefPtr frameOwnerElement = frameView->frame().ownerElement()) {
        if (RefPtr ownerDocumentFrameView = frameOwnerElement->document().view())
            relativeFrame = ownerDocumentFrameView->contentsToRootView(relativeFrame);
    }

    if (auto* cache = frameView->axObjectCache())
        cache->onPaint(*frameView, WTFMove(relativeFrame));
}

} // namespace WebCore
