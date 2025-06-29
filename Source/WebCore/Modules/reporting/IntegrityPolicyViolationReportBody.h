/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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

#include "ReportBody.h"
#include "ViolationReportType.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FormData;

class IntegrityPolicyViolationReportBody final : public ReportBody {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(IntegrityPolicyViolationReportBody);
public:
    static Ref<IntegrityPolicyViolationReportBody> create(const String& documentURL, const String& blockedURL, String&& destination, bool reportOnly);

    const String& type() const final;

    const String& documentURL() const
    {
        return m_documentURL;
    }

    const String& blockedURL() const
    {
        return m_blockedURL;
    }

    const String& destination() const
    {
        return m_destination;
    }

    bool reportOnly() const
    {
        return m_reportOnly;
    }

private:
    IntegrityPolicyViolationReportBody(const String& documentURL, const String& blockedURL, String&& destination, bool reportOnly);

    ViolationReportType reportBodyType() const final { return ViolationReportType::IntegrityPolicy; }

    const String m_documentURL;
    const String m_blockedURL;
    const String m_destination;
    bool m_reportOnly;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::IntegrityPolicyViolationReportBody)
    static bool isType(const WebCore::ReportBody& reportBody) { return reportBody.reportBodyType() == WebCore::ViolationReportType::IntegrityPolicy; }
SPECIALIZE_TYPE_TRAITS_END()

