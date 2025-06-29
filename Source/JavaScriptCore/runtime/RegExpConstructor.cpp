/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2009 Torch Mobile, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "RegExpConstructor.h"

#include "GetterSetter.h"
#include "JSCInlines.h"
#include "NumberPrototype.h"
#include "ParseInt.h"
#include "RegExpGlobalDataInlines.h"
#include "RegExpPrototype.h"
#include "YarrFlags.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(regExpConstructorEscape);
static JSC_DECLARE_CUSTOM_GETTER(regExpConstructorInput);
static JSC_DECLARE_CUSTOM_GETTER(regExpConstructorMultiline);
static JSC_DECLARE_CUSTOM_GETTER(regExpConstructorLastMatch);
static JSC_DECLARE_CUSTOM_GETTER(regExpConstructorLastParen);
static JSC_DECLARE_CUSTOM_GETTER(regExpConstructorLeftContext);
static JSC_DECLARE_CUSTOM_GETTER(regExpConstructorRightContext);
static JSC_DECLARE_CUSTOM_GETTER(regExpConstructorDollar);
static JSC_DECLARE_CUSTOM_SETTER(setRegExpConstructorInput);
static JSC_DECLARE_CUSTOM_SETTER(setRegExpConstructorMultiline);

} // namespace JSC

#include "RegExpConstructor.lut.h"

namespace JSC {

const ClassInfo RegExpConstructor::s_info = { "Function"_s, &InternalFunction::s_info, &regExpConstructorTable, nullptr, CREATE_METHOD_TABLE(RegExpConstructor) };

/* Source for RegExpConstructor.lut.h
@begin regExpConstructorTable
    input           regExpConstructorInput          CustomAccessor|DontEnum
    $_              regExpConstructorInput          CustomAccessor|DontEnum
    multiline       regExpConstructorMultiline      CustomAccessor|DontEnum
    $*              regExpConstructorMultiline      CustomAccessor|DontEnum
    lastMatch       regExpConstructorLastMatch      CustomAccessor|ReadOnly|DontEnum
    $&              regExpConstructorLastMatch      CustomAccessor|ReadOnly|DontEnum
    lastParen       regExpConstructorLastParen      CustomAccessor|ReadOnly|DontEnum
    $+              regExpConstructorLastParen      CustomAccessor|ReadOnly|DontEnum
    leftContext     regExpConstructorLeftContext    CustomAccessor|ReadOnly|DontEnum
    $`              regExpConstructorLeftContext    CustomAccessor|ReadOnly|DontEnum
    rightContext    regExpConstructorRightContext   CustomAccessor|ReadOnly|DontEnum
    $'              regExpConstructorRightContext   CustomAccessor|ReadOnly|DontEnum
    $1              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $2              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $3              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $4              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $5              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $6              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $7              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $8              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
    $9              regExpConstructorDollar         CustomAccessor|ReadOnly|DontEnum
@end
*/


static JSC_DECLARE_HOST_FUNCTION(callRegExpConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructWithRegExpConstructor);

RegExpConstructor::RegExpConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callRegExpConstructor, constructWithRegExpConstructor)
{
}

void RegExpConstructor::finishCreation(VM& vm, RegExpPrototype* regExpPrototype)
{
    Base::finishCreation(vm, 2, vm.propertyNames->RegExp.string(), PropertyAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, regExpPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

    JSGlobalObject* globalObject = regExpPrototype->globalObject();

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("escape"_s, regExpConstructorEscape, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);

    GetterSetter* speciesGetterSetter = GetterSetter::create(vm, globalObject, JSFunction::create(vm, globalObject, 0, "get [Symbol.species]"_s, globalFuncSpeciesGetter, ImplementationVisibility::Public, SpeciesGetterIntrinsic), nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, speciesGetterSetter, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

JSC_DEFINE_HOST_FUNCTION(regExpConstructorEscape, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = callFrame->argument(0);
    if (!value.isString()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "RegExp.escape requires a string"_s);

    auto string = asString(value)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    StringBuilder builder(OverflowPolicy::RecordOverflow);
    builder.reserveCapacity(string->length());

    for (unsigned i = 0; i < string->length() && !builder.hasOverflowed();) {
        char32_t codePoint;
        if (string->is8Bit())
            codePoint = string->span8()[i++];
        else {
            auto characters = string->span16();
            U16_NEXT(characters, i, string->length(), codePoint);
        }

        if (builder.isEmpty() && isASCIIAlphanumeric(codePoint)) {
            builder.append('\\', 'x', toStringWithRadix(codePoint, 16));
            continue;
        }

        if (StringView("^$\\.*+?()[]{}|/"_s).contains(codePoint)) {
            builder.append('\\', codePoint);
            continue;
        }

        switch (codePoint) {
        case '\t':
            builder.append('\\', 't');
            continue;
        case '\n':
            builder.append('\\', 'n');
            continue;
        case '\v':
            builder.append('\\', 'v');
            continue;
        case '\f':
            builder.append('\\', 'f');
            continue;
        case '\r':
            builder.append('\\', 'r');
            continue;
        default:
            break;
        }

        if (StringView(",-=<>#&!%:;@~'`\""_s).contains(codePoint) || isStrWhiteSpace(codePoint) || U16_IS_SURROGATE(codePoint)) {
            if (isLatin1(codePoint))
                builder.append('\\', 'x', pad('0', 2, toStringWithRadix(codePoint, 16)));
            else if (U_IS_BMP(codePoint))
                builder.append('\\', 'u', pad('0', 4, toStringWithRadix(codePoint, 16)));
            else {
                builder.append('\\', 'u', pad('0', 4, toStringWithRadix(U16_LEAD(codePoint), 16)));
                builder.append('\\', 'u', pad('0', 4, toStringWithRadix(U16_TRAIL(codePoint), 16)));
            }
            continue;
        }

        if (U_IS_BMP(codePoint))
            builder.append(codePoint);
        else
            builder.append(U16_LEAD(codePoint), U16_TRAIL(codePoint));
    }

    if (builder.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, builder.toString())));
}

JSC_DEFINE_CUSTOM_GETTER(regExpConstructorDollar, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName propertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor())
        return throwVMTypeError(globalObject, scope, "RegExp.$N getters require RegExp constructor as |this|"_s);
    unsigned N = propertyName.uid()->at(1) - '0';
    ASSERT(N >= 1 && N <= 9);
    RELEASE_AND_RETURN(scope, JSValue::encode(globalObject->regExpGlobalData().getBackref(globalObject, N)));
}

JSC_DEFINE_CUSTOM_GETTER(regExpConstructorInput, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor())
        return throwVMTypeError(globalObject, scope, "RegExp.input getter requires RegExp constructor as |this|"_s);
    return JSValue::encode(globalObject->regExpGlobalData().input());
}

JSC_DEFINE_CUSTOM_GETTER(regExpConstructorMultiline, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor())
        return throwVMTypeError(globalObject, scope, "RegExp.multiline getter require RegExp constructor as |this|"_s);
    return JSValue::encode(jsBoolean(globalObject->regExpGlobalData().multiline()));
}

JSC_DEFINE_CUSTOM_GETTER(regExpConstructorLastMatch, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor())
        return throwVMTypeError(globalObject, scope, "RegExp.lastMatch getter require RegExp constructor as |this|"_s);
    RELEASE_AND_RETURN(scope, JSValue::encode(globalObject->regExpGlobalData().getBackref(globalObject, 0)));
}

JSC_DEFINE_CUSTOM_GETTER(regExpConstructorLastParen, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor())
        return throwVMTypeError(globalObject, scope, "RegExp.lastParen getter require RegExp constructor as |this|"_s);
    RELEASE_AND_RETURN(scope, JSValue::encode(globalObject->regExpGlobalData().getLastParen(globalObject)));
}

JSC_DEFINE_CUSTOM_GETTER(regExpConstructorLeftContext, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor())
        return throwVMTypeError(globalObject, scope, "RegExp.leftContext getter require RegExp constructor as |this|"_s);
    RELEASE_AND_RETURN(scope, JSValue::encode(globalObject->regExpGlobalData().getLeftContext(globalObject)));
}

JSC_DEFINE_CUSTOM_GETTER(regExpConstructorRightContext, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor())
        return throwVMTypeError(globalObject, scope, "RegExp.rightContext getter require RegExp constructor as |this|"_s);
    RELEASE_AND_RETURN(scope, JSValue::encode(globalObject->regExpGlobalData().getRightContext(globalObject)));
}

JSC_DEFINE_CUSTOM_SETTER(setRegExpConstructorInput, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor()) {
        throwTypeError(globalObject, scope, "RegExp.input setters require RegExp constructor as |this|"_s);
        return false;
    }
    auto* string = JSValue::decode(value).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    scope.release();
    globalObject->regExpGlobalData().setInput(globalObject, string);
    return true;
}

JSC_DEFINE_CUSTOM_SETTER(setRegExpConstructorMultiline, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue::decode(thisValue) != globalObject->regExpConstructor()) {
        throwTypeError(globalObject, scope, "RegExp.multiline setters require RegExp constructor as |this|"_s);
        return false;
    }
    bool multiline = JSValue::decode(value).toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    scope.release();
    globalObject->regExpGlobalData().setMultiline(multiline);
    return true;
}

static inline bool areLegacyFeaturesEnabled(JSGlobalObject* globalObject, JSValue newTarget)
{
    if (!newTarget)
        return true;
    return newTarget == globalObject->regExpConstructor();
}

inline Structure* getRegExpStructure(JSGlobalObject* globalObject, JSValue newTarget)
{
    if (!newTarget)
        return globalObject->regExpStructure();
    VM& vm = globalObject->vm();
    return JSC_GET_DERIVED_STRUCTURE(vm, regExpStructure, asObject(newTarget), globalObject->regExpConstructor());
}

inline OptionSet<Yarr::Flags> toFlags(JSGlobalObject* globalObject, JSValue flags)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (flags.isUndefined())
        return { };
    
    auto result = Yarr::parseFlags(flags.toWTFString(globalObject));
    RETURN_IF_EXCEPTION(scope, { });
    if (!result) {
        throwSyntaxError(globalObject, scope, "Invalid flags supplied to RegExp constructor."_s);
        return { };
    }

    return result.value();
}

static JSObject* regExpCreate(JSGlobalObject* globalObject, JSValue newTarget, JSValue patternArg, JSValue flagsArg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String pattern = patternArg.isUndefined() ? emptyString() : patternArg.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto flags = toFlags(globalObject, flagsArg);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RegExp* regExp = RegExp::create(vm, pattern, flags);
    if (!regExp->isValid()) [[unlikely]] {
        throwException(globalObject, scope, regExp->errorToThrow(globalObject));
        return nullptr;
    }

    Structure* structure = getRegExpStructure(globalObject, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return RegExpObject::create(vm, structure, regExp, areLegacyFeaturesEnabled(globalObject, newTarget));
}

JSObject* constructRegExp(JSGlobalObject* globalObject, const ArgList& args,  JSObject* callee, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue patternArg = args.at(0);
    JSValue flagsArg = args.at(1);

    bool isPatternRegExp = patternArg.inherits<RegExpObject>();
    bool constructAsRegExp = isRegExp(vm, globalObject, patternArg);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (!newTarget && constructAsRegExp && flagsArg.isUndefined()) {
        JSValue constructor = patternArg.get(globalObject, vm.propertyNames->constructor);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (callee == constructor) {
            // We know that patternArg is a object otherwise constructAsRegExp would be false.
            return patternArg.getObject();
        }
    }

    if (isPatternRegExp) {
        RegExp* regExp = jsCast<RegExpObject*>(patternArg)->regExp();
        Structure* structure = getRegExpStructure(globalObject, newTarget);
        RETURN_IF_EXCEPTION(scope, nullptr);

        if (!flagsArg.isUndefined()) {
            auto flags = toFlags(globalObject, flagsArg);
            RETURN_IF_EXCEPTION(scope, nullptr);

            regExp = RegExp::create(vm, regExp->pattern(), flags);
            if (!regExp->isValid()) [[unlikely]] {
                throwException(globalObject, scope, regExp->errorToThrow(globalObject));
                return nullptr;
            }
        }

        return RegExpObject::create(vm, structure, regExp, areLegacyFeaturesEnabled(globalObject, newTarget));
    }

    if (constructAsRegExp) {
        JSValue pattern = patternArg.get(globalObject, vm.propertyNames->source);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (flagsArg.isUndefined()) {
            flagsArg = patternArg.get(globalObject, vm.propertyNames->flags);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        patternArg = pattern;
    }

    RELEASE_AND_RETURN(scope, regExpCreate(globalObject, newTarget, patternArg, flagsArg));
}

JSC_DEFINE_HOST_FUNCTION(esSpecRegExpCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue patternArg = callFrame->argument(0);
    JSValue flagsArg = callFrame->argument(1);
    return JSValue::encode(regExpCreate(globalObject, JSValue(), patternArg, flagsArg));
}

JSC_DEFINE_HOST_FUNCTION(esSpecIsRegExp, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    return JSValue::encode(jsBoolean(isRegExp(vm, globalObject, callFrame->argument(0))));
}

JSC_DEFINE_HOST_FUNCTION(constructWithRegExpConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructRegExp(globalObject, args, callFrame->jsCallee(), callFrame->newTarget()));
}

JSC_DEFINE_HOST_FUNCTION(callRegExpConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructRegExp(globalObject, args, callFrame->jsCallee()));
}

} // namespace JSC
