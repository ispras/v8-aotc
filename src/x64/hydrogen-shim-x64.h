#ifndef V8_HYDROGEN_SHIM_64_H_
#define V8_HYDROGEN_SHIM_64_H_

#include "src/hydrogen-shim.h"

// These Lithium instructions are not included in the SHIM_OWNER_LIST below.
// They share the same HConstantShim, but are handled differently from other
// instructions (constant shims are saved during the extra pass).
#define LITHIUM_CONSTANT_INSTRUCTION_LIST(V) \
  V(ConstantD)                               \
  V(ConstantE)                               \
  V(ConstantI)                               \
  V(ConstantS)                               \
  V(ConstantT)

#define HYDROGEN_CONCRETE_SHIM_OWNER_LIST(V)                    \
  V(HStoreContextSlotShim, LStoreContextSlot)                   \
  V(HBitwiseShim, LBitI)                                        \
  V(HBranchShim, LBranch)                                       \
  V(HUnaryControlInstructionShim, LCompareMinusZeroAndBranch)   \
  V(HUnaryControlInstructionShim, LIsStringAndBranch)           \
  V(HUnaryControlInstructionShim, LIsUndetectableAndBranch)     \
  V(HUnaryControlInstructionShim, LHasInstanceTypeAndBranch)    \
  V(HChangeShim, LTaggedToI)                                    \
  V(HUnaryOperationShim, LNumberUntagD)                         \
  V(HUnaryOperationShim, LSmiTag)                               \
  V(HUnaryOperationShim, LCheckNonSmi)                          \
  V(HUnaryMathOperationShim, LMathAbs)                          \
  V(HStoreKeyedShim, LStoreKeyed)                               \
  V(HStoreKeyedGenericShim, LStoreKeyedGeneric)                 \
  V(HStringCharFromCodeShim, LStringCharFromCode)               \
  V(HCompareNumericAndBranchShim, LCompareNumericAndBranch)     \
  V(HStringCompareAndBranchShim, LStringCompareAndBranch)       \
  V(HDeoptimizeShim, LDeoptimize)                               \
  V(HFunctionLiteralShim, LFunctionLiteral)                     \
  V(HLoadContextSlotShim, LLoadContextSlot)                     \
  V(HStackCheckShim, LStackCheck)                               \
  V(HControlInstructionShim, LCmpObjectEqAndBranch)             \
  V(HControlInstructionShim, LCmpHoleAndBranch)                 \
  V(HControlInstructionShim, LIsObjectAndBranch)                \
  V(HControlInstructionShim, LIsSmiAndBranch)                   \
  V(HControlInstructionShim, LHasCachedArrayIndexAndBranch)     \
  V(HControlInstructionShim, LClassOfTestAndBranch)             \
  V(HControlInstructionShim, LIsConstructCallAndBranch)         \
  V(HCompareMapShim, LCmpMapAndBranch)                          \
  V(HBinaryOperationShim, LSubI)                                \
  V(HAddShim, LAddI)                                            \
  V(HMathMinMaxShim, LMathMinMax)                               \
  V(HPowerShim, LPower)                                         \
  V(HCheckValueShim, LCheckValue)                               \
  V(HCheckMapsShim, LCheckMaps)                                 \
  V(HDeclareGlobalsShim, LDeclareGlobals)                       \
  V(HCallShim, LCallNew)                                        \
  V(HGlobalCellShim, LStoreGlobalCell)                          \
  V(HGlobalCellShim, LLoadGlobalCell)                           \
  V(HCallRuntimeShim, LCallRuntime)                             \
  V(HDoubleBitsShim, LDoubleBits)                               \
  V(HCallJSFunctionShim, LCallJSFunction)                       \
  V(HCallFunctionShim, LCallFunction)                           \
  V(HInvokeFunctionShim, LInvokeFunction)                       \
  V(HCallNewArrayShim, LCallNewArray)                           \
  V(HCheckInstanceTypeShim, LCheckInstanceType)                 \
  V(HLoadNamedFieldShim, LLoadNamedField)                       \
  V(HBoundsCheckShim, LBoundsCheck)                             \
  V(HLoadKeyedShim, LLoadKeyed)                                 \
  V(HDoubleToIShim, LDoubleToI)                                 \
  V(HStoreNamedFieldShim, LStoreNamedField)                     \
  V(HStoreNamedGenericShim, LStoreNamedGeneric)                 \
  V(HLoadNamedGenericShim, LLoadNamedGeneric)                   \
  V(HAllocateShim, LAllocate)                                   \
  V(HTransitionElementsKindShim, LTransitionElementsKind)       \
  V(HLoadRootShim, LLoadRoot)                                   \
  V(HStringAddShim, LStringAdd)                                 \
  V(HCompareGenericShim, LCmpT)                                 \
  V(HLoadGlobalGenericShim, LLoadGlobalGeneric)                 \
  V(HForInCacheArrayShim, LForInCacheArray)                     \
  V(HRegExpLiteralShim, LRegExpLiteral)                         \
  V(HArgumentsElementsShim, LArgumentsElements)                 \
  V(HWrapReceiverShim, LWrapReceiver)                           \
  V(HInstanceOfKnownGlobalShim, LInstanceOfKnownGlobal)         \
  V(HTypeofIsAndBranchShim, LTypeofIsAndBranch)                 \

#endif  // V8_HYDROGEN_SHIM_64_H_
