/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DFGDoesGC.h"

#if ENABLE(DFG_JIT)

#include "DFGClobberize.h"
#include "DFGGraph.h"
#include "DFGNode.h"

namespace JSC { namespace DFG {

bool doesGC(Graph& graph, Node* node)
{
    if (clobbersHeap(graph, node))
        return true;
    
    // Now consider nodes that don't clobber the world but that still may GC. This includes all
    // nodes. By default, we should assume every node can GC and return true. This includes the
    // world-clobbering nodes. We should only return false if we have proven that the node cannot
    // GC. Typical examples of how a node can GC is if the code emitted for the node does any of the
    // following:
    //     1. Allocates any objects.
    //     2. Resolves a rope string, which allocates a string.
    //     3. Produces a string (which allocates the string) except when we can prove that
    //        the string will always be one of the pre-allocated SmallStrings.
    //     4. Triggers a structure transition (which can allocate a new structure)
    //        unless it is a known transition between previously allocated structures
    //        such as between Array types.
    //     5. Calls to a JS function, which can execute arbitrary code including allocating objects.
    //     6. Calls operations that uses DeferGC, because it may GC in its destructor.

    switch (node->op()) {
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
    case LazyJSConstant:
    case Identity:
    case IdentityWithProfile:
    case GetCallee:
    case SetCallee:
    case GetArgumentCountIncludingThis:
    case SetArgumentCountIncludingThis:
    case GetRestLength:
    case GetLocal:
    case SetLocal:
    case MovHint:
    case InitializeEntrypointArguments:
    case ZombieHint:
    case ExitOK:
    case Phantom:
    case Upsilon:
    case Phi:
    case Flush:
    case PhantomLocal:
    case SetArgumentDefinitely:
    case SetArgumentMaybe:
    case ArithBitNot:
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitXor:
    case ArithBitLShift:
    case ArithBitRShift:
    case ArithBitURShift:
    case ValueToInt32:
    case UInt32ToNumber:
    case DoubleAsInt32:
    case ArithAdd:
    case ArithClz32:
    case ArithSub:
    case ArithNegate:
    case ArithMul:
    case ArithIMul:
    case ArithDiv:
    case ArithMod:
    case ArithAbs:
    case ArithMin:
    case ArithMax:
    case ArithPow:
    case ArithSqrt:
    case ArithRandom:
    case ArithRound:
    case ArithFloor:
    case ArithCeil:
    case ArithTrunc:
    case ArithFRound:
    case ArithF16Round:
    case ArithUnary:
    case CheckStructure:
    case CheckStructureOrEmpty:
    case CheckStructureImmediate:
    case GetExecutable:
    case GetButterfly:
    case CheckJSCast:
    case CheckNotJSCast:
    case CheckArray:
    case CheckArrayOrEmpty:
    case CheckDetached:
    case GetScope:
    case GetEvalScope:
    case SkipScope:
    case GetGlobalObject:
    case GetGlobalThis:
    case UnwrapGlobalProxy:
    case GetClosureVar:
    case PutClosureVar:
    case GetInternalField:
    case PutInternalField:
    case GetRegExpObjectLastIndex:
    case SetRegExpObjectLastIndex:
    case RecordRegExpCachedResult:
    case GetGlobalVar:
    case GetGlobalLexicalVariable:
    case PutGlobalVariable:
    case CheckIsConstant:
    case CheckNotEmpty:
    case AssertNotEmpty:
    case CheckIdent:
    case CompareBelow:
    case CompareBelowEq:
    case CompareEqPtr:
    case ProfileControlFlow:
    case OverridesHasInstance:
    case IsEmpty:
    case IsEmptyStorage:
    case TypeOfIsUndefined:
    case TypeOfIsObject:
    case TypeOfIsFunction:
    case IsUndefinedOrNull:
    case IsBoolean:
    case IsNumber:
    case IsBigInt:
    case NumberIsInteger:
    case IsObject:
    case IsCallable:
    case IsConstructor:
    case IsCellWithType:
    case IsTypedArrayView:
    case TypeOf:
    case ToBoolean:
    case LogicalNot:
    case Jump:
    case Branch:
    case EntrySwitch:
    case CountExecution:
    case SuperSamplerBegin:
    case SuperSamplerEnd:
    case CPUIntrinsic:
    case NormalizeMapKey: // HeapBigInt => BigInt32 conversion does not involve GC.
    case MapGet:
    case LoadMapValue:
    case MapIteratorNext:
    case MapIteratorKey:
    case MapIteratorValue:
    case MapStorageOrSentinel:
    case MapIterationNext:
    case MapIterationEntry:
    case MapIterationEntryKey:
    case MapIterationEntryValue:
    case ExtractValueFromWeakMapGet:
    case Unreachable:
    case ExtractOSREntryLocal:
    case ExtractCatchLocal:
    case ClearCatchLocals:
    case LoopHint:
    case StoreBarrier:
    case FencedStoreBarrier:
    case InvalidationPoint:
    case NotifyWrite:
    case AssertInBounds:
    case CheckInBounds:
    case CheckInBoundsInt52:
    case ConstantStoragePointer:
    case Check:
    case CheckVarargs:
    case CheckTypeInfoFlags:
    case HasStructureWithFlags:
    case MultiGetByOffset:
    case MultiDeleteByOffset:
    case ValueRep:
    case DoubleRep:
    case PurifyNaN:
    case Int52Rep:
    case GetGetter:
    case GetSetter:
    case GetArrayLength:
    case GetUndetachedTypeArrayLength:
    case GetTypedArrayLengthAsInt52:
    case GetVectorLength:
    case StringCharCodeAt:
    case StringCodePointAt:
    case GetTypedArrayByteOffset:
    case GetTypedArrayByteOffsetAsInt52:
    case GetPrototypeOf:
    case GetWebAssemblyInstanceExports:
    case PutStructure:
    case GetByOffset:
    case GetGetterSetterByOffset:
    case FiatInt52:
    case BooleanToNumber:
    case CheckBadValue:
    case BottomValue:
    case PhantomNewObject:
    case PhantomNewArrayWithConstantSize:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomNewInternalFieldObject:
    case PhantomCreateActivation:
    case PhantomDirectArguments:
    case PhantomCreateRest:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
    case PhantomSpread:
    case PhantomClonedArguments:
    case PhantomNewRegExp:
    case GetMyArgumentByVal:
    case GetMyArgumentByValOutOfBounds:
    case ForwardVarargs:
    case PutHint:
    case KillStack:
    case GetStack:
    case GetFromArguments:
    case GetArgument:
    case LogShadowChickenPrologue:
    case LogShadowChickenTail:
    case NukeStructureAndSetButterfly:
    case AtomicsAdd:
    case AtomicsAnd:
    case AtomicsCompareExchange:
    case AtomicsExchange:
    case AtomicsLoad:
    case AtomicsOr:
    case AtomicsStore:
    case AtomicsSub:
    case AtomicsXor:
    case AtomicsIsLockFree:
    case MatchStructure:
    case FilterCallLinkStatus:
    case FilterGetByStatus:
    case FilterPutByStatus:
    case FilterInByStatus:
    case FilterDeleteByStatus:
    case FilterCheckPrivateBrandStatus:
    case FilterSetPrivateBrandStatus:
    case DateGetInt32OrNaN:
    case DateGetTime:
    case DataViewGetInt:
    case DataViewGetFloat:
    case DataViewSet:
    case PutByOffset:
    case WeakMapGet:
    case NumberIsNaN:
    case NumberIsFinite:
    case NumberIsSafeInteger:
        return false;

#if ASSERT_ENABLED
    case ArrayPush:
    case ArrayPop:
    case ArraySplice:
    case PushWithScope:
    case CreateActivation:
    case CreateDirectArguments:
    case CreateScopedArguments:
    case CreateClonedArguments:
    case Call:
    case CallDirectEval:
    case CallForwardVarargs:
    case CallObjectConstructor:
    case CallVarargs:
    case CheckTierUpAndOSREnter:
    case CheckTierUpAtReturn:
    case CheckTierUpInLoop:
    case Construct:
    case ConstructForwardVarargs:
    case ConstructVarargs:
    case DataViewGetByteLength:
    case DataViewGetByteLengthAsInt52:
    case DefineDataProperty:
    case DefineAccessorProperty:
    case DeleteById:
    case DeleteByVal:
    case DirectCall:
    case DirectConstruct:
    case DirectTailCall:
    case DirectTailCallInlinedCaller:
    case CallWasm:
    case CallCustomAccessorGetter:
    case CallCustomAccessorSetter:
    case ForceOSRExit:
    case FunctionToString:
    case FunctionBind:
    case GetById:
    case GetByIdDirect:
    case GetByIdDirectFlush:
    case GetByIdFlush:
    case GetByIdMegamorphic:
    case GetByIdWithThis:
    case GetByIdWithThisMegamorphic:
    case GetByValWithThis:
    case GetByValWithThisMegamorphic:
    case GetDynamicVar:
    case HasIndexedProperty:
    case HasOwnProperty:
    case InById:
    case InByIdMegamorphic:
    case InByVal:
    case InByValMegamorphic:
    case HasPrivateName:
    case HasPrivateBrand:
    case InstanceOf:
    case InstanceOfMegamorphic:
    case InstanceOfCustom:
    case VarargsLength:
    case LoadVarargs:
    case NumberToStringWithRadix:
    case NumberToStringWithValidRadixConstant:
    case ProfileType:
    case PutById:
    case PutByIdDirect:
    case PutByIdFlush:
    case PutByIdMegamorphic:
    case PutByIdWithThis:
    case PutByValWithThis:
    case PutDynamicVar:
    case PutGetterById:
    case PutGetterByVal:
    case PutGetterSetterById:
    case PutSetterById:
    case PutSetterByVal:
    case PutPrivateName:
    case PutPrivateNameById:
    case GetPrivateName:
    case GetPrivateNameById:
    case SetPrivateBrand:
    case CheckPrivateBrand:
    case PutStack:
    case PutToArguments:
    case RegExpExec:
    case RegExpExecNonGlobalOrSticky:
    case RegExpMatchFast:
    case RegExpMatchFastGlobal:
    case RegExpTest:
    case RegExpTestInline:
    case RegExpSearch:
    case ResolveScope:
    case ResolveScopeForHoistingFuncDeclInEval:
    case Return:
    case StringAt:
    case StringCharAt:
    case StringLocaleCompare:
    case TailCall:
    case TailCallForwardVarargs:
    case TailCallForwardVarargsInlinedCaller:
    case TailCallInlinedCaller:
    case TailCallVarargs:
    case TailCallVarargsInlinedCaller:
    case Throw:
    case ToNumber:
    case ToNumeric:
    case ToObject:
    case ToPrimitive:
    case ToPropertyKey:
    case ToPropertyKeyOrNumber:
    case ToThis:
    case TryGetById:
    case CreateThis:
    case CreatePromise:
    case CreateGenerator:
    case CreateAsyncGenerator:
    case ObjectAssign:
    case ObjectCreate:
    case ObjectKeys:
    case ObjectGetOwnPropertyNames:
    case ObjectGetOwnPropertySymbols:
    case ObjectToString:
    case ReflectOwnKeys:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
    case Arrayify:
    case ArrayifyToStructure:
    case NewObject:
    case NewGenerator:
    case NewAsyncGenerator:
    case NewArray:
    case NewArrayWithSpread:
    case NewInternalFieldObject:
    case Spread:
    case NewArrayWithSize:
    case NewArrayWithConstantSize:
    case NewArrayWithSpecies:
    case NewArrayWithSizeAndStructure:
    case NewArrayBuffer:
    case NewRegExp:
    case NewRegExpUntyped:
    case NewStringObject:
    case NewMap:
    case NewSet:
    case NewSymbol:
    case MakeRope:
    case MakeAtomString:
    case NewFunction:
    case NewGeneratorFunction:
    case NewAsyncGeneratorFunction:
    case NewAsyncFunction:
    case NewBoundFunction:
    case NewTypedArray:
    case NewTypedArrayBuffer:
    case ThrowStaticError:
    case GetPropertyEnumerator:
    case EnumeratorInByVal:
    case EnumeratorHasOwnProperty:
    case EnumeratorNextUpdatePropertyName:
    case EnumeratorNextUpdateIndexAndMode:
    case MaterializeNewObject:
    case MaterializeNewArrayWithConstantSize:
    case MaterializeNewInternalFieldObject:
    case MaterializeCreateActivation:
    case SetFunctionName:
    case StrCat:
    case StringReplace:
    case StringReplaceAll:
    case StringReplaceRegExp:
    case StringReplaceString:
    case StringSlice:
    case StringSubstring:
    case StringValueOf:
    case CreateRest:
    case ToLowerCase:
    case CallDOMGetter:
    case CallDOM:
    case ArraySlice:
    case ArrayIncludes:
    case ArrayIndexOf:
    case ParseInt: // We might resolve a rope even though we don't clobber anything.
    case SetAdd:
    case MapSet:
    case MapOrSetDelete:
    case MapStorage:
    case ValueBitAnd:
    case ValueBitOr:
    case ValueBitXor:
    case ValueBitLShift:
    case ValueBitRShift:
    case ValueBitURShift:
    case ValueAdd:
    case ValueSub:
    case ValueMul:
    case ValueDiv:
    case ValueMod:
    case ValuePow:
    case ValueBitNot:
    case ValueNegate:
    case DateSetTime:
    case StringIndexOf:
#else // not ASSERT_ENABLED
    // See comment at the top for why the default for all nodes should be to
    // return true.
    default:
#endif // not ASSERT_ENABLED
        return true;

    case ToIntegerOrInfinity:
    case ToLength:
    case GlobalIsFinite:
    case GlobalIsNaN:
        return node->child1().useKind() == UntypedUse;

    case CallNumberConstructor:
        switch (node->child1().useKind()) {
        case BigInt32Use:
            return false;
        default:
            break;
        }
        return true;

    case CallStringConstructor:
    case ToString:
        switch (node->child1().useKind()) {
        case StringObjectUse:
        case StringOrStringObjectUse:
        case StringOrOtherUse:
            return false;
        default:
            break;
        }
        return true;

    case CheckTraps:
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=194323
        ASSERT(Options::usePollingTraps() || graph.m_plan.isUnlinked());
        return true;

    case CompareEq:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
        // FIXME: Add AnyBigIntUse and HeapBigIntUse specific optimizations in DFG / FTL code generation and ensure it does not perform GC.
        // https://bugs.webkit.org/show_bug.cgi?id=210923
        if (node->isBinaryUseKind(Int32Use)
#if USE(JSVALUE64)
            || node->isBinaryUseKind(Int52RepUse)
#endif
            || node->isBinaryUseKind(DoubleRepUse)
            || node->isBinaryUseKind(BigInt32Use)
            || node->isBinaryUseKind(StringIdentUse)
            )
            return false;
        if (node->op() == CompareEq) {
            if (node->isBinaryUseKind(BooleanUse)
                || node->isBinaryUseKind(SymbolUse)
                || node->isBinaryUseKind(ObjectUse)
                || node->isSymmetricBinaryUseKind(ObjectUse, ObjectOrOtherUse))
                return false;
        }
        return true;

    case CompareStrictEq:
        if (node->isBinaryUseKind(BooleanUse)
            || node->isSymmetricBinaryUseKind(BooleanUse, UntypedUse)
            || node->isBinaryUseKind(Int32Use)
#if USE(JSVALUE64)
            || node->isBinaryUseKind(Int52RepUse)
#endif
            || node->isBinaryUseKind(DoubleRepUse)
            || node->isBinaryUseKind(SymbolUse)
            || node->isSymmetricBinaryUseKind(SymbolUse, UntypedUse)
            || node->isBinaryUseKind(StringIdentUse)
            || node->isSymmetricBinaryUseKind(ObjectUse, UntypedUse)
            || node->isBinaryUseKind(ObjectUse)
            || node->isBinaryUseKind(OtherUse)
            || node->isSymmetricBinaryUseKind(OtherUse, UntypedUse)
            || node->isBinaryUseKind(MiscUse)
            || node->isSymmetricBinaryUseKind(MiscUse, UntypedUse)
            || node->isSymmetricBinaryUseKind(StringIdentUse, NotStringVarUse)
            || node->isSymmetricBinaryUseKind(NotDoubleUse, NeitherDoubleNorHeapBigIntNorStringUse))
            return false;
        return true;

    case GetIndexedPropertyStorage:
        return false;

    case GetByVal:
    case GetByValMegamorphic:
    case EnumeratorGetByVal:
        if (node->arrayMode().type() == Array::String)
            return true;
        return false;

    case MultiGetByVal:
        return true;

    case MultiPutByVal:
        return true;

    case ResolveRope:
        return true;

    case ExtractFromTuple:
        return false;

    case PutByValDirect:
    case PutByVal:
    case PutByValAlias:
    case PutByValMegamorphic:
        if (!graph.m_plan.isFTL()) {
            switch (node->arrayMode().modeForPut().type()) {
            case Array::Int8Array:
            case Array::Int16Array:
            case Array::Int32Array:
            case Array::Uint8Array:
            case Array::Uint8ClampedArray:
            case Array::Uint16Array:
            case Array::Uint32Array:
                return true;
            default:
                break;
            }
        }
        return false;

    case EnumeratorPutByVal:
        return true;

    case MapHash:
        switch (node->child1().useKind()) {
        case BooleanUse:
        case Int32Use:
        case SymbolUse:
        case ObjectUse:
#if USE(BIGINT32)
        case BigInt32Use:
#endif
        case HeapBigIntUse:
            return false;
        default:
            // We might resolve a rope.
            return true;
        }
        
    case MultiPutByOffset:
        return node->multiPutByOffsetData().reallocatesStorage();

    case SameValue:
        if (node->isBinaryUseKind(DoubleRepUse))
            return false;
        return true;

    case StringFromCharCode:
        // FIXME: Should we constant fold this case?
        // https://bugs.webkit.org/show_bug.cgi?id=194308
        if (node->child1()->isInt32Constant() && (node->child1()->asUInt32() <= maxSingleCharacterString))
            return false;
        return true;

    case Switch:
        switch (node->switchData()->kind) {
        case SwitchCell:
            ASSERT(graph.m_plan.isFTL());
            [[fallthrough]];
        case SwitchImm:
            return false;
        case SwitchChar:
            return true;
        case SwitchString:
            if (node->child1().useKind() == StringIdentUse)
                return false;
            ASSERT(node->child1().useKind() == StringUse || node->child1().useKind() == UntypedUse);
            return true;
        }
        RELEASE_ASSERT_NOT_REACHED();

    case Inc:
    case Dec:
        switch (node->child1().useKind()) {
        case Int32Use:
        case Int52RepUse:
        case DoubleRepUse:
            return false;
        default:
            return true;
        }

    // WeakSet / WeakMap storages are not GC-managed buffer, thus adding an element does not cause GC,
    // unless we throw an error due to key type. An error can be thrown even though a key is Symbol
    // if the Symbol key is not registered one.
    case WeakSetAdd:
        return node->child2().useKind() != ObjectUse;
    case WeakMapSet:
        return graph.varArgChild(node, 1).useKind() != ObjectUse;

    case LastNodeType:
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
