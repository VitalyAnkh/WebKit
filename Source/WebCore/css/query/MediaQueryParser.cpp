/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaQueryParser.h"

#include "CSSMarkup.h"
#include "CSSTokenizer.h"
#include "CSSValueKeywords.h"
#include "CommonAtomStrings.h"
#include "GenericMediaQuerySerialization.h"
#include "MediaQueryFeatures.h"

namespace WebCore {
namespace MQ {

Vector<const FeatureSchema*> MediaQueryParser::featureSchemas()
{
    return Features::allSchemas();
}

MediaQueryList MediaQueryParser::parse(const String& string, const CSSParserContext& context)
{
    auto tokenizer = CSSTokenizer::tryCreate(string);
    if (!tokenizer)
        return { };

    auto range = tokenizer->tokenRange();
    return parse(range, context);
}

MediaQueryList MediaQueryParser::parse(CSSParserTokenRange range, const CSSParserContext& context)
{
    return consumeMediaQueryList(range, context);
}

std::optional<MediaQuery> MediaQueryParser::parseCondition(CSSParserTokenRange range, const CSSParserContext& context)
{
    range.consumeWhitespace();

    if (range.atEnd())
        return MediaQuery { { }, allAtom() };

    auto condition = consumeCondition(range, context);
    if (!condition)
        return { };

    return MediaQuery { { }, { }, condition };
}

MediaQueryList MediaQueryParser::consumeMediaQueryList(CSSParserTokenRange& range, const CSSParserContext& context)
{
    range.consumeWhitespace();

    if (range.atEnd())
        return { };

    MediaQueryList list;

    while (true) {
        auto begin = range;
        while (!range.atEnd() && range.peek().type() != CommaToken)
            range.consumeComponentValue();

        auto subrange = begin.rangeUntil(range);

        auto consumeMediaQueryOrNotAll = [&] {
            if (auto query = consumeMediaQuery(subrange, context))
                return *query;
            // "A media query that does not match the grammar in the previous section must be replaced by not all during parsing."
            return MediaQuery { Prefix::Not, allAtom() };
        };

        list.append(consumeMediaQueryOrNotAll());

        if (range.atEnd())
            break;
        range.consumeIncludingWhitespace();
    }

    return list;
}

std::optional<MediaQuery> MediaQueryParser::consumeMediaQuery(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <media-condition>

    auto rangeCopy = range;
    if (auto condition = consumeCondition(range, context)) {
        if (!range.atEnd())
            return { };
        return MediaQuery { { }, { }, condition };
    }

    range = rangeCopy;

    // [ not | only ]? <media-type> [ and <media-condition-without-or> ]

    auto consumePrefix = [&]() -> std::optional<Prefix> {
        if (range.peek().type() != IdentToken)
            return { };

        if (range.peek().id() == CSSValueNot) {
            range.consumeIncludingWhitespace();
            return Prefix::Not;
        }
        if (range.peek().id() == CSSValueOnly) {
            // 'only' doesn't do anything. It exists to hide the rule from legacy agents.
            range.consumeIncludingWhitespace();
            return Prefix::Only;
        }
        return { };
    };

    auto consumeMediaType = [&]() -> AtomString {
        if (range.peek().type() != IdentToken)
            return { };

        auto identifier = range.peek().id();
        if (identifier == CSSValueOnly || identifier == CSSValueNot || identifier == CSSValueAnd || identifier == CSSValueOr)
            return { };

        auto mediaType = range.consumeIncludingWhitespace().value().convertToASCIILowercaseAtom();
        if (mediaType == "layer"_s)
            return { };
        
        return mediaType;
    };

    auto prefix = consumePrefix();
    auto mediaType = consumeMediaType();

    if (mediaType.isNull())
        return { };

    if (range.atEnd())
        return MediaQuery { prefix, mediaType, { } };

    if (range.peek().type() != IdentToken || range.peek().id() != CSSValueAnd)
        return { };

    range.consumeIncludingWhitespace();

    auto condition = consumeCondition(range, context);
    if (!condition)
        return { };

    if (!range.atEnd())
        return { };

    if (condition->logicalOperator == LogicalOperator::Or)
        return { };

    return MediaQuery { prefix, mediaType, condition };
}

const FeatureSchema* MediaQueryParser::schemaForFeatureName(const AtomString& name, const CSSParserContext& context, State& state)
{
    auto* schema = GenericMediaQueryParser<MediaQueryParser>::schemaForFeatureName(name, context, state);

    if (schema == &Features::prefersDarkInterface()) {
        if (!context.useSystemAppearance && !isUASheetBehavior(context.mode))
            return nullptr;
    }
    
    return schema;
}

void serialize(StringBuilder& builder, const MediaQueryList& list)
{
    builder.append(interleave(list, serialize, ", "_s));
}

void serialize(StringBuilder& builder, const MediaQuery& query)
{
    if (query.prefix) {
        switch (*query.prefix) {
        case Prefix::Not:
            builder.append("not "_s);
            break;
        case Prefix::Only:
            builder.append("only "_s);
            break;
        }
    }

    if (!query.mediaType.isEmpty() && (!query.condition || query.prefix || query.mediaType != allAtom())) {
        serializeIdentifier(query.mediaType, builder);
        if (query.condition)
            builder.append(" and "_s);
    }

    if (query.condition)
        serialize(builder, *query.condition);
}

}
}
