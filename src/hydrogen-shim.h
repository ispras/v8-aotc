#ifndef V8_HYDROGEN_SHIM_H_
#define V8_HYDROGEN_SHIM_H_

#include "src/zone.h"
#include "src/hydrogen.h"
#include "src/hydrogen-instructions.h"

namespace v8 {
namespace internal {

class LChunkSaver;

#define HYDROGEN_ABSTRACT_SHIM_LIST(V)          \
  V(HValueShim)                                 \
  V(HUnaryControlInstructionShim)               \
  V(HUnaryOperationShim)                        \
  V(HControlInstructionShim)                    \
  V(HBinaryOperationShim)                       \
  V(HCallShim)                                  \
  V(HGlobalCellShim)                            \
  V(HKeyedShim)                                 \

#define HYDROGEN_CONCRETE_SHIM_LIST(V)          \
  V(HUnaryMathOperationShim)                    \
  V(HStoreContextSlotShim)                      \
  V(HBitwiseShim)                               \
  V(HStoreKeyedShim)                            \
  V(HStoreKeyedGenericShim)                     \
  V(HStringCharFromCodeShim)                    \
  V(HChangeShim)                                \
  V(HCompareNumericAndBranchShim)               \
  V(HStringCompareAndBranchShim)                \
  V(HDeoptimizeShim)                            \
  V(HFunctionLiteralShim)                       \
  V(HLoadContextSlotShim)                       \
  V(HConstantShim)                              \
  V(HStackCheckShim)                            \
  V(HAddShim)                                   \
  V(HMathMinMaxShim)                            \
  V(HPowerShim)                                 \
  V(HCheckValueShim)                            \
  V(HCheckMapsShim)                             \
  V(HDeclareGlobalsShim)                        \
  V(HCallRuntimeShim)                           \
  V(HDoubleBitsShim)                            \
  V(HCallJSFunctionShim)                        \
  V(HCallFunctionShim)                          \
  V(HInvokeFunctionShim)                        \
  V(HCallNewArrayShim)                          \
  V(HCheckInstanceTypeShim)                     \
  V(HLoadNamedFieldShim)                        \
  V(HBoundsCheckShim)                           \
  V(HLoadKeyedShim)                             \
  V(HDoubleToIShim)                             \
  V(HStoreNamedFieldShim)                       \
  V(HStoreNamedGenericShim)                     \
  V(HLoadNamedGenericShim)                      \
  V(HAllocateShim)                              \
  V(HBranchShim)                                \
  V(HTransitionElementsKindShim)                \
  V(HCompareMapShim)                            \
  V(HLoadRootShim)                              \
  V(HStringAddShim)                             \
  V(HCompareGenericShim)                        \
  V(HLoadGlobalGenericShim)                     \
  V(HForInCacheArrayShim)                       \
  V(HRegExpLiteralShim)                         \
  V(HArgumentsElementsShim)                     \
  V(HWrapReceiverShim)                          \
  V(HInstanceOfKnownGlobalShim)                 \
  V(HTypeofIsAndBranchShim)                     \

#define HYDROGEN_SHIM_LIST(V)                   \
  HYDROGEN_ABSTRACT_SHIM_LIST(V)                \
  HYDROGEN_CONCRETE_SHIM_LIST(V)                \


#define DECLARE_SHIM(type)                              \
  explicit H##type##Shim() {}                           \
  static H##type##Shim* cast(HValueShim* value) {       \
    return reinterpret_cast<H##type##Shim*>(value);     \
  }


class HValueShim : public ZoneObject {
 public:
  explicit HValueShim(/* failure */)
      : position_(HSourcePosition::Unknown()),
        type_(HType::Any()) {}

  explicit HValueShim(HValue* h)
      : id_(h->id()),
        block_id_(h->block() ? h->block()->block_id() : -1),
        position_(h->position()),
        representation_(h->representation()),
        type_(h->type()),
        flags_(h->flags()) {
    DCHECK(block_id_ >= 0 ||
           (h->IsInstruction() && !HInstruction::cast(h)->IsLinked()));
  }

  HValueShim(int id, int block_id, HSourcePosition position,
             Representation representation, HType type, int flags)
      : id_(id),
        block_id_(block_id),
        position_(position),
        representation_(representation),
        type_(type),
        flags_(flags) {}

  int id() const { return id_; }
  int block_id() const { return block_id_; }

  HSourcePosition position() const { return position_; }
  Representation representation() const { return representation_; }
  HType type() const { return type_; }
  int flags() const { return flags_; }

  bool CheckFlag(HValue::Flag f) const { return flags_ & (1 << f); }

  MinusZeroMode GetMinusZeroMode() const {
    return CheckFlag(HValue::kBailoutOnMinusZero)
      ? FAIL_ON_MINUS_ZERO : TREAT_MINUS_ZERO_AS_ZERO;
  }

 private:
  int id_, block_id_;
  HSourcePosition position_;
  Representation representation_;
  HType type_;
  int flags_;
};


class HStoreContextSlotShim : public HValueShim {
 public:
  DECLARE_SHIM(StoreContextSlot)

  explicit HStoreContextSlotShim(HStoreContextSlot* h)
      : HValueShim(h),
        slot_index_(h->slot_index()),
        mode_(h->mode()),
        needs_write_barrier_(h->NeedsWriteBarrier()),
        check_needed_(CheckNeeded(h)) {}

  HStoreContextSlotShim(HValueShim base,
                        int slot_index, HStoreContextSlot::Mode mode,
                        bool needs_write_barrier, SmiCheck check_needed)
      : HValueShim(base),
        slot_index_(slot_index),
        mode_(mode),
        needs_write_barrier_(needs_write_barrier),
        check_needed_(check_needed) {}

  int slot_index() const { return slot_index_; }
  HStoreContextSlot::Mode mode() const { return mode_; }
  bool NeedsWriteBarrier() const { return needs_write_barrier_; }
  SmiCheck CheckNeeded() const { return check_needed_; }

  bool DeoptimizesOnHole() const { return mode_ == HStoreContextSlot::kCheckDeoptimize; }
  bool RequiresHoleCheck() const { return mode_ != HStoreContextSlot::kNoCheck; }

 private:
  SmiCheck CheckNeeded(HStoreContextSlot* hydrogen) {
    return hydrogen->value()->type().IsHeapObject()
      ? OMIT_SMI_CHECK
      : INLINE_SMI_CHECK;
  }

  int slot_index_;
  HStoreContextSlot::Mode mode_;
  bool needs_write_barrier_;
  SmiCheck check_needed_;
};


class HBinaryOperationShim : public HValueShim {
public:
  DECLARE_SHIM(BinaryOperation)

  explicit HBinaryOperationShim(HBinaryOperation* h)
      : HValueShim(h),
        left_representation_(h->left()->representation()),
        right_representation_(h->right()->representation()) {}

  HBinaryOperationShim(HValueShim base,
                       Representation left_representation,
                       Representation right_representation)
      : HValueShim(base),
        left_representation_(left_representation),
        right_representation_(right_representation) {}

  Representation left_representation() const { return left_representation_; }
  Representation right_representation() const { return right_representation_; }

 private:
  Representation left_representation_, right_representation_;
};


class HCompareGenericShim : public HValueShim {
 public:
  DECLARE_SHIM(CompareGeneric)

  explicit HCompareGenericShim(HCompareGeneric* h)
      : HValueShim(h),
        token_(h->token()) {}

  HCompareGenericShim(HValueShim base, Token::Value token)
      : HValueShim(base),
        token_(token) {}

  Token::Value token() const { return token_; }

 private:
  Token::Value token_;
};


class HBitwiseShim : public HBinaryOperationShim {
 public:
  DECLARE_SHIM(Bitwise)

  explicit HBitwiseShim(HBitwise* h)
      : HBinaryOperationShim(h),
        op_(h->op()),
        is_integer32_(h->representation().IsInteger32()) {}

  HBitwiseShim(HBinaryOperationShim base,
               Token::Value op, bool is_integer32)
      : HBinaryOperationShim(base),
        op_(op),
        is_integer32_(is_integer32) {}

  Token::Value op() const { return op_; }
  bool IsInteger32() const { return is_integer32_; }

 private:
  Token::Value op_;
  bool is_integer32_;
};


class HControlInstructionShim : public HValueShim {
 public:
  DECLARE_SHIM(ControlInstruction)

  explicit HControlInstructionShim(HControlInstruction* h)
      : HValueShim(h),
        true_block_id_(h->SuccessorAt(0)->block_id()),
        false_block_id_(h->SuccessorAt(1)->block_id()) {}

  HControlInstructionShim(HValueShim base,
                          int true_block_id, int false_block_id)
      : HValueShim(base),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) {}

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_, false_block_id_;
};


class HCompareMapShim : public HControlInstructionShim {
 public:
  DECLARE_SHIM(CompareMap)

  explicit HCompareMapShim(HCompareMap* h)
      : HControlInstructionShim(h),
        map_(h->map().handle()) {}

  HCompareMapShim(HControlInstructionShim base, Handle<Map> map)
      : HControlInstructionShim(base),
        map_(map) {}

  Handle<Map> map() const { return map_; }

 private:
  Handle<Map> map_;
};


class HUnaryControlInstructionShim : public HControlInstructionShim {
 public:
  DECLARE_SHIM(UnaryControlInstruction)

  explicit HUnaryControlInstructionShim(HUnaryControlInstruction* h)
      : HControlInstructionShim(h),
        value_(h->value()) {}

  HUnaryControlInstructionShim(HControlInstructionShim base, HValueShim value)
      : HControlInstructionShim(base),
        value_(value) {}

  HValueShim* value() { return &value_; }

 private:
  HValueShim value_;
};


class HBranchShim : public HUnaryControlInstructionShim {
 public:
  DECLARE_SHIM(Branch)

  explicit HBranchShim(HBranch* h)
      : HUnaryControlInstructionShim(h),
        expected_input_types_(h->expected_input_types()) {}

  HBranchShim(HUnaryControlInstructionShim base,
              ToBooleanStub::Types expected_input_types)
      : HUnaryControlInstructionShim(base),
        expected_input_types_(expected_input_types) {}

  ToBooleanStub::Types expected_input_types() { return expected_input_types_; }

 private:
  ToBooleanStub::Types expected_input_types_;
};


class HUnaryOperationShim : public HValueShim {
 public:
  DECLARE_SHIM(UnaryOperation)

  explicit HUnaryOperationShim(HUnaryOperation* h)
      : HValueShim(h),
        value_(h->value()) {}

  HUnaryOperationShim(HValueShim base, HValueShim value)
      : HValueShim(base),
        value_(value) {}

  HValueShim* value() { return &value_; }

 private:
  HValueShim value_;
};


class HChangeShim : public HUnaryOperationShim {
 public:
  DECLARE_SHIM(Change)

  explicit HChangeShim(HChange* h)
      : HUnaryOperationShim(h),
        can_convert_undefined_to_nan_(h->can_convert_undefined_to_nan()) {}

  HChangeShim(HUnaryOperationShim base, bool can_convert_undefined_to_nan)
      : HUnaryOperationShim(base),
        can_convert_undefined_to_nan_(can_convert_undefined_to_nan) {}

  bool CanTruncateToInt32() const {
    return CheckFlag(HValue::kTruncatingToInt32);
  }
  bool deoptimize_on_minus_zero() const {
    return CheckFlag(HValue::kBailoutOnMinusZero);
  }
  bool can_convert_undefined_to_nan() const {
    return can_convert_undefined_to_nan_;
  }

 private:
  bool can_convert_undefined_to_nan_;
};


class HUnaryMathOperationShim : public HValueShim {
 public:
  DECLARE_SHIM(UnaryMathOperation)

  explicit HUnaryMathOperationShim(HUnaryMathOperation* h)
      : HValueShim(h),
        value_(h->value()) {}

  HUnaryMathOperationShim(HValueShim base, HValueShim value)
      : HValueShim(base),
        value_(value) {}

  HValueShim* value() { return &value_; }

 private:
  HValueShim value_;
};


class HKeyedShim : public HValueShim {
 public:
  DECLARE_SHIM(Keyed)

  explicit HKeyedShim(HStoreKeyed* h)
      : HValueShim(h),
        key_(h->key()),
        elements_kind_(h->elements_kind()),
        base_offset_(h->base_offset()),
        is_dehoisted_(h->IsDehoisted()) {}

  explicit HKeyedShim(HLoadKeyed* h)
      : HValueShim(h),
        key_(h->key()),
        elements_kind_(h->elements_kind()),
        base_offset_(h->base_offset()),
        is_dehoisted_(h->IsDehoisted()) {}

  HKeyedShim(HValueShim base, HValueShim key, ElementsKind elements_kind,
             uint32_t base_offset, bool is_dehoisted)
      : HValueShim(base),
        key_(key),
        elements_kind_(elements_kind),
        base_offset_(base_offset),
        is_dehoisted_(is_dehoisted) {}

  HValueShim* key() { return &key_; }
  ElementsKind elements_kind() const { return elements_kind_; }
  uint32_t base_offset() const { return base_offset_; }
  bool IsDehoisted() const { return is_dehoisted_; }

  bool is_external() const { return IsExternalArrayElementsKind(elements_kind_); }
  bool is_fixed_typed_array() const { return IsFixedTypedArrayElementsKind(elements_kind_); }

 private:
  HValueShim key_;
  ElementsKind elements_kind_;
  uint32_t base_offset_;
  bool is_dehoisted_;
};


class HStoreKeyedShim : public HKeyedShim {
 public:
  DECLARE_SHIM(StoreKeyed)

  explicit HStoreKeyedShim(HStoreKeyed* h)
      : HKeyedShim(h),
        value_(h->value()),
        store_mode_(h->store_mode()),
        needs_write_barrier_(h->NeedsWriteBarrier()),
        needs_canonicalization_(h->NeedsCanonicalization()),
        pointers_to_here_check_for_value_(h->PointersToHereCheckForValue()) {}

  HStoreKeyedShim(HKeyedShim base,
                  HValueShim value,
                  StoreFieldOrKeyedMode store_mode,
                  bool needs_write_barrier,
                  bool needs_canonicalization,
                  PointersToHereCheck pointers_to_here_check_for_value)
      : HKeyedShim(base),
        value_(value),
        store_mode_(store_mode),
        needs_write_barrier_(needs_write_barrier),
        needs_canonicalization_(needs_canonicalization),
        pointers_to_here_check_for_value_(pointers_to_here_check_for_value) {}

  HValueShim* value() { return &value_; }
  StoreFieldOrKeyedMode store_mode() const { return store_mode_; }
  bool NeedsWriteBarrier() const { return needs_write_barrier_; }
  bool NeedsCanonicalization() const { return needs_canonicalization_; }
  PointersToHereCheck PointersToHereCheckForValue() const { return pointers_to_here_check_for_value_; }

 private:
  HValueShim value_;
  StoreFieldOrKeyedMode store_mode_;
  bool needs_write_barrier_, needs_canonicalization_;
  PointersToHereCheck pointers_to_here_check_for_value_;
};


class HLoadKeyedShim : public HKeyedShim {
 public:
  DECLARE_SHIM(LoadKeyed)

  explicit HLoadKeyedShim(HLoadKeyed* h)
      : HKeyedShim(h),
        requires_hole_check_(h->RequiresHoleCheck()) {}

  HLoadKeyedShim(HKeyedShim base, bool requires_hole_check)
      : HKeyedShim(base),
        requires_hole_check_(requires_hole_check) {}

  bool RequiresHoleCheck() const { return requires_hole_check_; }

 private:
  bool requires_hole_check_;
};


class HStoreKeyedGenericShim : public HValueShim {
 public:
  DECLARE_SHIM(StoreKeyedGeneric)

  explicit HStoreKeyedGenericShim(HStoreKeyedGeneric* h)
      : HValueShim(h),
        strict_mode_(h->strict_mode()) {}

  HStoreKeyedGenericShim(HValueShim base, StrictMode strict_mode)
      : HValueShim(base),
        strict_mode_(strict_mode) {}

  StrictMode strict_mode() const { return strict_mode_; }

 private:
  StrictMode strict_mode_;
};


class HStringCharFromCodeShim : public HValueShim {
 public:
  DECLARE_SHIM(StringCharFromCode)

  explicit HStringCharFromCodeShim(HStringCharFromCode* h)
      : HValueShim(h),
        value_(h->value()) {}

  HStringCharFromCodeShim(HValueShim base, HValueShim value)
      : HValueShim(base),
        value_(value) {}

  HValueShim* value() { return &value_; }

 private:
  HValueShim value_;
};


class HCompareNumericAndBranchShim : public HControlInstructionShim {
 public:
  DECLARE_SHIM(CompareNumericAndBranch)

  explicit HCompareNumericAndBranchShim(HCompareNumericAndBranch* h)
      : HControlInstructionShim(h),
        token_(h->token()),
        is_double_(h->representation().IsDouble()),
        is_unsigned_(is_unsigned(h)) {}

  HCompareNumericAndBranchShim(HControlInstructionShim base,
                               Token::Value token,
                               bool is_double, bool is_unsigned)
      : HControlInstructionShim(base),
        token_(token),
        is_double_(is_double),
        is_unsigned_(is_unsigned) {}

  Token::Value token() const { return token_; }
  bool is_double() const { return is_double_; }
  bool is_unsigned() const { return is_unsigned_; }

 private:
  bool is_unsigned(HCompareNumericAndBranch* h) {
    return is_double_
      || h->left()->CheckFlag(HInstruction::kUint32)
      || h->right()->CheckFlag(HInstruction::kUint32);
  }

  Token::Value token_;
  bool is_double_, is_unsigned_;
};


class HStringCompareAndBranchShim : public HControlInstructionShim {
 public:
  DECLARE_SHIM(StringCompareAndBranch)

  explicit HStringCompareAndBranchShim(HStringCompareAndBranch* h)
      : HControlInstructionShim(h),
        token_(h->token()) {}

  HStringCompareAndBranchShim(HControlInstructionShim base, Token::Value token)
      : HControlInstructionShim(base),
        token_(token) {}

  Token::Value token() const { return token_; }

 private:
  Token::Value token_;
};


class HDeoptimizeShim : public HValueShim {
 public:
  DECLARE_SHIM(Deoptimize)

  explicit HDeoptimizeShim(HDeoptimize* h)
      : HValueShim(h),
        reason_(h->reason()),
        type_(h->type()) {}

  HDeoptimizeShim(HValueShim base,
                  const char* reason, Deoptimizer::BailoutType type)
      : HValueShim(base),
        reason_(reason),
        type_(type) {}

  const char* reason() const { return reason_; }
  Deoptimizer::BailoutType type() const { return type_; }

 private:
  const char* reason_;
  Deoptimizer::BailoutType type_;
};


class HFunctionLiteralShim : public HValueShim {
 public:
  DECLARE_SHIM(FunctionLiteral)

  explicit HFunctionLiteralShim(HFunctionLiteral* h)
      : HValueShim(h),
        shared_info_(h->shared_info()),
        pretenure_(h->pretenure()),
        has_no_literals_(h->has_no_literals()),
        kind_(h->kind()),
        strict_mode_(h->strict_mode()) {}

  HFunctionLiteralShim(HValueShim base,
                       Handle<SharedFunctionInfo> shared_info, bool pretenure,
                       bool has_no_literals, FunctionKind kind,
                       StrictMode strict_mode)
    : HValueShim(base),
      shared_info_(shared_info),
      pretenure_(pretenure),
      has_no_literals_(has_no_literals),
      kind_(kind),
      strict_mode_(strict_mode) {}

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  bool pretenure() const { return pretenure_; }
  bool has_no_literals() const { return has_no_literals_; }
  FunctionKind kind() const { return kind_; }
  StrictMode strict_mode() const { return strict_mode_; }

 private:
  Handle<SharedFunctionInfo> shared_info_;
  bool pretenure_, has_no_literals_;
  FunctionKind kind_;
  StrictMode strict_mode_;
};


class HLoadContextSlotShim : public HValueShim {
 public:
  DECLARE_SHIM(LoadContextSlot)

  explicit HLoadContextSlotShim(HLoadContextSlot* h)
      : HValueShim(h),
        slot_index_(h->slot_index()),
        mode_(h->mode()) {}

  HLoadContextSlotShim(HValueShim base,
                       int slot_index, HLoadContextSlot::Mode mode)
      : HValueShim(base),
        slot_index_(slot_index),
        mode_(mode) {}

  int slot_index() const { return slot_index_; }
  HLoadContextSlot::Mode mode() const { return mode_; }

  bool DeoptimizesOnHole() const { return mode_ == HLoadContextSlot::kCheckDeoptimize; }
  bool RequiresHoleCheck() const { return mode_ != HLoadContextSlot::kNoCheck; }

 private:
  int slot_index_;
  HLoadContextSlot::Mode mode_;
};


class HConstantShim : public HValueShim {
 public:
  DECLARE_SHIM(Constant)

  explicit HConstantShim(HConstant* h)
      : HValueShim(h),
        hydrogen_(h),
        object_(h->GetUnique().handle()),
        int32_value_(h->int32_value_),
        double_value_(h->double_value_),
        external_reference_value_(h->external_reference_value_),
        bit_field_(h->bit_field_) {
    DCHECK(object_.is_null() || !object_->IsContext() ||
           (Name().is_null() && !ContextOwner().is_null()));
  }

  HConstantShim(HValueShim base, Handle<Object> object,
                int32_t int32_value, double double_value, int32_t bit_field)
      : HValueShim(base),
        hydrogen_(nullptr),
        object_(object),
        int32_value_(int32_value),
        double_value_(double_value),
        bit_field_(bit_field) {}

  HConstantShim(HValueShim base, int32_t int32_value, int32_t bit_field)
      : HValueShim(base),
        hydrogen_(nullptr),
        int32_value_(int32_value),
        double_value_(FastI2D(int32_value)),
        bit_field_(bit_field) {}

  HConstantShim(HValueShim base, double double_value, int32_t bit_field)
      : HValueShim(base),
        hydrogen_(nullptr),
        int32_value_(DoubleToInt32(double_value)),
        double_value_(double_value),
        bit_field_(bit_field) {}

  HConstantShim(HValueShim base,
                ExternalReference external_reference_value, int32_t bit_field)
      : HValueShim(base),
        hydrogen_(nullptr),
        external_reference_value_(external_reference_value),
        bit_field_(bit_field) {}

  Unique<Object> GetUnique() const {
    return Unique<Object>::CreateUninitialized(object_);
  }

  Handle<Object> handle(Isolate* isolate) {
    if (object_.is_null()) {
      object_ = isolate->factory()->NewNumber(double_value_, TENURED);
    }
    DCHECK(HasInteger32Value() || !object_->IsSmi());
    return object_;
  }

  bool HasInteger32Value() const {
    return HConstant::HasInt32ValueField::decode(bit_field_);
  }

  int32_t Integer32Value() const {
    DCHECK(HasInteger32Value());
    return int32_value_;
  }

  bool HasDoubleValue() const {
    return HConstant::HasDoubleValueField::decode(bit_field_);
  }

  double DoubleValue() const {
    DCHECK(HasDoubleValue());
    return double_value_;
  }

  bool HasExternalReferenceValue() const {
    return HConstant::HasExternalReferenceValueField::decode(bit_field_);
  }

  ExternalReference ExternalReferenceValue() const {
    DCHECK(HasExternalReferenceValue());
    return external_reference_value_;
  }

 private:
  bool HasName() const { return !hydrogen_->Name().is_null(); }
  Handle<String> Name() const { return hydrogen_->Name(); }
  bool IsBuiltin() const { return hydrogen_->IsBuiltin(); }
  Handle<JSFunction> ContextOwner() const {
    return hydrogen_->ContextOwner();
  }
  const HConstant::CodeRelocationData* CodeRelocation() const {
    return hydrogen_->CodeRelocation();
  }

  int32_t int32_value() const { return int32_value_; }
  double double_value() const { return double_value_; }
  int32_t bit_field() const { return bit_field_; }

  // For save mode only.
  HConstant* hydrogen_;

  Handle<Object> object_;
  int32_t int32_value_;
  double double_value_;
  ExternalReference external_reference_value_;
  int32_t bit_field_;

  friend class LChunkSaver;
};


class HStackCheckShim : public HValueShim {
 public:
  DECLARE_SHIM(StackCheck)

  explicit HStackCheckShim(HStackCheck* h)
      : HValueShim(h),
        type_(h->type_) {}

  HStackCheckShim(HValueShim base, HStackCheck::Type type)
      : HValueShim(base),
        type_(type) {}

  HStackCheck::Type type() const { return type_; }

  bool is_function_entry() { return type_ == HStackCheck::kFunctionEntry; }
  bool is_backwards_branch() { return type_ == HStackCheck::kBackwardsBranch; }

 private:
  HStackCheck::Type type_;
};


class HAddShim : public HBinaryOperationShim {
 public:
  DECLARE_SHIM(Add)

  explicit HAddShim(HAdd* h)
      : HBinaryOperationShim(h),
        use_lea_(use_lea(h)) {}

  HAddShim(HBinaryOperationShim base, bool use_lea)
      : HBinaryOperationShim(base),
        use_lea_(use_lea) {}

  bool use_lea() const { return use_lea_; }

 private:
  bool use_lea(HAdd* add) {
    return !add->CheckFlag(HValue::kCanOverflow)
      && add->BetterLeftOperand()->UseCount() > 1;
  }

  bool use_lea_;
};


class HMathMinMaxShim : public HBinaryOperationShim {
 public:
  DECLARE_SHIM(MathMinMax)

  explicit HMathMinMaxShim(HMathMinMax* h)
      : HBinaryOperationShim(h),
        operation_(h->operation()) {}

  HMathMinMaxShim(HBinaryOperationShim base,
                  HMathMinMax::Operation operation)
      : HBinaryOperationShim(base),
        operation_(operation) {}

  HMathMinMax::Operation operation() const { return operation_; }

 private:
  HMathMinMax::Operation operation_;
};


class HPowerShim : public HValueShim {
public:
  DECLARE_SHIM(Power)

  explicit HPowerShim(HPower* h)
      : HValueShim(h),
        left_representation_(h->left()->representation()),
        right_representation_(h->right()->representation()) {}

  HPowerShim(HValueShim base,
             Representation left_representation,
             Representation right_representation)
      : HValueShim(base),
        left_representation_(left_representation),
        right_representation_(right_representation) {}

  Representation left_representation() const { return left_representation_; }
  Representation right_representation() const { return right_representation_; }

 private:
  Representation left_representation_, right_representation_;
};


class HCheckValueShim : public HValueShim {
 public:
  DECLARE_SHIM(CheckValue)

  explicit HCheckValueShim(HCheckValue* h)
      : HValueShim(h),
        object_(h->object().handle()) {}

  HCheckValueShim(HValueShim base, Handle<Object> object)
      : HValueShim(base),
        object_(object) {
    DCHECK(object->IsHeapObject());
  }

  Handle<Object> object() {
    return object_;
  }

 private:
  Handle<Object> object_;
};


class HCheckMapsShim : public HValueShim {
 public:
  DECLARE_SHIM(CheckMaps)

  explicit HCheckMapsShim(HCheckMaps* h)
      : HValueShim(h),
        bit_field_(h->bit_field_),
        maps_(h->maps()) {}

  HCheckMapsShim(HValueShim base, uint32_t bit_field,
                 const UniqueSet<Map>* maps)
      : HValueShim(base),
        bit_field_(bit_field),
        maps_(maps) {}

  const UniqueSet<Map>* maps() const { return maps_; }

  bool IsStabilityCheck() const {
    return HCheckMaps::IsStabilityCheckField::decode(bit_field_);
  }

  bool HasMigrationTarget() const {
    return HCheckMaps::HasMigrationTargetField::decode(bit_field_);
  }

 private:
  uint32_t bit_field() const { return bit_field_; }

  uint32_t bit_field_;
  const UniqueSet<Map>* maps_;

  friend class LChunkSaver;
};


class HDeclareGlobalsShim : public HValueShim {
 public:
  DECLARE_SHIM(DeclareGlobals)

  explicit HDeclareGlobalsShim(HDeclareGlobals* h)
      : HValueShim(h),
        pairs_(h->pairs()),
        flags_(h->flags()) {}

  HDeclareGlobalsShim(HValueShim base, Handle<FixedArray> pairs, int flags)
      : HValueShim(base),
        pairs_(pairs),
        flags_(flags) {}

  Handle<FixedArray> pairs() const { return pairs_; }
  int flags() const { return flags_; }

 private:
  Handle<FixedArray> pairs_;
  int flags_;
};


class HCallShim : public HValueShim {
 public:
  DECLARE_SHIM(Call)

  template<int V>
  explicit HCallShim(HCall<V>* h)
      : HValueShim(h),
        argument_count_(h->argument_count()) {}

  HCallShim(HValueShim base, int argument_count)
      : HValueShim(base),
        argument_count_(argument_count) {}

  int argument_count() const { return argument_count_; }

 private:
  int argument_count_;
};


class HCallRuntimeShim : public HCallShim {
 public:
  DECLARE_SHIM(CallRuntime)

  explicit HCallRuntimeShim(HCallRuntime* h)
      : HCallShim(h),
        function_(h->function()),
        save_doubles_(h->save_doubles()) {}

  HCallRuntimeShim(HCallShim base, const Runtime::Function* function,
                   SaveFPRegsMode save_doubles)
      : HCallShim(base),
        function_(function),
        save_doubles_(save_doubles) {}

  const Runtime::Function* function() const { return function_; }
  SaveFPRegsMode save_doubles() const { return save_doubles_; }

 private:
  const Runtime::Function* function_;
  SaveFPRegsMode save_doubles_;
};


class HCallJSFunctionShim : public HCallShim {
 public:
  DECLARE_SHIM(CallJSFunction)

  explicit HCallJSFunctionShim(HCallJSFunction* h)
      : HCallShim(h),
        pass_argument_count_(h->pass_argument_count()) {
    if (h->function()->IsConstant()) {
      hydrogen_constant_ = HConstant::cast(h->function());
    }
  }

  HCallJSFunctionShim(HCallShim base, MaybeHandle<JSFunction> function,
                      bool pass_argument_count)
      : HCallShim(base),
        hydrogen_constant_(nullptr),
        function_(function),
        pass_argument_count_(pass_argument_count) {}

  bool pass_argument_count() const { return pass_argument_count_; }

  MaybeHandle<JSFunction> function(Isolate* isolate) const {
    if (hydrogen_constant_) {
      return Handle<JSFunction>::cast(hydrogen_constant_->handle(isolate));
    }
    return function_;
  }

  bool has_function() const {
    return hydrogen_constant_ || !function_.is_null();
  }

 private:
  HConstant* hydrogen_constant_;

  MaybeHandle<JSFunction> function_;
  bool pass_argument_count_;
};


class HCallFunctionShim : public HCallShim {
 public:
  DECLARE_SHIM(CallFunction)

  explicit HCallFunctionShim(HCallFunction* h)
      : HCallShim(h),
        function_flags_(h->function_flags()) {}

  HCallFunctionShim(HCallShim base, CallFunctionFlags function_flags)
      : HCallShim(base),
        function_flags_(function_flags) {}

  CallFunctionFlags function_flags() const { return function_flags_; }

 private:
  CallFunctionFlags function_flags_;
};


class HInvokeFunctionShim : public HCallShim {
 public:
  DECLARE_SHIM(InvokeFunction)

  explicit HInvokeFunctionShim(HInvokeFunction* h)
      : HCallShim(h),
        known_function_(h->known_function()),
        formal_parameter_count_(h->formal_parameter_count()) {}

  HInvokeFunctionShim(HCallShim base)
      : HCallShim(base),
        formal_parameter_count_(-1) {}

  Handle<JSFunction> known_function() const { return known_function_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

 private:
  Handle<JSFunction> known_function_;
  int formal_parameter_count_;
};


class HCallNewArrayShim : public HCallShim {
 public:
  DECLARE_SHIM(CallNewArray)

  explicit HCallNewArrayShim(HCallNewArray* h)
      : HCallShim(h),
        elements_kind_(h->elements_kind()) {}

  HCallNewArrayShim(HCallShim base, ElementsKind elements_kind)
      : HCallShim(base),
        elements_kind_(elements_kind) {}

  ElementsKind elements_kind() const { return elements_kind_; }

 private:
  ElementsKind elements_kind_;
};


class HGlobalCellShim : public HValueShim {
 public:
  DECLARE_SHIM(GlobalCell)

  explicit HGlobalCellShim(HStoreGlobalCell* h)
      : HValueShim(h),
        name_(h->name()),
        cell_(h->cell()),
        requires_hole_check_(h->RequiresHoleCheck()) {}

  explicit HGlobalCellShim(HLoadGlobalCell* h)
      : HValueShim(h),
        name_(h->name()),
        cell_(h->cell()),
        requires_hole_check_(h->RequiresHoleCheck()) {}

  HGlobalCellShim(HValueShim base, Handle<String> name,
                  Handle<PropertyCell> cell, bool requires_hole_check)
      : HValueShim(base),
        name_(name),
        cell_(Unique<PropertyCell>::CreateUninitialized(cell)),
        requires_hole_check_(requires_hole_check) {}

  Unique<PropertyCell> cell() const { return cell_; }
  Handle<String> name() const { return name_; }
  bool RequiresHoleCheck() const { return requires_hole_check_; }

 private:
  Handle<String> name_;
  Unique<PropertyCell> cell_;
  bool requires_hole_check_;
};


class HDoubleBitsShim : public HValueShim {
 public:
  DECLARE_SHIM(DoubleBits)

  typedef HDoubleBits::Bits Bits;

  explicit HDoubleBitsShim(HDoubleBits* h)
      : HValueShim(h),
        bits_(h->bits()) {}

  HDoubleBitsShim(HValueShim base, Bits bits)
      : HValueShim(base),
        bits_(bits) {}

  Bits bits() const { return bits_; }

 private:
  Bits bits_;
};


class HCheckInstanceTypeShim : public HValueShim {
 public:
  DECLARE_SHIM(CheckInstanceType)

  explicit HCheckInstanceTypeShim(HCheckInstanceType* h)
      : HValueShim(h),
        hydrogen_(h),
        is_interval_check_(h->is_interval_check()) {}

  HCheckInstanceTypeShim(HValueShim base,
                         InstanceType first, InstanceType last)
      : HValueShim(base),
        hydrogen_(nullptr),
        is_interval_check_(true) {
    u.interval_check.first = first;
    u.interval_check.last = last;
  }

  HCheckInstanceTypeShim(HValueShim base,
                         uint8_t mask, uint8_t tag)
      : HValueShim(base),
        hydrogen_(nullptr),
        is_interval_check_(false) {
    u.mask_and_tag_check.mask = mask;
    u.mask_and_tag_check.tag = tag;
  }

  bool is_interval_check() const { return is_interval_check_; }

  void GetCheckInterval(InstanceType* first, InstanceType* last) {
    DCHECK(is_interval_check());

    if (hydrogen_) {
      hydrogen_->GetCheckInterval(&u.interval_check.first,
                                  &u.interval_check.last);
      hydrogen_ = nullptr;
    }

    *first = u.interval_check.first;
    *last = u.interval_check.last;
  }

  void GetCheckMaskAndTag(uint8_t* mask, uint8_t* tag) {
    DCHECK(!is_interval_check());

    if (hydrogen_) {
      hydrogen_->GetCheckMaskAndTag(&u.mask_and_tag_check.mask,
                                    &u.mask_and_tag_check.tag);
      hydrogen_ = nullptr;
    }

    *mask = u.mask_and_tag_check.mask;
    *tag = u.mask_and_tag_check.tag;
  }

 private:
  HCheckInstanceType* hydrogen_;

  bool is_interval_check_;

  union {
    struct {
      InstanceType first, last;
    } interval_check;
    struct {
      uint8_t mask, tag;
    } mask_and_tag_check;
  } u;
};


class HLoadNamedFieldShim : public HValueShim {
 public:
  DECLARE_SHIM(LoadNamedField)

  explicit HLoadNamedFieldShim(HLoadNamedField* h)
      : HValueShim(h),
        access_(h->access()) {}

  HLoadNamedFieldShim(HValueShim base, uint32_t value)
      : HValueShim(base) {
    access_.value_ = value;
  }

  HObjectAccess access() const { return access_; }

 private:
  uint32_t value() const { return access_.value_; }

  HObjectAccess access_;

  friend class LChunkSaver;
};


class HBoundsCheckShim : public HValueShim {
 public:
  DECLARE_SHIM(BoundsCheck)

  explicit HBoundsCheckShim(HBoundsCheck* h)
      : HValueShim(h),
        skip_check_(h->skip_check()),
        allow_equality_(h->allow_equality()),
        index_(h->index()),
        length_(h->length()) {}

  HBoundsCheckShim(HValueShim base, bool skip_check, bool allow_equality,
                   HValueShim index, HValueShim length)
      : HValueShim(base),
        skip_check_(skip_check),
        allow_equality_(allow_equality),
        index_(index),
        length_(length) {}

  bool skip_check() const { return skip_check_; }
  bool allow_equality() const { return allow_equality_; }
  HValueShim* index() { return &index_; }
  HValueShim* length() { return &length_; }

 private:
  bool skip_check_, allow_equality_;
  HValueShim index_, length_;
};


class HDoubleToIShim : public HValueShim {
 public:
  DECLARE_SHIM(DoubleToI)

  explicit HDoubleToIShim(HInstruction* h)
      : HValueShim(h),
        can_truncate_to_int32_(h->CanTruncateToInt32()) {}

  HDoubleToIShim(HValueShim base, bool can_truncate_to_int32)
      : HValueShim(base),
        can_truncate_to_int32_(can_truncate_to_int32) {}

  bool CanTruncateToInt32() const { return can_truncate_to_int32_; }

 private:
  bool can_truncate_to_int32_;
};


class HStoreNamedFieldShim : public HLoadNamedFieldShim {
 public:
  DECLARE_SHIM(StoreNamedField)

  explicit HStoreNamedFieldShim(HStoreNamedField* h)
      : HLoadNamedFieldShim(HValueShim(h), h->access().value_),
        needs_write_barrier_(h->NeedsWriteBarrier()),
        needs_write_barrier_for_map_(h->NeedsWriteBarrierForMap()),
        transition_map_(h->transition_map()),
        value_(h->value()),
        pointers_to_here_check_for_value_(h->PointersToHereCheckForValue()),
        bit_field_(h->bit_field_) {}

  HStoreNamedFieldShim(HLoadNamedFieldShim base,
                       bool needs_write_barrier,
                       bool needs_write_barrier_for_map,
                       Handle<Map> transition_map,
                       HValueShim value,
                       PointersToHereCheck pointers_to_here_check_for_value,
                       uint32_t bit_field)
      : HLoadNamedFieldShim(base),
        needs_write_barrier_(needs_write_barrier),
        needs_write_barrier_for_map_(needs_write_barrier_for_map),
        transition_map_(transition_map),
        value_(value),
        pointers_to_here_check_for_value_(pointers_to_here_check_for_value),
        bit_field_(bit_field) {}

  bool NeedsWriteBarrier() const { return needs_write_barrier_; }
  bool NeedsWriteBarrierForMap() const { return needs_write_barrier_for_map_; }
  Handle<Map> transition_map() const { return transition_map_; }
  HValueShim* value() { return &value_; }
  PointersToHereCheck PointersToHereCheckForValue() const { return pointers_to_here_check_for_value_; }

  bool has_transition() const {
    return HStoreNamedField::HasTransitionField::decode(bit_field_);
  }

  StoreFieldOrKeyedMode store_mode() const {
    return HStoreNamedField::StoreModeField::decode(bit_field_);
  }

  Representation field_representation() const {
    return access().representation();
  }

  SmiCheck SmiCheckForWriteBarrier() const {
    if (field_representation().IsHeapObject()) return OMIT_SMI_CHECK;
    if (value_.type().IsHeapObject()) return OMIT_SMI_CHECK;
    return INLINE_SMI_CHECK;
  }

 private:
  bool needs_write_barrier_, needs_write_barrier_for_map_;
  Handle<Map> transition_map_;
  HValueShim value_;
  PointersToHereCheck pointers_to_here_check_for_value_;
  uint32_t bit_field_;

  friend class LChunkSaver;
};


class HStoreNamedGenericShim : public HValueShim {
 public:
  DECLARE_SHIM(StoreNamedGeneric)

  explicit HStoreNamedGenericShim(HStoreNamedGeneric* h)
      : HValueShim(h),
        name_(h->name()),
        strict_mode_(h->strict_mode()) {}

  HStoreNamedGenericShim(HValueShim base,
                         Handle<Object> name, StrictMode strict_mode)
      : HValueShim(base),
        name_(name),
        strict_mode_(strict_mode) {}

  Handle<Object> name() const { return name_; }
  StrictMode strict_mode() { return strict_mode_; }

 private:
  Handle<Object> name_;
  StrictMode strict_mode_;
};


class HLoadNamedGenericShim : public HValueShim {
 public:
  DECLARE_SHIM(LoadNamedGeneric)

  explicit HLoadNamedGenericShim(HLoadNamedGeneric* h)
      : HValueShim(h),
        name_(h->name()) {}

  HLoadNamedGenericShim(HValueShim base, Handle<Object> name)
      : HValueShim(base),
        name_(name) {}

  Handle<Object> name() const { return name_; }

 private:
  Handle<Object> name_;
};


class HAllocateShim : public HValueShim {
 public:
  DECLARE_SHIM(Allocate)

  typedef HAllocate::Flags Flags;

  explicit HAllocateShim(HAllocate* h)
      : HValueShim(h),
        flags_(h->flags_) {}

  HAllocateShim(HValueShim base, Flags flags)
      : HValueShim(base),
        flags_(flags) {}

  bool IsNewSpaceAllocation() const {
    return flags_ & HAllocate::ALLOCATE_IN_NEW_SPACE;
  }

  bool IsOldDataSpaceAllocation() const {
    return flags_ & HAllocate::ALLOCATE_IN_OLD_DATA_SPACE;
  }

  bool IsOldPointerSpaceAllocation() const {
    return flags_ & HAllocate::ALLOCATE_IN_OLD_POINTER_SPACE;
  }

  bool MustAllocateDoubleAligned() const {
    return flags_ & HAllocate::ALLOCATE_DOUBLE_ALIGNED;
  }

  bool MustPrefillWithFiller() const {
    return flags_ & HAllocate::PREFILL_WITH_FILLER;
  }

 private:
  Flags flags_;

  friend class LChunkSaver;
};


class HTransitionElementsKindShim : public HValueShim {
 public:
  DECLARE_SHIM(TransitionElementsKind)

  explicit HTransitionElementsKindShim(HTransitionElementsKind* h)
      : HValueShim(h),
        original_map_(h->original_map().handle()),
        transitioned_map_(h->transitioned_map().handle()),
        from_kind_(h->from_kind()),
        to_kind_(h->to_kind()) {}

  HTransitionElementsKindShim(HValueShim base,
                              Handle<Map> original_map,
                              Handle<Map> transitioned_map,
                              ElementsKind from_kind,
                              ElementsKind to_kind)
      : HValueShim(base),
        original_map_(original_map),
        transitioned_map_(transitioned_map),
        from_kind_(from_kind),
        to_kind_(to_kind) {}

  Handle<Map> original_map() const { return original_map_; }
  Handle<Map> transitioned_map() const { return transitioned_map_; }
  ElementsKind from_kind() const { return from_kind_; }
  ElementsKind to_kind() const { return to_kind_; }

 private:
  Handle<Map> original_map_;
  Handle<Map> transitioned_map_;
  ElementsKind from_kind_;
  ElementsKind to_kind_;
};


class HLoadRootShim : public HValueShim {
 public:
  DECLARE_SHIM(LoadRoot)

  explicit HLoadRootShim(HLoadRoot* h)
      : HValueShim(h),
        index_(h->index()) {}

  HLoadRootShim(HValueShim base, Heap::RootListIndex index)
      : HValueShim(base),
        index_(index) {}

  Heap::RootListIndex index() const { return index_; }

 private:
  Heap::RootListIndex index_;
};


class HStringAddShim : public HValueShim {
 public:
  DECLARE_SHIM(StringAdd)

  explicit HStringAddShim(HStringAdd* h)
      : HValueShim(h),
        flags_(h->flags()),
        pretenure_flag_(h->pretenure_flag()) {}

  HStringAddShim(HValueShim base,
                 StringAddFlags flags, PretenureFlag pretenure_flag)
      : HValueShim(base),
        flags_(flags),
        pretenure_flag_(pretenure_flag) {}

  StringAddFlags flags() const { return flags_; }
  PretenureFlag pretenure_flag() const { return pretenure_flag_; }

 private:
  StringAddFlags flags_;
  PretenureFlag pretenure_flag_;
};


class HLoadGlobalGenericShim : public HValueShim {
 public:
  DECLARE_SHIM(LoadGlobalGeneric)

  explicit HLoadGlobalGenericShim(HLoadGlobalGeneric* h)
      : HValueShim(h),
        name_(h->name()),
        for_typeof_(h->for_typeof()) {}

  HLoadGlobalGenericShim(HValueShim base, Handle<Object> name,
                         bool for_typeof)
      : HValueShim(base),
        name_(name),
        for_typeof_(for_typeof) {}

  Handle<Object> name() const { return name_; }
  bool for_typeof() const { return for_typeof_; }

 private:
  Handle<Object> name_;
  bool for_typeof_;
};


class HForInCacheArrayShim : public HValueShim {
 public:
  DECLARE_SHIM(ForInCacheArray)

  explicit HForInCacheArrayShim(HForInCacheArray* h)
      : HValueShim(h),
        idx_(h->idx()) {}

  HForInCacheArrayShim(HValueShim base, int idx)
      : HValueShim(base),
        idx_(idx) {}

  int idx() const { return idx_; }

 private:
  int idx_;
};


class HRegExpLiteralShim : public HValueShim {
 public:
  DECLARE_SHIM(RegExpLiteral)

  explicit HRegExpLiteralShim(HRegExpLiteral* h)
      : HValueShim(h),
        literals_(h->literals()),
        literal_index_(h->literal_index()),
        pattern_(h->pattern()),
        flags_(h->flags()) {}

  HRegExpLiteralShim(HValueShim base, Handle<FixedArray> literals,
                     int literal_index, Handle<String> pattern,
                     Handle<String> flags)
      : HValueShim(base),
        literals_(literals),
        literal_index_(literal_index),
        pattern_(pattern),
        flags_(flags) {}

  Handle<FixedArray> literals() { return literals_; }
  int literal_index() const { return literal_index_; }
  Handle<String> pattern() { return pattern_; }
  Handle<String> flags() { return flags_; }

 private:
  Handle<FixedArray> literals_;
  int literal_index_;
  Handle<String> pattern_;
  Handle<String> flags_;
};


class HArgumentsElementsShim : public HValueShim {
 public:
  DECLARE_SHIM(ArgumentsElements)

  explicit HArgumentsElementsShim(HArgumentsElements* h)
      : HValueShim(h),
        from_inlined_(h->from_inlined()) {}

  HArgumentsElementsShim(HValueShim base, bool from_inlined)
      : HValueShim(base),
        from_inlined_(from_inlined) {}

  bool from_inlined() const { return from_inlined_; }

 private:
  bool from_inlined_;
};


class HWrapReceiverShim : public HValueShim {
 public:
  DECLARE_SHIM(WrapReceiver)

  explicit HWrapReceiverShim(HWrapReceiver* h)
      : HValueShim(h),
        known_function_(h->known_function()) {}

  HWrapReceiverShim(HValueShim base, bool known_function)
      : HValueShim(base),
        known_function_(known_function) {}

  bool known_function() const { return known_function_; }

 private:
  bool known_function_;
};


class HInstanceOfKnownGlobalShim : public HValueShim {
 public:
  DECLARE_SHIM(InstanceOfKnownGlobal)

  explicit HInstanceOfKnownGlobalShim(HInstanceOfKnownGlobal* h)
      : HValueShim(h),
        function_(h->function()) {}

  HInstanceOfKnownGlobalShim(HValueShim base, Handle<JSFunction> function)
      : HValueShim(base),
        function_(function) {}

  Handle<JSFunction> function() const { return function_; }

 private:
  Handle<JSFunction> function_;
};


class HTypeofIsAndBranchShim : public HControlInstructionShim {
 public:
  DECLARE_SHIM(TypeofIsAndBranch)

  explicit HTypeofIsAndBranchShim(HTypeofIsAndBranch* h)
      : HControlInstructionShim(h),
        type_literal_(h->type_literal()) {}

  HTypeofIsAndBranchShim(HControlInstructionShim base,
                         Handle<String> type_literal)
      : HControlInstructionShim(base),
        type_literal_(type_literal) {}

  Handle<String> type_literal() const { return type_literal_; }

 private:
  Handle<String> type_literal_;
};

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_SHIM_H_
