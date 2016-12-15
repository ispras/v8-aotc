#include "src/x64/lithium-saveload-x64.h"
#include "src/hydrogen-osr.h"

namespace v8 {
namespace internal {


void LChunkSaver::SavePlatformChunk(const LPlatformChunk* chunk) {
  SaveBitVector(&chunk->dehoisted_key_ids_);
}


void LChunkSaver::Save(const LChunk* chunk) {
  SavePlatformChunk(reinterpret_cast<const LPlatformChunk*>(chunk));
  RETURN_ON_FAIL();

  SavePrimitive<bool>(info()->this_has_uses());
  SavePrimitive<int>(chunk->spill_slot_count());

  HGraph* graph = chunk->graph();
  if (graph->has_osr()) {
    SaveTrue();
    SavePrimitive<int>(graph->osr()->UnoptimizedFrameSlots());
  } else {
    SaveFalse();
  }

  Synchronize();

  RETURN_ON_FAIL(SaveBasicBlocks(graph->blocks()));
  RETURN_ON_FAIL(SaveConstants(chunk));
  RETURN_ON_FAIL(SaveInstructions(chunk->instructions()));

  SavePrimitive<int>(chunk->inlined_closures()->length());
  for (const Handle<JSFunction> closure: *chunk->inlined_closures()) {
    SaveSharedFunctionInfo(closure->shared());
  }
}


void LChunkSaver::SaveBasicBlocks(const ZoneList<HBasicBlock*>* blocks) {
  SavePrimitive<int>(blocks->length());

  for (int block_index = 0; block_index < blocks->length(); block_index++) {
    HBasicBlock* bb = blocks->at(block_index);
    if (block_index) {
      BailoutId ast_id = bb->last_environment()->ast_id();
      SavePrimitive(ast_id);
    }

    SavePrimitive<bool>(bb->IsLoopHeader());

    bool is_inline_return_target = bb->IsInlineReturnTarget();
    SavePrimitive<bool>(is_inline_return_target);
    if (is_inline_return_target) {
      SavePrimitive<int>(bb->inlined_entry_block()->block_id());
    }

    SavePrimitive<bool>(bb->IsReachable());
    SavePrimitive<bool>(bb->IsLoopSuccessorDominator());
    SavePrimitive<bool>(bb->is_osr_entry());
    SavePrimitive<bool>(bb->IsOrdered());

    SavePrimitive(bb->first_instruction_index());
    SavePrimitive(bb->last_instruction_index());
  }

  Synchronize();
}


static BitVector* LiveConstants(const LChunk* chunk, Zone* zone) {
  auto live = new(zone) BitVector(chunk->graph()->GetMaximumValueID(), zone);
  auto check_and_mark = [live](LOperand* operand) {
    if (operand && operand->IsConstantOperand()) {
      live->Add(operand->index());
    }
  };

  for (LInstruction* instr: *chunk->instructions()) {
    if (instr->hydrogen_value()->IsConstant()) {
      live->Add(instr->hydrogen_value()->id());
    }

    for (int i = 0; i < instr->InputCount(); ++i) {
      check_and_mark(instr->InputAt(i));
    }

    if (instr->IsGap()) {
      auto gap = LGap::cast(instr);
      for (int pos = LGap::FIRST_INNER_POSITION;
           pos <= LGap::LAST_INNER_POSITION; ++pos) {
        LParallelMove* move =
          gap->GetParallelMove(static_cast<LGap::InnerPosition>(pos));
        if (move) {
          for (LMoveOperands& move_operands: *move->move_operands()) {
            check_and_mark(move_operands.source());
            check_and_mark(move_operands.destination());
          }
        }
      }
    }

    for (LEnvironment* env = instr->environment(); env; env = env->outer()) {
      for (LOperand* value: *env->values()) {
        check_and_mark(value);
      }
    }
  }

  return live;
}


void LChunkSaver::SaveConstants(const LChunk* chunk) {
  BitVector* live_constants = LiveConstants(chunk, zone());
  SavePrimitive<int>(live_constants->Count());
  for (BitVector::Iterator it(live_constants); !it.Done(); it.Advance()) {
    int id = it.Current();
    SavePrimitive<int>(id);

    HValue* value = chunk->graph()->LookupValue(id);
    DCHECK(value->IsConstant());
    HConstantShim constant(HConstant::cast(value));
    RETURN_ON_FAIL(SaveHConstantShim(&constant));
  }
  Synchronize();
}


static void PrintInstruction(int instr_index, LInstruction* instr) {
  if (instr->IsLabel()) {
    PrintF("       basic block B%d\n", instr->hydrogen_shim()->block_id());
  }

  HeapStringAllocator allocator;
  StringStream instr_stream(&allocator);
  instr->PrintTo(&instr_stream);
  SmartArrayPointer<const char> instr_string = instr_stream.ToCString();
  PrintF("  [%02d] %s\n", instr_index, instr_string.get());
}


void LChunkSaver::SaveInstructions(const ZoneList<LInstruction*>* instructions) {
  if (FLAG_trace_saveload_code) {
    PrintF("[Saved instructions for function at %d:]\n",
           info()->function()->start_position());
  }

  SavePrimitive<int>(instructions->length());
  for (int i = 0; i < instructions->length(); ++i) {
    LInstruction* instr = instructions->at(i);
    RETURN_ON_FAIL(SaveInstruction(instr));

    if (FLAG_trace_saveload_code) {
      PrintInstruction(i, instr);
    }
  }
}


void LChunkSaver::SaveInstruction(const LInstruction* instruction) {
  SavePrimitive<LInstruction::Opcode>(instruction->opcode());

  if (instruction->IsTemplateInstruction()) {
    instruction->SaveTemplateInstructionTo(this);
    RETURN_ON_FAIL();
  }

  RETURN_ON_FAIL(instruction->SaveTo(this));
  RETURN_ON_FAIL(instruction->SaveHydrogenShimTo(this));

  if (instruction->IsTemplateResultInstruction()) {
    instruction->SaveTemplateResultInstructionTo(this);
    RETURN_ON_FAIL();
  }

  if (instruction->HasEnvironment()) {
    SaveTrue();
    SaveEnvironment(instruction->environment());
    RETURN_ON_FAIL();
  } else {
    SaveFalse();
  }

  if (instruction->HasPointerMap()) {
    SaveTrue();
    SavePointerMap(instruction->pointer_map());
    RETURN_ON_FAIL();
  } else {
    SaveFalse();
  }

  Synchronize();
}


void LChunkSaver::SaveLGap(const LGap* gap) {
  SavePrimitive<int>(gap->block()->block_id());

  for (int i = LGap::FIRST_INNER_POSITION; i <= LGap::LAST_INNER_POSITION; ++i) {
    auto pos = static_cast<LGap::InnerPosition>(i);
    const LParallelMove* move = gap->GetParallelMove(pos);

    SavePrimitive<bool>(move);
    if (!move) {
      continue;
    }

    SavePrimitive<size_t>(move->move_operands()->length());
    for (const LMoveOperands& operand: *move->move_operands()) {
      ConditionallySaveLOperand(operand.source());
      ConditionallySaveLOperand(operand.destination());
    }
  }
}


void LChunkSaver::SaveLLabel(const LLabel* label) {
  SaveLGap(label);
}


void LChunkSaver::SaveLInstructionGap(const LInstructionGap* gap) {
  SaveLGap(gap);
}


void LChunkSaver::SaveLGoto(const LGoto* gotoo) {
  SavePrimitive<int>(gotoo->block_id());
}


void LChunkSaver::SaveLModByPowerOf2I(const LModByPowerOf2I* mod) {
  SavePrimitive<int>(mod->divisor());
}


void LChunkSaver::SaveLModByConstI(const LModByConstI* mod) {
  SavePrimitive<int>(mod->divisor());
}


void LChunkSaver::SaveLDivByPowerOf2I(const LDivByPowerOf2I* div) {
  SavePrimitive<int>(div->divisor());
}


void LChunkSaver::SaveLDivByConstI(const LDivByConstI* div) {
  SavePrimitive<int>(div->divisor());
}


void LChunkSaver::SaveLFlooringDivByPowerOf2I(const LFlooringDivByPowerOf2I* div) {
  SavePrimitive<int>(div->divisor());
}


void LChunkSaver::SaveLFlooringDivByConstI(const LFlooringDivByConstI* div) {
  SavePrimitive<int>(div->divisor());
}


void LChunkSaver::SaveLArithmeticD(const LArithmeticD* arithmetic) {
  SavePrimitive<Token::Value>(arithmetic->op());
}


void LChunkSaver::SaveLArithmeticT(const LArithmeticT* arithmetic) {
  SavePrimitive<Token::Value>(arithmetic->op());
}


void LChunkSaver::SaveLShiftI(const LShiftI* shift) {
  SavePrimitive<Token::Value>(shift->op());
  SavePrimitive<bool>(shift->can_deopt());
}


void LChunkSaver::SaveLCallWithDescriptor(const LCallWithDescriptor* callwd) {
  SavePrimitive<int>(callwd->InputCount() - 1);

  for (int i = 0; i < callwd->InputCount(); ++i) {
    ConditionallySaveLOperand(callwd->InputAt(i));
  }
}


void LChunkSaver::SaveLSmiUntag(const LSmiUntag* untag) {
  SavePrimitive<bool>(untag->needs_check());
}


void LChunkSaver::SaveLInstanceOfKnownGlobal(const LInstanceOfKnownGlobal* instanceOf) {
  SaveEnvironment(instanceOf->GetDeferredLazyDeoptimizationEnvironment());
}


void LChunkSaver::SaveLDrop(const LDrop* drop) {
  SavePrimitive<int>(drop->count());
}


void LChunkSaver::SaveLContext(const LContext*) {}
void LChunkSaver::SaveLArgumentsElements(const LArgumentsElements*) {}
void LChunkSaver::SaveLArgumentsLength(const LArgumentsLength*) {}
void LChunkSaver::SaveLParameter(const LParameter*) {}
void LChunkSaver::SaveLStackCheck(const LStackCheck*) {}
void LChunkSaver::SaveLLazyBailout(const LLazyBailout*) {}
void LChunkSaver::SaveLConstantI(const LConstantI*) {}
void LChunkSaver::SaveLConstantS(const LConstantS*) {}
void LChunkSaver::SaveLConstantD(const LConstantD*) {}
void LChunkSaver::SaveLConstantE(const LConstantE*) {}
void LChunkSaver::SaveLConstantT(const LConstantT*) {}
void LChunkSaver::SaveLReturn(const LReturn*) {}
void LChunkSaver::SaveLUnknownOSRValue(const LUnknownOSRValue*) {}
void LChunkSaver::SaveLOsrEntry(const LOsrEntry*) {}
void LChunkSaver::SaveLTaggedToI(const LTaggedToI*) {}
void LChunkSaver::SaveLCompareNumericAndBranch(const LCompareNumericAndBranch*) {}
void LChunkSaver::SaveLBitI(const LBitI*) {}
void LChunkSaver::SaveLAddI(const LAddI*) {}
void LChunkSaver::SaveLSmiTag(const LSmiTag*) {}
void LChunkSaver::SaveLStoreContextSlot(const LStoreContextSlot*) {}
void LChunkSaver::SaveLLoadContextSlot(const LLoadContextSlot*) {}
void LChunkSaver::SaveLFunctionLiteral(const LFunctionLiteral*) {}
void LChunkSaver::SaveLDeoptimize(const LDeoptimize*) {}
void LChunkSaver::SaveLDummy(const LDummy*) {}
void LChunkSaver::SaveLDummyUse(const LDummyUse*) {}
void LChunkSaver::SaveLCheckSmi(const LCheckSmi*) {}
void LChunkSaver::SaveLCheckNonSmi(const LCheckNonSmi*) {}
void LChunkSaver::SaveLCheckValue(const LCheckValue*) {}
void LChunkSaver::SaveLMulI(const LMulI*) {}
void LChunkSaver::SaveLModI(const LModI*) {}
void LChunkSaver::SaveLDivI(const LDivI*) {}
void LChunkSaver::SaveLFlooringDivI(const LFlooringDivI*) {}
void LChunkSaver::SaveLMathAbs(const LMathAbs*) {}
void LChunkSaver::SaveLMathFloor(const LMathFloor*) {}
void LChunkSaver::SaveLMathRound(const LMathRound*) {}
void LChunkSaver::SaveLMathFround(const LMathFround*) {}
void LChunkSaver::SaveLMathSqrt(const LMathSqrt*) {}
void LChunkSaver::SaveLMathPowHalf(const LMathPowHalf*) {}
void LChunkSaver::SaveLMathExp(const LMathExp*) {}
void LChunkSaver::SaveLMathLog(const LMathLog*) {}
void LChunkSaver::SaveLMathClz32(const LMathClz32*) {}
void LChunkSaver::SaveLSubI(const LSubI*) {}
void LChunkSaver::SaveLMathMinMax(const LMathMinMax*) {}
void LChunkSaver::SaveLPower(const LPower*) {}
void LChunkSaver::SaveLBranch(const LBranch*) {}
void LChunkSaver::SaveLCompareMinusZeroAndBranch(const LCompareMinusZeroAndBranch*) {}
void LChunkSaver::SaveLIsStringAndBranch(const LIsStringAndBranch*) {}
void LChunkSaver::SaveLIsUndetectableAndBranch(const LIsUndetectableAndBranch*) {}
void LChunkSaver::SaveLHasInstanceTypeAndBranch(const LHasInstanceTypeAndBranch*) {}
void LChunkSaver::SaveLNumberUntagD(const LNumberUntagD*) {}
void LChunkSaver::SaveLStoreKeyed(const LStoreKeyed*) {}
void LChunkSaver::SaveLStoreKeyedGeneric(const LStoreKeyedGeneric*) {}
void LChunkSaver::SaveLStringCharFromCode(const LStringCharFromCode*) {}
void LChunkSaver::SaveLCmpObjectEqAndBranch(const LCmpObjectEqAndBranch*) {}
void LChunkSaver::SaveLCmpHoleAndBranch(const LCmpHoleAndBranch*) {}
void LChunkSaver::SaveLCmpMapAndBranch(const LCmpMapAndBranch*) {}
void LChunkSaver::SaveLClassOfTestAndBranch(const LClassOfTestAndBranch*) {}
void LChunkSaver::SaveLIsObjectAndBranch(const LIsObjectAndBranch*) {}
void LChunkSaver::SaveLIsSmiAndBranch(const LIsSmiAndBranch*) {}
void LChunkSaver::SaveLStringCompareAndBranch(const LStringCompareAndBranch*) {}
void LChunkSaver::SaveLHasCachedArrayIndexAndBranch(const LHasCachedArrayIndexAndBranch*) {}
void LChunkSaver::SaveLTypeofIsAndBranch(const LTypeofIsAndBranch*) {}
void LChunkSaver::SaveLIsConstructCallAndBranch(const LIsConstructCallAndBranch*) {}
void LChunkSaver::SaveLInteger32ToDouble(const LInteger32ToDouble*) {}
void LChunkSaver::SaveLUint32ToDouble(const LUint32ToDouble*) {}
void LChunkSaver::SaveLCheckMaps(const LCheckMaps*) {}
void LChunkSaver::SaveLDeclareGlobals(const LDeclareGlobals*) {}
void LChunkSaver::SaveLPushArgument(const LPushArgument*) {}
void LChunkSaver::SaveLCallNew(const LCallNew*) {}
void LChunkSaver::SaveLNumberTagI(const LNumberTagI*) {}
void LChunkSaver::SaveLNumberTagU(const LNumberTagU*) {}
void LChunkSaver::SaveLNumberTagD(const LNumberTagD*) {}
void LChunkSaver::SaveLStoreGlobalCell(const LStoreGlobalCell*) {}
void LChunkSaver::SaveLLoadGlobalCell(const LLoadGlobalCell*) {}
void LChunkSaver::SaveLCallRuntime(const LCallRuntime*) {}
void LChunkSaver::SaveLDoubleBits(const LDoubleBits*) {}
void LChunkSaver::SaveLCallJSFunction(const LCallJSFunction*) {}
void LChunkSaver::SaveLCallFunction(const LCallFunction*) {}
void LChunkSaver::SaveLInvokeFunction(const LInvokeFunction*) {}
void LChunkSaver::SaveLCheckInstanceType(const LCheckInstanceType*) {}
void LChunkSaver::SaveLLoadNamedField(const LLoadNamedField*) {}
void LChunkSaver::SaveLBoundsCheck(const LBoundsCheck*) {}
void LChunkSaver::SaveLLoadKeyed(const LLoadKeyed*) {}
void LChunkSaver::SaveLDoubleToI(const LDoubleToI*) {}
void LChunkSaver::SaveLDoubleToSmi(const LDoubleToSmi*) {}
void LChunkSaver::SaveLStoreNamedField(const LStoreNamedField*) {}
void LChunkSaver::SaveLStoreNamedGeneric(const LStoreNamedGeneric*) {}
void LChunkSaver::SaveLLoadNamedGeneric(const LLoadNamedGeneric*) {}
void LChunkSaver::SaveLLoadKeyedGeneric(const LLoadKeyedGeneric*) {}
void LChunkSaver::SaveLAllocate(const LAllocate*) {}
void LChunkSaver::SaveLInnerAllocatedObject(const LInnerAllocatedObject*) {}
void LChunkSaver::SaveLTransitionElementsKind(const LTransitionElementsKind*) {}
void LChunkSaver::SaveLLoadRoot(const LLoadRoot*) {}
void LChunkSaver::SaveLStringAdd(const LStringAdd*) {}
void LChunkSaver::SaveLCmpT(const LCmpT*) {}
void LChunkSaver::SaveLStringCharCodeAt(const LStringCharCodeAt*) {}
void LChunkSaver::SaveLLoadGlobalGeneric(const LLoadGlobalGeneric*) {}
void LChunkSaver::SaveLCallNewArray(const LCallNewArray*) {}
void LChunkSaver::SaveLForInPrepareMap(const LForInPrepareMap*) {}
void LChunkSaver::SaveLForInCacheArray(const LForInCacheArray*) {}
void LChunkSaver::SaveLMapEnumLength(const LMapEnumLength*) {}
void LChunkSaver::SaveLCheckMapValue(const LCheckMapValue*) {}
void LChunkSaver::SaveLLoadFieldByIndex(const LLoadFieldByIndex*) {}
void LChunkSaver::SaveLConstructDouble(const LConstructDouble*) {}
void LChunkSaver::SaveLRegExpLiteral(const LRegExpLiteral*) {}
void LChunkSaver::SaveLWrapReceiver(const LWrapReceiver*) {}
void LChunkSaver::SaveLApplyArguments(const LApplyArguments*) {}
void LChunkSaver::SaveLInstanceOf(const LInstanceOf*) {}
void LChunkSaver::SaveLThisFunction(const LThisFunction*) {}
void LChunkSaver::SaveLLoadFunctionPrototype(const LLoadFunctionPrototype*) {}
void LChunkSaver::SaveLToFastProperties(const LToFastProperties*) {}
void LChunkSaver::SaveLAccessArgumentsAt(const LAccessArgumentsAt*) {}
void LChunkSaver::SaveLTypeof(const LTypeof*) {}


void LChunkLoader::LoadPlatformChunk(LPlatformChunk* chunk) {
  LoadBitVector(&chunk->dehoisted_key_ids_);
}


LChunk* LChunkLoader::Load() {
  InitializeChunk();
  LoadPlatformChunk(reinterpret_cast<LPlatformChunk*>(chunk()));

  auto has_uses = LoadPrimitive<bool>();
  info()->set_this_has_uses(has_uses);

  auto spill_slot_count = LoadPrimitive<int>();
  chunk()->set_spill_slot_count(spill_slot_count);

  HGraph* graph = chunk()->graph();
  bool has_osr = LoadBool();
  if (has_osr) {
    HOsrBuilder* osr = new(zone()) HOsrBuilder(nullptr);
    osr->unoptimized_frame_slots_ = LoadPrimitive<int>();
    graph->set_osr(osr);
  }

  Synchronize();

  RETURN_VALUE_ON_FAIL(nullptr, LoadBasicBlocks());
  RETURN_VALUE_ON_FAIL(nullptr, LoadConstants());
  RETURN_VALUE_ON_FAIL(nullptr, LoadInstructions());

  auto number_of_inlined_closures = LoadPrimitive<int>();
  for (int i = 0; i < number_of_inlined_closures; ++i) {
    Handle<SharedFunctionInfo> shared_info = LoadSharedFunctionInfo();
    RETURN_VALUE_ON_FAIL(nullptr);

    if (shared_info->has_deoptimization_support()) {
      continue;
    }

    CompilationInfo info(shared_info, zone());
    if (!Compiler::ParseAndAnalyze(&info) ||
        !Compiler::EnsureDeoptimizationSupport(&info)) {
      if (FLAG_trace_saveload) {
        PrintF("Could not ensure deoptimization support for inlined function: ");
        shared_info->ShortPrint();
        PrintF("\n");
      }
      Fail("could not ensure deoptimization support for inlined function");
      return nullptr;
    }
  }

  return chunk();
}


void LChunkLoader::LoadBasicBlocks() {
  HGraph* graph = chunk()->graph();

  struct InlineReturnTargetRecord {
    int inline_return_target, inlined_entry_block_id;
  };
  List<InlineReturnTargetRecord> inline_return_target_records;

  auto number_of_blocks = LoadPrimitive<int>();
  for (int block_index = 0; block_index < number_of_blocks; block_index++) {
    HBasicBlock* bb;
    if (block_index == 0)
      bb = graph->blocks()->at(0);
    else {
      bb = graph->CreateBasicBlock();

      auto ast_id = LoadPrimitive<BailoutId>();

      auto env = new (zone())
        HEnvironment(NULL, info()->scope(), info()->closure(), zone());
      env->set_ast_id(ast_id);
      bb->SetInitialEnvironment(env);
    }

    auto is_loop_header = LoadPrimitive<bool>();
    if (is_loop_header) {
      bb->AttachLoopInformation();
    }

    auto is_inline_return_target = LoadPrimitive<bool>();
    if (is_inline_return_target) {
      auto inlined_entry = LoadPrimitive<int>();
      inline_return_target_records.Add({ block_index, inlined_entry });
    }

    auto is_reachable = LoadPrimitive<bool>();
    if (!is_reachable) {
      bb->MarkUnreachable();
    }

    auto dominates_loop_successors = LoadPrimitive<bool>();
    if (dominates_loop_successors) {
      bb->MarkAsLoopSuccessorDominator();
    }

    auto is_osr_entry = LoadPrimitive<bool>();
    if (is_osr_entry) {
      bb->set_osr_entry();
    }

    auto is_ordered = LoadPrimitive<bool>();
    if (is_ordered) {
      bb->MarkAsOrdered();
    }

    auto first_instruction_index = LoadPrimitive<int>();
    auto last_instruction_index = LoadPrimitive<int>();
    bb->set_first_instruction_index(first_instruction_index);
    bb->set_last_instruction_index(last_instruction_index);
  }

  for (InlineReturnTargetRecord &rec: inline_return_target_records) {
    HBasicBlock* return_target =
      graph->blocks()->at(rec.inline_return_target);
    HBasicBlock* entry_block =
      graph->blocks()->at(rec.inlined_entry_block_id);
    return_target->MarkAsInlineReturnTarget(entry_block);
  }

  Synchronize();
}


void LChunkLoader::LoadConstants() {
  auto nof_value_pairs = LoadPrimitive<int>();
  while (nof_value_pairs--) {
    auto id = LoadPrimitive<int>();
    auto constant = new(zone()) HConstantShim(LoadHConstantShim());
    RETURN_ON_FAIL();
    chunk()->SetValue(id, constant);
  }
  Synchronize();
}


void LChunkLoader::LoadInstructions() {
  if (FLAG_trace_saveload_code) {
    PrintF("[Loaded instructions for function at %d:]\n",
           info()->function()->start_position());
  }

  auto length = LoadPrimitive<int>();
  for (int i = 0; i < length; i++) {
    LInstruction* instr = LoadInstruction();
    RETURN_ON_FAIL();
    chunk()->AddInstruction(instr);

    if (FLAG_trace_saveload_code) {
      PrintInstruction(i, instr);
    }
  }
}


LInstruction* LChunkLoader::LoadInstruction() {
  LInstruction* instruction;
  auto opcode = LoadPrimitive<LInstruction::Opcode>();

  switch (opcode) {
#define DISPATCH_ON_LITHIUM_INSTRUCTION_OPCODE(type)                    \
    case LInstruction::k##type: {                                       \
      auto concrete_instruction = LoadL##type();                        \
      HValueShim* hydrogen_shim = LoadHydrogenShim(concrete_instruction); \
      RETURN_VALUE_ON_FAIL(nullptr);                                    \
      instruction = concrete_instruction;                               \
      instruction->set_hydrogen_shim(hydrogen_shim);                    \
      break;                                                            \
    }
    LITHIUM_CONCRETE_INSTRUCTION_LIST(DISPATCH_ON_LITHIUM_INSTRUCTION_OPCODE)
#undef DISPATCH_ON_LITHIUM_INSTRUCTION_OPCODE
    default:
      UNREACHABLE();
  }

  if (instruction->IsTemplateResultInstruction()) {
    instruction->LoadTemplateResultInstruction(this);
  }

  if (LoadBool()) {
    auto env = LoadEnvironment();
    instruction->set_environment(env);
  }

  if (LoadBool()) {
    auto pointer_map = LoadPointerMap();
    instruction->set_pointer_map(pointer_map);
  }

  Synchronize();

  return instruction;
}


void LChunkLoader::LoadLGap(LGap* gap) {
  for (int i = LGap::FIRST_INNER_POSITION; i <= LGap::LAST_INNER_POSITION; ++i) {
    auto pos = static_cast<LGap::InnerPosition>(i);

    if (!LoadBool()) {
      // No move for this position.
      continue;
    }

    auto move = new(zone()) LParallelMove(zone());
    gap->SetParallelMove(pos, move);

    auto number_of_operands = LoadPrimitive<size_t>();
    for (size_t i = 0; i < number_of_operands; ++i) {
      auto source = ConditionallyLoadLOperand();
      auto destination = ConditionallyLoadLOperand();
      move->AddMove(source, destination, zone());
    }
  }
}


LLabel* LChunkLoader::LoadLLabel() {
  auto id = LoadPrimitive<int>();
  auto label = new(zone()) LLabel(chunk()->graph()->blocks()->at(id));
  LoadLGap(label);
  return label;
}


LInstructionGap* LChunkLoader::LoadLInstructionGap() {
  auto id = LoadPrimitive<int>();
  auto gap = new(zone()) LInstructionGap(chunk()->graph()->blocks()->at(id));
  LoadLGap(gap);
  return gap;
}


LContext* LChunkLoader::LoadLContext() {
  return new(zone()) LContext();
}


LArgumentsElements* LChunkLoader::LoadLArgumentsElements() {
  info()->MarkAsRequiresFrame();
  return new(zone()) LArgumentsElements();
}


LArgumentsLength* LChunkLoader::LoadLArgumentsLength() {
  //info()->MarkAsRequiresFrame();
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LArgumentsLength(value);
}


LParameter* LChunkLoader::LoadLParameter() {
  return new(zone()) LParameter();
}


LGoto* LChunkLoader::LoadLGoto() {
  auto id = LoadPrimitive<int>();
  return new(zone()) LGoto(chunk()->graph()->blocks()->at(id));
}


LStackCheck* LChunkLoader::LoadLStackCheck() {
  // TODO: mark all similar cases.
  // Grep for "info()->MarkAs" and "MarkAsCall" in "lithium-x64.cc".
  info()->MarkAsDeferredCalling();
  auto context = ConditionallyLoadLOperand();
  return new(zone()) LStackCheck(context);
}


LLazyBailout* LChunkLoader::LoadLLazyBailout() {
  return new(zone()) LLazyBailout();
}


LConstantI* LChunkLoader::LoadLConstantI() {
  return new(zone()) LConstantI();
}

LConstantS* LChunkLoader::LoadLConstantS() {
  return new(zone()) LConstantS();
}

LConstantD* LChunkLoader::LoadLConstantD() {
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LConstantD(temp);
}

LConstantE* LChunkLoader::LoadLConstantE() {
  return new(zone()) LConstantE();
}

LConstantT* LChunkLoader::LoadLConstantT() {
  return new(zone()) LConstantT();
}


LReturn* LChunkLoader::LoadLReturn() {
  auto i1 = ConditionallyLoadLOperand();
  auto i2 = ConditionallyLoadLOperand();
  auto i3 = ConditionallyLoadLOperand();
  return new(zone()) LReturn(i1, i2, i3);
}


LUnknownOSRValue* LChunkLoader::LoadLUnknownOSRValue() {
  return new(zone()) LUnknownOSRValue();
}


LOsrEntry* LChunkLoader::LoadLOsrEntry() {
  return new(zone()) LOsrEntry();
}


LTaggedToI* LChunkLoader::LoadLTaggedToI() {
  auto input = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LTaggedToI(input, temp);
}


LCompareNumericAndBranch* LChunkLoader::LoadLCompareNumericAndBranch() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LCompareNumericAndBranch(left, right);
}


LBitI* LChunkLoader::LoadLBitI() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LBitI(left, right);
}


LAddI* LChunkLoader::LoadLAddI() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LAddI(left, right);
}


LSmiTag* LChunkLoader::LoadLSmiTag() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LSmiTag(value);
}


LSmiUntag* LChunkLoader::LoadLSmiUntag() {
  auto value = ConditionallyLoadLOperand();
  auto needs_check = LoadPrimitive<bool>();
  return new(zone()) LSmiUntag(value, needs_check);
}


LStoreContextSlot* LChunkLoader::LoadLStoreContextSlot() {
  auto context = ConditionallyLoadLOperand();
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LStoreContextSlot(context, value, temp);
}


LLoadContextSlot* LChunkLoader::LoadLLoadContextSlot() {
  auto context = ConditionallyLoadLOperand();
  return new(zone()) LLoadContextSlot(context);
}


LFunctionLiteral* LChunkLoader::LoadLFunctionLiteral() {
  auto context = ConditionallyLoadLOperand();
  return new(zone()) LFunctionLiteral(context);
}


LDeoptimize* LChunkLoader::LoadLDeoptimize() {
  return new(zone()) LDeoptimize();
}


LDummy* LChunkLoader::LoadLDummy() {
  return new(zone()) LDummy();
}


LDummyUse* LChunkLoader::LoadLDummyUse() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LDummyUse(value);
}


LCheckSmi* LChunkLoader::LoadLCheckSmi() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LCheckSmi(value);
}


LCheckNonSmi* LChunkLoader::LoadLCheckNonSmi() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LCheckNonSmi(value);
}


LCheckValue* LChunkLoader::LoadLCheckValue() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LCheckValue(value);
}


LMulI* LChunkLoader::LoadLMulI() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LMulI(left, right);
}


LModByPowerOf2I* LChunkLoader::LoadLModByPowerOf2I() {
  auto divident = ConditionallyLoadLOperand();
  auto divisor = LoadPrimitive<int>();
  return new(zone()) LModByPowerOf2I(divident, divisor);
}


LModByConstI* LChunkLoader::LoadLModByConstI() {
  auto divident = ConditionallyLoadLOperand();
  auto temp1 = ConditionallyLoadLOperand();
  auto temp2 = ConditionallyLoadLOperand();

  int divisor = LoadPrimitive<int>();
  return new(zone()) LModByConstI(divident, divisor, temp1, temp2);
}


LModI* LChunkLoader::LoadLModI() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LModI(left, right, temp);
}


LDivByPowerOf2I* LChunkLoader::LoadLDivByPowerOf2I() {
  auto divident = ConditionallyLoadLOperand();
  auto divisor = LoadPrimitive<int>();
  return new(zone()) LDivByPowerOf2I(divident, divisor);
}


LDivByConstI* LChunkLoader::LoadLDivByConstI() {
  auto divident = ConditionallyLoadLOperand();
  auto temp1 = ConditionallyLoadLOperand();
  auto temp2 = ConditionallyLoadLOperand();

  int divisor = LoadPrimitive<int>();
  return new(zone()) LDivByConstI(divident, divisor, temp1, temp2);
}


LDivI* LChunkLoader::LoadLDivI() {
  auto divident = ConditionallyLoadLOperand();
  auto divisor = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LDivI(divident, divisor, temp);
}


LFlooringDivByPowerOf2I* LChunkLoader::LoadLFlooringDivByPowerOf2I() {
  auto divident = ConditionallyLoadLOperand();
  auto divisor = LoadPrimitive<int>();
  return new(zone()) LFlooringDivByPowerOf2I(divident, divisor);
}


LFlooringDivByConstI* LChunkLoader::LoadLFlooringDivByConstI() {
  auto divident = ConditionallyLoadLOperand();
  auto temp1 = ConditionallyLoadLOperand();
  auto temp2 = ConditionallyLoadLOperand();
  auto temp3 = ConditionallyLoadLOperand();

  int divisor = LoadPrimitive<int>();
  return new(zone())
    LFlooringDivByConstI(divident, divisor, temp1, temp2, temp3);
}


LFlooringDivI* LChunkLoader::LoadLFlooringDivI() {
  auto divident = ConditionallyLoadLOperand();
  auto divisor = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LFlooringDivI(divident, divisor, temp);
}


LMathAbs* LChunkLoader::LoadLMathAbs() {
  auto value = ConditionallyLoadLOperand();
  auto context = ConditionallyLoadLOperand();
  return new(zone()) LMathAbs(context, value);
}


LMathFloor* LChunkLoader::LoadLMathFloor() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LMathFloor(value);
}


LMathRound* LChunkLoader::LoadLMathRound() {
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LMathRound(value, temp);
}


LMathFround* LChunkLoader::LoadLMathFround() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LMathFround(value);
}


LMathSqrt* LChunkLoader::LoadLMathSqrt() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LMathSqrt(value);
}


LMathPowHalf* LChunkLoader::LoadLMathPowHalf() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LMathPowHalf(value);
}


LMathExp* LChunkLoader::LoadLMathExp() {
  auto value = ConditionallyLoadLOperand();
  auto temp1 = ConditionallyLoadLOperand();
  auto temp2 = ConditionallyLoadLOperand();
  return new(zone()) LMathExp(value, temp1, temp2);
}


LMathLog* LChunkLoader::LoadLMathLog() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LMathLog(value);
}


LMathClz32* LChunkLoader::LoadLMathClz32() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LMathClz32(value);
}


LSubI* LChunkLoader::LoadLSubI() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LSubI(left, right);
}


LMathMinMax* LChunkLoader::LoadLMathMinMax() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LMathMinMax(left, right);
}


LPower* LChunkLoader::LoadLPower() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LPower(left, right);
}


LBranch* LChunkLoader::LoadLBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LBranch(value);
}


LCompareMinusZeroAndBranch* LChunkLoader::LoadLCompareMinusZeroAndBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LCompareMinusZeroAndBranch(value);
}


LIsStringAndBranch* LChunkLoader::LoadLIsStringAndBranch() {
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LIsStringAndBranch(value, temp);
}


LIsUndetectableAndBranch* LChunkLoader::LoadLIsUndetectableAndBranch() {
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LIsUndetectableAndBranch(value, temp);
}


LHasInstanceTypeAndBranch* LChunkLoader::LoadLHasInstanceTypeAndBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LHasInstanceTypeAndBranch(value);
}


LNumberUntagD* LChunkLoader::LoadLNumberUntagD() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LNumberUntagD(value);
}


LStoreKeyed* LChunkLoader::LoadLStoreKeyed() {
  auto object = ConditionallyLoadLOperand();
  auto key = ConditionallyLoadLOperand();
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LStoreKeyed(object, key, value);
}


LStoreKeyedGeneric* LChunkLoader::LoadLStoreKeyedGeneric() {
  auto context = ConditionallyLoadLOperand();
  auto object = ConditionallyLoadLOperand();
  auto key = ConditionallyLoadLOperand();
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LStoreKeyedGeneric(context, object, key, value);
}


LStringCharFromCode* LChunkLoader::LoadLStringCharFromCode() {
  auto context = ConditionallyLoadLOperand();
  auto char_code = ConditionallyLoadLOperand();
  return new(zone()) LStringCharFromCode(context, char_code);
}


LCmpObjectEqAndBranch* LChunkLoader::LoadLCmpObjectEqAndBranch() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LCmpObjectEqAndBranch(left, right);
}


LCmpHoleAndBranch* LChunkLoader::LoadLCmpHoleAndBranch() {
  auto object = ConditionallyLoadLOperand();
  return new(zone()) LCmpHoleAndBranch(object);
}


LCmpMapAndBranch* LChunkLoader::LoadLCmpMapAndBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LCmpMapAndBranch(value);
}


LClassOfTestAndBranch* LChunkLoader::LoadLClassOfTestAndBranch() {
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  auto temp2 = ConditionallyLoadLOperand();
  return new(zone()) LClassOfTestAndBranch(value, temp, temp2);
}


LIsObjectAndBranch* LChunkLoader::LoadLIsObjectAndBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LIsObjectAndBranch(value);
}


LIsSmiAndBranch* LChunkLoader::LoadLIsSmiAndBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LIsSmiAndBranch(value);
}


LStringCompareAndBranch* LChunkLoader::LoadLStringCompareAndBranch() {
  auto context = ConditionallyLoadLOperand();
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LStringCompareAndBranch(context, left, right);
}


LHasCachedArrayIndexAndBranch* LChunkLoader::LoadLHasCachedArrayIndexAndBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LHasCachedArrayIndexAndBranch(value);
}


LTypeofIsAndBranch* LChunkLoader::LoadLTypeofIsAndBranch() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LTypeofIsAndBranch(value);
}


LIsConstructCallAndBranch* LChunkLoader::LoadLIsConstructCallAndBranch() {
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LIsConstructCallAndBranch(temp);
}


LInteger32ToDouble* LChunkLoader::LoadLInteger32ToDouble() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LInteger32ToDouble(value);
}


LUint32ToDouble* LChunkLoader::LoadLUint32ToDouble() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LUint32ToDouble(value);
}


LArithmeticD* LChunkLoader::LoadLArithmeticD() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  auto op = LoadPrimitive<Token::Value>();
  return new(zone()) LArithmeticD(op, left, right);
}


LArithmeticT* LChunkLoader::LoadLArithmeticT() {
  auto context = ConditionallyLoadLOperand();
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  auto op = LoadPrimitive<Token::Value>();
  return new(zone()) LArithmeticT(op, context, left, right);
}


LCheckMaps* LChunkLoader::LoadLCheckMaps() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LCheckMaps(value);
}


LDeclareGlobals* LChunkLoader::LoadLDeclareGlobals() {
  auto context = ConditionallyLoadLOperand();
  return new(zone()) LDeclareGlobals(context);
}


LShiftI* LChunkLoader::LoadLShiftI() {
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  auto op = LoadPrimitive<Token::Value>();
  auto can_deopt = LoadBool();
  return new(zone()) LShiftI(op, left, right, can_deopt);
}


LPushArgument* LChunkLoader::LoadLPushArgument() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LPushArgument(value);
}


LCallNew* LChunkLoader::LoadLCallNew() {
  auto context = ConditionallyLoadLOperand();
  auto constructor = ConditionallyLoadLOperand();
  return new(zone()) LCallNew(context, constructor);
}


LNumberTagI* LChunkLoader::LoadLNumberTagI() {
  auto value = ConditionallyLoadLOperand();
  auto temp1 = ConditionallyLoadLOperand();
  auto temp2 = ConditionallyLoadLOperand();
  return new(zone()) LNumberTagI(value, temp1, temp2);
}


LNumberTagU* LChunkLoader::LoadLNumberTagU() {
  auto value = ConditionallyLoadLOperand();
  auto temp1 = ConditionallyLoadLOperand();
  auto temp2 = ConditionallyLoadLOperand();
  return new(zone()) LNumberTagU(value, temp1, temp2);
}


LNumberTagD* LChunkLoader::LoadLNumberTagD() {
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LNumberTagD(value, temp);
}


LStoreGlobalCell* LChunkLoader::LoadLStoreGlobalCell() {
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LStoreGlobalCell(value, temp);
}


LLoadGlobalCell* LChunkLoader::LoadLLoadGlobalCell() {
  return new(zone()) LLoadGlobalCell();
}


LCallRuntime* LChunkLoader::LoadLCallRuntime() {
  auto context = ConditionallyLoadLOperand();
  return new(zone()) LCallRuntime(context);
}


LDoubleBits* LChunkLoader::LoadLDoubleBits() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LDoubleBits(value);
}


LCallJSFunction* LChunkLoader::LoadLCallJSFunction() {
  auto function = ConditionallyLoadLOperand();
  return new(zone()) LCallJSFunction(function);
}


LCallFunction* LChunkLoader::LoadLCallFunction() {
  auto context = ConditionallyLoadLOperand();
  auto function = ConditionallyLoadLOperand();
  return new(zone()) LCallFunction(context, function);
}


LInvokeFunction* LChunkLoader::LoadLInvokeFunction() {
  auto context = ConditionallyLoadLOperand();
  auto function = ConditionallyLoadLOperand();
  return new(zone()) LInvokeFunction(context, function);
}


LCheckInstanceType* LChunkLoader::LoadLCheckInstanceType() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LCheckInstanceType(value);
}


LLoadNamedField* LChunkLoader::LoadLLoadNamedField() {
  auto object = ConditionallyLoadLOperand();
  return new(zone()) LLoadNamedField(object);
}


LBoundsCheck* LChunkLoader::LoadLBoundsCheck() {
  auto index = ConditionallyLoadLOperand();
  auto length = ConditionallyLoadLOperand();
  return new(zone()) LBoundsCheck(index, length);
}


LLoadKeyed* LChunkLoader::LoadLLoadKeyed() {
  auto elements = ConditionallyLoadLOperand();
  auto key = ConditionallyLoadLOperand();
  return new(zone()) LLoadKeyed(elements, key);
}


LDoubleToI* LChunkLoader::LoadLDoubleToI() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LDoubleToI(value);
}


LDoubleToSmi* LChunkLoader::LoadLDoubleToSmi() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LDoubleToSmi(value);
}


LStoreNamedField* LChunkLoader::LoadLStoreNamedField() {
  auto object = ConditionallyLoadLOperand();
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LStoreNamedField(object, value, temp);
}


LStoreNamedGeneric* LChunkLoader::LoadLStoreNamedGeneric() {
  auto context = ConditionallyLoadLOperand();
  auto object = ConditionallyLoadLOperand();
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LStoreNamedGeneric(context, object, value);
}


LLoadNamedGeneric* LChunkLoader::LoadLLoadNamedGeneric() {
  auto context = ConditionallyLoadLOperand();
  auto object = ConditionallyLoadLOperand();
  auto vector = ConditionallyLoadLOperand();
  return new(zone()) LLoadNamedGeneric(context, object, vector);
}


LLoadKeyedGeneric* LChunkLoader::LoadLLoadKeyedGeneric() {
  auto context = ConditionallyLoadLOperand();
  auto object = ConditionallyLoadLOperand();
  auto key = ConditionallyLoadLOperand();
  auto vector = ConditionallyLoadLOperand();
  return new(zone()) LLoadKeyedGeneric(context, object, key, vector);
}


LAllocate* LChunkLoader::LoadLAllocate() {
  auto context = ConditionallyLoadLOperand();
  auto size = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone()) LAllocate(context, size, temp);
}


LInnerAllocatedObject* LChunkLoader::LoadLInnerAllocatedObject() {
  auto base_object = ConditionallyLoadLOperand();
  auto offset = ConditionallyLoadLOperand();
  return new(zone()) LInnerAllocatedObject(base_object, offset);
}


LTransitionElementsKind* LChunkLoader::LoadLTransitionElementsKind() {
  auto object = ConditionallyLoadLOperand();
  auto context = ConditionallyLoadLOperand();
  auto new_map_temp = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  return new(zone())
    LTransitionElementsKind(object, context, new_map_temp, temp);
}


LLoadRoot* LChunkLoader::LoadLLoadRoot() {
  return new(zone()) LLoadRoot();
}


LStringAdd* LChunkLoader::LoadLStringAdd() {
  auto context = ConditionallyLoadLOperand();
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LStringAdd(context, left, right);
}


LCmpT* LChunkLoader::LoadLCmpT() {
  auto context = ConditionallyLoadLOperand();
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LCmpT(context, left, right);
}


LStringCharCodeAt* LChunkLoader::LoadLStringCharCodeAt() {
  auto context = ConditionallyLoadLOperand();
  auto string = ConditionallyLoadLOperand();
  auto index = ConditionallyLoadLOperand();
  return new(zone()) LStringCharCodeAt(context, string, index);
}


LLoadGlobalGeneric* LChunkLoader::LoadLLoadGlobalGeneric() {
  auto context = ConditionallyLoadLOperand();
  auto global_object = ConditionallyLoadLOperand();
  auto vector = ConditionallyLoadLOperand();
  return new(zone()) LLoadGlobalGeneric(context, global_object, vector);
}


LCallWithDescriptor* LChunkLoader::LoadLCallWithDescriptor() {
  int register_parameter_count = LoadPrimitive<int>();
  int number_of_operands = register_parameter_count + 1;

  ZoneList<LOperand*> operands(number_of_operands, zone());

  for (int i = 0; i < number_of_operands; ++i) {
    operands.Add(ConditionallyLoadLOperand(), zone());
  }

  return new(zone())
    LCallWithDescriptor(register_parameter_count, operands, zone());
}


LCallNewArray* LChunkLoader::LoadLCallNewArray() {
  auto context = ConditionallyLoadLOperand();
  auto constructor = ConditionallyLoadLOperand();
  return new(zone()) LCallNewArray(context, constructor);
}


LForInPrepareMap* LChunkLoader::LoadLForInPrepareMap() {
  auto context = ConditionallyLoadLOperand();
  auto object = ConditionallyLoadLOperand();
  return new(zone()) LForInPrepareMap(context, object);
}


LForInCacheArray* LChunkLoader::LoadLForInCacheArray() {
  auto map = ConditionallyLoadLOperand();
  return new(zone()) LForInCacheArray(map);
}


LMapEnumLength* LChunkLoader::LoadLMapEnumLength() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LMapEnumLength(value);
}


LCheckMapValue* LChunkLoader::LoadLCheckMapValue() {
  auto value = ConditionallyLoadLOperand();
  auto map = ConditionallyLoadLOperand();
  return new(zone()) LCheckMapValue(value, map);
}


LLoadFieldByIndex* LChunkLoader::LoadLLoadFieldByIndex() {
  auto object = ConditionallyLoadLOperand();
  auto index = ConditionallyLoadLOperand();
  return new(zone()) LLoadFieldByIndex(object, index);
}


LConstructDouble* LChunkLoader::LoadLConstructDouble() {
  auto hi = ConditionallyLoadLOperand();
  auto lo = ConditionallyLoadLOperand();
  return new(zone()) LConstructDouble(hi, lo);
}


LRegExpLiteral* LChunkLoader::LoadLRegExpLiteral() {
  auto context = ConditionallyLoadLOperand();
  return new(zone()) LRegExpLiteral(context);
}


LWrapReceiver* LChunkLoader::LoadLWrapReceiver() {
  auto receiver = ConditionallyLoadLOperand();
  auto function = ConditionallyLoadLOperand();
  return new(zone()) LWrapReceiver(receiver, function);
}


LApplyArguments* LChunkLoader::LoadLApplyArguments() {
  auto function = ConditionallyLoadLOperand();
  auto receiver = ConditionallyLoadLOperand();
  auto length = ConditionallyLoadLOperand();
  auto elements = ConditionallyLoadLOperand();
  return new(zone()) LApplyArguments(function, receiver, length, elements);
}


LInstanceOf* LChunkLoader::LoadLInstanceOf() {
  auto context = ConditionallyLoadLOperand();
  auto left = ConditionallyLoadLOperand();
  auto right = ConditionallyLoadLOperand();
  return new(zone()) LInstanceOf(context, left, right);
}


LInstanceOfKnownGlobal* LChunkLoader::LoadLInstanceOfKnownGlobal() {
  auto context = ConditionallyLoadLOperand();
  auto value = ConditionallyLoadLOperand();
  auto temp = ConditionallyLoadLOperand();
  LEnvironment* env = LoadEnvironment();

  auto instanceOf = new(zone()) LInstanceOfKnownGlobal(context, value, temp);
  instanceOf->SetDeferredLazyDeoptimizationEnvironment(env);
  return instanceOf;
}


LThisFunction* LChunkLoader::LoadLThisFunction() {
  return new(zone()) LThisFunction();
}


LLoadFunctionPrototype* LChunkLoader::LoadLLoadFunctionPrototype() {
  auto function = ConditionallyLoadLOperand();
  return new(zone()) LLoadFunctionPrototype(function);
}


LToFastProperties* LChunkLoader::LoadLToFastProperties() {
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LToFastProperties(value);
}


LAccessArgumentsAt* LChunkLoader::LoadLAccessArgumentsAt() {
  auto arguments = ConditionallyLoadLOperand();
  auto length = ConditionallyLoadLOperand();
  auto index = ConditionallyLoadLOperand();
  return new(zone()) LAccessArgumentsAt(arguments, length, index);
}


LDrop* LChunkLoader::LoadLDrop() {
  int count = LoadPrimitive<int>();
  return new(zone()) LDrop(count);
}


LTypeof* LChunkLoader::LoadLTypeof() {
  auto context = ConditionallyLoadLOperand();
  auto value = ConditionallyLoadLOperand();
  return new(zone()) LTypeof(context, value);
}


#define DEFINE_LITHIUM_INSTRUCTION_SAVELOAD_STUBS(type) \
  void LChunkSaver::SaveL##type(const L##type*) {       \
    UNREACHABLE();                                      \
  }                                                     \
  L##type* LChunkLoader::LoadL##type() {                \
    UNREACHABLE();                                      \
    return NULL;                                        \
  }
  LITHIUM_INSTRUCTIONS_FOR_WHICH_SAVELOAD_IS_YET_TO_BE_IMPLEMENTED(DEFINE_LITHIUM_INSTRUCTION_SAVELOAD_STUBS)
#undef DEFINE_LITHIUM_INSTRUCTION_SAVELOAD_STUBS


void LChunkSaver::SaveHydrogenShim(const LInstruction* instruction) {
  SaveHValueShim(instruction->hydrogen_shim());
}

HValueShim* LChunkLoader::LoadHydrogenShim(const LInstruction*) {
  return new(zone()) HValueShim(LoadHValueShim());
}


// Constants have been already processed in a previous pass, reuse.
#define DEFINE_HCONSTANT_SHIM_SAVELOAD(Constant)                        \
  void LChunkSaver::SaveHydrogenShim(const L##Constant* instr) {        \
    SavePrimitive<int>(instr->hydrogen_shim()->id());                   \
  }                                                                     \
  HValueShim* LChunkLoader::LoadHydrogenShim(const L##Constant*) {      \
    auto id = LoadPrimitive<int>();                                     \
    return chunk()->GetValue(id);                                       \
  }
  LITHIUM_CONSTANT_INSTRUCTION_LIST(DEFINE_HCONSTANT_SHIM_SAVELOAD)
#undef DEFINE_HCONSTANT_SHIM_SAVELOAD


#define DEFINE_HYDROGEN_SHIM_SAVELOAD(type, owner)                      \
  void LChunkSaver::SaveHydrogenShim(const owner* instruction) {        \
    Save##type(instruction->hydrogen_shim());                           \
  }                                                                     \
  HValueShim* LChunkLoader::LoadHydrogenShim(const owner*) {            \
    type value = Load##type();                                          \
    return new(zone()) type(value);                                     \
  }
  HYDROGEN_CONCRETE_SHIM_OWNER_LIST(DEFINE_HYDROGEN_SHIM_SAVELOAD)
#undef DEFINE_HYDROGEN_SHIM_SAVELOAD


// TODO: Move shims to LChunkSaverBase.


void LChunkSaver::SaveHValueShim(HValueShim* shim) {
  SavePrimitive<int>(shim->id());
  SavePrimitive<int>(shim->block_id());
  SavePrimitive<int>(shim->position().raw());
  SaveRepresentation(shim->representation());
  SaveHType(shim->type());
  SavePrimitive<int>(shim->flags());
}


HValueShim LChunkLoader::LoadHValueShim() {
  auto id = LoadPrimitive<int>();
  auto block_id = LoadPrimitive<int>();
  HSourcePosition position(LoadPrimitive<int>());
  Representation representation = LoadRepresentation();
  HType type = LoadHType();
  auto flags = LoadPrimitive<int>();
  return HValueShim(id, block_id, position, representation, type, flags);
}


void LChunkSaver::SaveHStoreContextSlotShim(HStoreContextSlotShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<int>(shim->slot_index());
  SavePrimitive<HStoreContextSlot::Mode>(shim->mode());
  SavePrimitive<bool>(shim->NeedsWriteBarrier());
  SavePrimitive<SmiCheck>(shim->CheckNeeded());
}


HStoreContextSlotShim LChunkLoader::LoadHStoreContextSlotShim() {
  auto base_shim = LoadHValueShim();
  auto slot_index = LoadPrimitive<int>();
  auto mode = LoadPrimitive<HStoreContextSlot::Mode>();
  auto needs_write_barrier = LoadPrimitive<bool>();
  auto check_needed = LoadPrimitive<SmiCheck>();
  return HStoreContextSlotShim(base_shim, slot_index, mode,
                               needs_write_barrier, check_needed);
}


void LChunkSaver::SaveHCompareGenericShim(HCompareGenericShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<Token::Value>(shim->token());
}


HCompareGenericShim LChunkLoader::LoadHCompareGenericShim() {
  auto base_shim = LoadHValueShim();
  auto token = LoadPrimitive<Token::Value>();
  return HCompareGenericShim(base_shim, token);
}


void LChunkSaver::SaveHBitwiseShim(HBitwiseShim* shim) {
  SaveHBinaryOperationShim(shim);
  SavePrimitive<Token::Value>(shim->op());
  SavePrimitive<bool>(shim->IsInteger32());
}


HBitwiseShim LChunkLoader::LoadHBitwiseShim() {
  auto base_shim = LoadHBinaryOperationShim();
  auto op = LoadPrimitive<Token::Value>();
  auto is_integer32 = LoadPrimitive<bool>();
  return HBitwiseShim(base_shim, op, is_integer32);
}


void LChunkSaver::SaveHUnaryControlInstructionShim(HUnaryControlInstructionShim* shim) {
  SaveHControlInstructionShim(shim);
  SaveHValueShim(shim->value());
}


HUnaryControlInstructionShim LChunkLoader::LoadHUnaryControlInstructionShim() {
  auto base_shim = LoadHControlInstructionShim();
  auto value = LoadHValueShim();
  return HUnaryControlInstructionShim(base_shim, value);
}


void LChunkSaver::SaveHUnaryOperationShim(HUnaryOperationShim* shim) {
  SaveHValueShim(shim);
  SaveHValueShim(shim->value());
}


HUnaryOperationShim LChunkLoader::LoadHUnaryOperationShim() {
  auto base_shim = LoadHValueShim();
  auto value = LoadHValueShim();
  return HUnaryOperationShim(base_shim, value);
}


void LChunkSaver::SaveHChangeShim(HChangeShim* shim) {
  SaveHUnaryOperationShim(shim);
  SavePrimitive<bool>(shim->can_convert_undefined_to_nan());
}


HChangeShim LChunkLoader::LoadHChangeShim() {
  auto base_shim = LoadHUnaryOperationShim();
  bool can_convert_undefined_to_nan = LoadBool();
  return HChangeShim(base_shim, can_convert_undefined_to_nan);
}


void LChunkSaver::SaveHUnaryMathOperationShim(HUnaryMathOperationShim* shim) {
  SaveHValueShim(shim);
  SaveHValueShim(shim->value());
}


HUnaryMathOperationShim LChunkLoader::LoadHUnaryMathOperationShim() {
  auto base_shim = LoadHValueShim();
  auto value = LoadHValueShim();
  return HUnaryMathOperationShim(base_shim, value);
}


void LChunkSaver::SaveHKeyedShim(HKeyedShim* shim) {
  SaveHValueShim(shim);
  SaveHValueShim(shim->key());
  SavePrimitive<ElementsKind>(shim->elements_kind());
  SavePrimitive<uint32_t>(shim->base_offset());
  SavePrimitive<bool>(shim->IsDehoisted());
}


HKeyedShim LChunkLoader::LoadHKeyedShim() {
  auto base_shim = LoadHValueShim();
  HValueShim key = LoadHValueShim();
  auto elements_kind = LoadPrimitive<ElementsKind>();
  auto base_offset = LoadPrimitive<uint32_t>();
  bool is_dehoisted = LoadBool();
  return HKeyedShim(base_shim, key, elements_kind, base_offset, is_dehoisted);
}


void LChunkSaver::SaveHStoreKeyedShim(HStoreKeyedShim* shim) {
  SaveHKeyedShim(shim);
  SaveHValueShim(shim->value());
  SavePrimitive<StoreFieldOrKeyedMode>(shim->store_mode());
  SavePrimitive<bool>(shim->NeedsWriteBarrier());
  SavePrimitive<bool>(shim->NeedsCanonicalization());
  SavePrimitive<PointersToHereCheck>(shim->PointersToHereCheckForValue());
}


HStoreKeyedShim LChunkLoader::LoadHStoreKeyedShim() {
  auto base_shim = LoadHKeyedShim();
  HValueShim value = LoadHValueShim();
  auto store_mode = LoadPrimitive<StoreFieldOrKeyedMode>();
  bool needs_write_barrier = LoadBool();
  bool needs_canonicalization = LoadBool();
  auto pointers_to_here_check_for_value =
    LoadPrimitive<PointersToHereCheck>();
  return HStoreKeyedShim(base_shim, value, store_mode,
                         needs_write_barrier, needs_canonicalization,
                         pointers_to_here_check_for_value);
}


void LChunkSaver::SaveHLoadKeyedShim(HLoadKeyedShim* shim) {
  SaveHKeyedShim(shim);
  SavePrimitive<bool>(shim->RequiresHoleCheck());
}


HLoadKeyedShim LChunkLoader::LoadHLoadKeyedShim() {
  auto base_shim = LoadHKeyedShim();
  bool requires_hole_check = LoadBool();
  return HLoadKeyedShim(base_shim, requires_hole_check);
}


void LChunkSaver::SaveHStoreKeyedGenericShim(HStoreKeyedGenericShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<StrictMode>(shim->strict_mode());
}


HStoreKeyedGenericShim LChunkLoader::LoadHStoreKeyedGenericShim() {
  auto base_shim = LoadHValueShim();
  StrictMode strict_mode = LoadPrimitive<StrictMode>();
  return HStoreKeyedGenericShim(base_shim, strict_mode);
}


void LChunkSaver::SaveHStringCharFromCodeShim(HStringCharFromCodeShim* shim) {
  SaveHValueShim(shim);
  SaveHValueShim(shim->value());
}


HStringCharFromCodeShim LChunkLoader::LoadHStringCharFromCodeShim() {
  auto base_shim = LoadHValueShim();
  auto value = LoadHValueShim();
  return HStringCharFromCodeShim(base_shim, value);
}


void LChunkSaver::SaveHCompareNumericAndBranchShim(HCompareNumericAndBranchShim* shim) {
  SaveHControlInstructionShim(shim);
  SavePrimitive<Token::Value>(shim->token());
  SavePrimitive<bool>(shim->is_double());
  SavePrimitive<bool>(shim->is_unsigned());
}


HCompareNumericAndBranchShim LChunkLoader::LoadHCompareNumericAndBranchShim() {
  auto base_shim = LoadHControlInstructionShim();
  auto token = LoadPrimitive<Token::Value>();
  auto is_double = LoadPrimitive<bool>();
  auto is_unsigned = LoadPrimitive<bool>();
  return HCompareNumericAndBranchShim(base_shim, token, is_double, is_unsigned);
}


void LChunkSaver::SaveHStringCompareAndBranchShim(HStringCompareAndBranchShim* shim) {
  SaveHControlInstructionShim(shim);
  SavePrimitive<Token::Value>(shim->token());
}


HStringCompareAndBranchShim LChunkLoader::LoadHStringCompareAndBranchShim() {
  auto base_shim = LoadHControlInstructionShim();
  auto token = LoadPrimitive<Token::Value>();
  return HStringCompareAndBranchShim(base_shim, token);
}


void LChunkSaver::SaveHDeoptimizeShim(HDeoptimizeShim* shim) {
  SaveHValueShim(shim);
  SavePrimitiveArray(CStrVector(shim->reason()));
  SavePrimitive<Deoptimizer::BailoutType>(shim->type());
}


HDeoptimizeShim LChunkLoader::LoadHDeoptimizeShim() {
  auto base_shim = LoadHValueShim();

  Vector<const char> reason_unterminated = LoadPrimitiveArray<char>();
  int length = reason_unterminated.length();
  Vector<char> reason = Vector<char>::New(length + 1);
  memcpy(reason.start(), reason_unterminated.start(), length);
  reason.last() = '\0';

  auto type = LoadPrimitive<Deoptimizer::BailoutType>();
  return HDeoptimizeShim(base_shim, reason.start(), type);
}


void LChunkSaver::SaveHFunctionLiteralShim(HFunctionLiteralShim* shim) {
  SaveHValueShim(shim);
  SaveSharedFunctionInfo(*shim->shared_info());
  SavePrimitive<bool>(shim->pretenure());
  SavePrimitive<bool>(shim->has_no_literals());
  SavePrimitive<FunctionKind>(shim->kind());
  SavePrimitive<StrictMode>(shim->strict_mode());
}


HFunctionLiteralShim LChunkLoader::LoadHFunctionLiteralShim() {
  auto base_shim = LoadHValueShim();
  Handle<SharedFunctionInfo> shared_info = LoadSharedFunctionInfo();
  RETURN_VALUE_ON_FAIL(HFunctionLiteralShim(/* failure */));
  auto pretenure = LoadBool();
  auto has_no_literals = LoadPrimitive<bool>();
  auto kind = LoadPrimitive<FunctionKind>();
  auto strict_mode = LoadPrimitive<StrictMode>();
  return HFunctionLiteralShim(base_shim, shared_info, pretenure,
                              has_no_literals, kind, strict_mode);
}


void LChunkSaver::SaveHLoadContextSlotShim(HLoadContextSlotShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<int>(shim->slot_index());
  SavePrimitive(shim->mode());
}


HLoadContextSlotShim LChunkLoader::LoadHLoadContextSlotShim() {
  auto base_shim = LoadHValueShim();
  auto slot_index = LoadPrimitive<int>();
  auto mode = LoadPrimitive<HLoadContextSlot::Mode>();
  return HLoadContextSlotShim(base_shim, slot_index, mode);
}


enum ConstantType {
  kInteger,
  kDouble,
  kExternalReference,
  kNamed,
  kLiteralsArray,
  kContext,
  kCode,
  kOther
};


void LChunkSaver::SaveHConstantShim(HConstantShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<int32_t>(shim->bit_field());

  Representation r = shim->representation();
  Handle<Object> object = shim->GetUnique().handle();

  ConstantType constant_type = kOther;

  if (shim->HasExternalReferenceValue()) {
    constant_type = kExternalReference;
  } else if (object.is_null()) {
    constant_type = IsSmiDouble(shim->DoubleValue())
      ? kInteger
      : kDouble;
  } else if (r.IsSmi() || r.IsInteger32() ||
             (r.IsTagged() && object->IsSmi())) {
    constant_type = kInteger;
  } else if (r.IsDouble() ||
             (r.IsTagged() && object->IsHeapNumber())) {
    constant_type = kDouble;
  } else if (shim->HasName()) {
    constant_type = kNamed;
  } else {
    DCHECK(!object->IsSmi() && !object->IsHeapNumber());

    if (*object == info()->closure()->literals()) {
      constant_type = kLiteralsArray;
    } else if (object->IsContext()) {
      constant_type = kContext;
    } else if (object->IsCode()) {
      constant_type = kCode;
    }
  }

  SavePrimitive<ConstantType>(constant_type);

  switch (constant_type) {
    case kInteger:
      SavePrimitive<int32_t>(shim->Integer32Value());
      return;

    case kDouble:
      SavePrimitive<double>(shim->DoubleValue());
      return;

    case kExternalReference: {
      Address address = shim->ExternalReferenceValue().address();
      SavePrimitive<uint32_t>(external_reference_encoder_->Encode(address));
      return;
    }

    case kNamed:
      SaveString(*shim->Name());
      SavePrimitive<bool>(shim->IsBuiltin());
      DCHECK(object->IsHeapObject());
      SaveMap(HeapObject::cast(*object)->map());
      break;

    case kLiteralsArray:
      break;

    case kContext:
      SaveJSFunction(*shim->ContextOwner());
      break;

    case kCode: {
      const HConstant::CodeRelocationData* data = shim->CodeRelocation();
      SavePrimitive<HConstant::CodeRelocationType>(data->type);

      switch (data->type) {
        case HConstant::kApiFunctionStub:
          SavePrimitive<bool>(data->is_store);
          SavePrimitive<bool>(data->call_data_undefined);
          SavePrimitive<int>(data->argc);
          break;

        case HConstant::kArgumentsAdaptor:
          break;

        default: UNREACHABLE();
      }
      break;
    }

    case kOther:
      SaveHeapObject(HeapObject::cast(*object));
      break;

    default: UNREACHABLE();
  }

  RETURN_ON_FAIL();

  SavePrimitive<int32_t>(shim->int32_value());
  SavePrimitive<double>(shim->double_value());
}


HConstantShim LChunkLoader::LoadHConstantShim() {
  auto base_shim = LoadHValueShim();
  auto bit_field = LoadPrimitive<int32_t>();
  auto type = LoadPrimitive<ConstantType>();

  Handle<Object> object;

  switch (type) {
    case kInteger: {
      auto value = LoadPrimitive<int32_t>();
      return HConstantShim(base_shim, value, bit_field);
    }

    case kDouble: {
      auto value = LoadPrimitive<double>();
      return HConstantShim(base_shim, value, bit_field);
    }

    case kExternalReference: {
      auto reference_id = LoadPrimitive<uint32_t>();
      Address address = external_reference_decoder_->Decode(reference_id);
      return HConstantShim(base_shim, ExternalReference(address), bit_field);
    }

    case kNamed: {
      Handle<String> name = LoadString();
      bool is_builtin = LoadBool();

      if (is_builtin) {
        Handle<GlobalObject> builtins(isolate()->js_builtins_object());
        LookupIterator lookup(builtins, name,
                              LookupIterator::OWN_SKIP_INTERCEPTOR);
        Handle<PropertyCell> cell = lookup.GetPropertyCell();
        DCHECK(cell->type()->IsConstant());
        object = cell->type()->AsConstant()->Value();
      } else {
        Handle<GlobalObject> globals(isolate()->global_object());
        object = Object::GetProperty(globals, name).ToHandleChecked();
      }

      // Load-time check-maps.
      Handle<Map> map = LoadMap();
      RETURN_VALUE_ON_FAIL(HConstantShim(/* failure */));
      if (!object->IsHeapObject() ||
          !HeapObject::cast(*object)->map()
              ->EquivalentToForDeduplication(*map)) {
        Fail("object layout changed - can't reference by name");
        return HConstantShim(/* failure */);
      }

      break;
    }

    case kLiteralsArray: {
      object = handle(info()->closure()->literals());
      break;
    }

    case kContext: {
      Handle<JSFunction> owner = LoadJSFunction();
      RETURN_VALUE_ON_FAIL(HConstantShim(/* failure */));
      object = handle(owner->context());
      break;
    }

    case kCode: {
      auto type = LoadPrimitive<HConstant::CodeRelocationType>();

      switch (type) {
        case HConstant::kApiFunctionStub: {
          auto is_store = LoadPrimitive<bool>();
          auto call_data_undefined = LoadPrimitive<bool>();
          auto argc = LoadPrimitive<int>();
          CallApiFunctionStub stub(isolate(), is_store,
                                   call_data_undefined, argc);
          object = stub.GetCode();
          break;
        }

        case HConstant::kArgumentsAdaptor:
          object = isolate()->builtins()->ArgumentsAdaptorTrampoline();
          break;

        default: UNREACHABLE();
      }
      break;
    }

    case kOther: {
      object = LoadHeapObject();
      break;
    }

    default: UNREACHABLE();
  }

  auto int32_value = LoadPrimitive<int32_t>();
  auto double_value = LoadPrimitive<double>();
  return HConstantShim(base_shim, object,
                       int32_value, double_value, bit_field);
}


void LChunkSaver::SaveHStackCheckShim(HStackCheckShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<HStackCheck::Type>(shim->type());
}


HStackCheckShim LChunkLoader::LoadHStackCheckShim() {
  auto base_shim = LoadHValueShim();
  auto type = LoadPrimitive<HStackCheck::Type>();
  return HStackCheckShim(base_shim, type);
}


void LChunkSaver::SaveHControlInstructionShim(HControlInstructionShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<int>(shim->true_block_id());
  SavePrimitive<int>(shim->false_block_id());
}


HControlInstructionShim LChunkLoader::LoadHControlInstructionShim() {
  auto base_shim = LoadHValueShim();
  auto true_block_id = LoadPrimitive<int>();
  auto false_block_id = LoadPrimitive<int>();
  return HControlInstructionShim(base_shim, true_block_id, false_block_id);
}


void LChunkSaver::SaveHCompareMapShim(HCompareMapShim* shim) {
  SaveHControlInstructionShim(shim);
  SaveMap(*shim->map());
}


HCompareMapShim LChunkLoader::LoadHCompareMapShim() {
  auto base_shim = LoadHControlInstructionShim();
  Handle<Map> map = LoadMap();
  RETURN_VALUE_ON_FAIL(HCompareMapShim(/* failure */));
  return HCompareMapShim(base_shim, map);
}


void LChunkSaver::SaveHBinaryOperationShim(HBinaryOperationShim* shim) {
  SaveHValueShim(shim);
  SaveRepresentation(shim->left_representation());
  SaveRepresentation(shim->right_representation());
}


HBinaryOperationShim LChunkLoader::LoadHBinaryOperationShim() {
  auto base_shim = LoadHValueShim();
  auto left_representation = LoadRepresentation();
  auto right_representation = LoadRepresentation();
  return HBinaryOperationShim(base_shim,
                              left_representation, right_representation);
}


void LChunkSaver::SaveHAddShim(HAddShim* shim) {
  SaveHBinaryOperationShim(shim);
  SavePrimitive<bool>(shim->use_lea());
}


HAddShim LChunkLoader::LoadHAddShim() {
  auto base_shim = LoadHBinaryOperationShim();
  auto use_lea = LoadPrimitive<bool>();
  return HAddShim(base_shim, use_lea);
}


void LChunkSaver::SaveHMathMinMaxShim(HMathMinMaxShim* shim) {
  SaveHBinaryOperationShim(shim);
  SavePrimitive<HMathMinMax::Operation>(shim->operation());
}


HMathMinMaxShim LChunkLoader::LoadHMathMinMaxShim() {
  auto base_shim = LoadHBinaryOperationShim();
  auto operation = LoadPrimitive<HMathMinMax::Operation>();
  return HMathMinMaxShim(base_shim, operation);
}


void LChunkSaver::SaveHPowerShim(HPowerShim* shim) {
  SaveHValueShim(shim);
  SaveRepresentation(shim->left_representation());
  SaveRepresentation(shim->right_representation());
}


HPowerShim LChunkLoader::LoadHPowerShim() {
  auto base_shim = LoadHValueShim();
  auto left_representation = LoadRepresentation();
  auto right_representation = LoadRepresentation();
  return HPowerShim(base_shim, left_representation, right_representation);
}


void LChunkSaver::SaveHCheckValueShim(HCheckValueShim* shim) {
  SaveHValueShim(shim);
  Handle<Object> object = shim->object();
  DCHECK(object->IsHeapObject());
  SaveHeapObject(*object);
}


HCheckValueShim LChunkLoader::LoadHCheckValueShim() {
  auto base_shim = LoadHValueShim();
  Handle<Object> object = LoadHeapObject();
  return HCheckValueShim(base_shim, object);
}


void LChunkSaver::SaveHCheckMapsShim(HCheckMapsShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<uint32_t>(shim->bit_field());

  const UniqueSet<Map>* maps = shim->maps();
  SavePrimitive<int>(maps->size());
  for (const Unique<Map>& uniq: *maps) {
    SaveMap(*uniq.handle());
    RETURN_ON_FAIL();
  }
}


HCheckMapsShim LChunkLoader::LoadHCheckMapsShim() {
  auto base_shim = LoadHValueShim();
  auto bit_field = LoadPrimitive<uint32_t>();

  auto number_of_maps = LoadPrimitive<int>();
  auto maps = new(zone()) UniqueSet<Map>(number_of_maps, zone());
  for (int i = 0; i < number_of_maps; ++i) {
    Handle<Map> map = LoadMap();
    maps->Add(Unique<Map>::CreateImmovable(map), zone());
  }

  return HCheckMapsShim(base_shim, bit_field, maps);
}


void LChunkSaver::SaveHDeclareGlobalsShim(HDeclareGlobalsShim* shim) {
  SaveHValueShim(shim);
  SaveFixedArray(*shim->pairs());
  SavePrimitive<int>(shim->flags());
}


HDeclareGlobalsShim LChunkLoader::LoadHDeclareGlobalsShim() {
  auto base_shim = LoadHValueShim();
  Handle<FixedArray> pairs = LoadFixedArray();
  auto flags = LoadPrimitive<int>();
  return HDeclareGlobalsShim(base_shim, pairs, flags);
}


void LChunkSaver::SaveHCallShim(HCallShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<int>(shim->argument_count());
}


HCallShim LChunkLoader::LoadHCallShim() {
  auto base_shim = LoadHValueShim();
  auto argument_count = LoadPrimitive<int>();
  return HCallShim(base_shim, argument_count);
}


void LChunkSaver::SaveHGlobalCellShim(HGlobalCellShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<bool>(shim->RequiresHoleCheck());
  SaveString(*shim->name());
}


HGlobalCellShim LChunkLoader::LoadHGlobalCellShim() {
  auto base_shim = LoadHValueShim();
  bool requires_hole_check = LoadPrimitive<bool>();

  DCHECK(info()->global_object()->IsJSGlobalObject());
  Handle<JSGlobalObject> global(
    JSGlobalObject::cast(info()->global_object()));

  Handle<String> name = LoadString();
  Handle<PropertyCell> cell =
    JSGlobalObject::EnsurePropertyCell(global, name);

  return HGlobalCellShim(base_shim, name, cell, requires_hole_check);
}


void LChunkSaver::SaveHCallRuntimeShim(HCallRuntimeShim* shim) {
  SaveHCallShim(shim);
  SavePrimitive<SaveFPRegsMode>(shim->save_doubles());
  SavePrimitive<Runtime::FunctionId>(shim->function()->function_id);
}


HCallRuntimeShim LChunkLoader::LoadHCallRuntimeShim() {
  auto base_shim = LoadHCallShim();
  auto save_doubles = LoadPrimitive<SaveFPRegsMode>();

  auto function_id = LoadPrimitive<Runtime::FunctionId>();
  const Runtime::Function* function = Runtime::FunctionForId(function_id);

  return HCallRuntimeShim(base_shim, function, save_doubles);
}


void LChunkSaver::SaveHDoubleBitsShim(HDoubleBitsShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<HDoubleBits::Bits>(shim->bits());
}


HDoubleBitsShim LChunkLoader::LoadHDoubleBitsShim() {
  auto base_shim = LoadHValueShim();
  auto bits = LoadPrimitive<HDoubleBits::Bits>();
  return HDoubleBitsShim(base_shim, bits);
}


void LChunkSaver::SaveHCallJSFunctionShim(HCallJSFunctionShim* shim) {
  SaveHCallShim(shim);
  SavePrimitive<bool>(shim->pass_argument_count());

  MaybeHandle<JSFunction> function = shim->function(isolate());
  if (!function.is_null()) {
    SaveTrue();
    SaveJSFunction(*function.ToHandleChecked());
  } else {
    SaveFalse();
  }
}


HCallJSFunctionShim LChunkLoader::LoadHCallJSFunctionShim() {
  auto base_shim = LoadHCallShim();
  bool pass_argument_count = LoadBool();

  MaybeHandle<JSFunction> function;
  if (LoadBool()) {
    RETURN_VALUE_ON_FAIL(HCallJSFunctionShim(/* failure */),
                         function = LoadJSFunction());
  }

  return HCallJSFunctionShim(base_shim, function, pass_argument_count);
}


void LChunkSaver::SaveHCallFunctionShim(HCallFunctionShim* shim) {
  SaveHCallShim(shim);
  SavePrimitive<CallFunctionFlags>(shim->function_flags());
}


HCallFunctionShim LChunkLoader::LoadHCallFunctionShim() {
  auto base_shim = LoadHCallShim();
  auto function_flags = LoadPrimitive<CallFunctionFlags>();
  return HCallFunctionShim(base_shim, function_flags);
}


void LChunkSaver::SaveHInvokeFunctionShim(HInvokeFunctionShim* shim) {
  SaveHCallShim(shim);
}


HInvokeFunctionShim LChunkLoader::LoadHInvokeFunctionShim() {
  auto base_shim = LoadHCallShim();
  return HInvokeFunctionShim(base_shim);
}


void LChunkSaver::SaveHCallNewArrayShim(HCallNewArrayShim* shim) {
  SaveHCallShim(shim);
  SavePrimitive<ElementsKind>(shim->elements_kind());
}


HCallNewArrayShim LChunkLoader::LoadHCallNewArrayShim() {
  auto base_shim = LoadHCallShim();
  auto elements_kind = LoadPrimitive<ElementsKind>();
  return HCallNewArrayShim(base_shim, elements_kind);
}


void LChunkSaver::SaveHCheckInstanceTypeShim(HCheckInstanceTypeShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<bool>(shim->is_interval_check());

  if (shim->is_interval_check()) {
    InstanceType first, last;
    shim->GetCheckInterval(&first, &last);
    SavePrimitive<InstanceType>(first);
    SavePrimitive<InstanceType>(last);
  } else {
    uint8_t mask, tag;
    shim->GetCheckMaskAndTag(&mask, &tag);
    SavePrimitive<uint8_t>(mask);
    SavePrimitive<uint8_t>(tag);
  }
}


HCheckInstanceTypeShim LChunkLoader::LoadHCheckInstanceTypeShim() {
  auto base_shim = LoadHValueShim();
  bool is_interval_check = LoadBool();

  if (is_interval_check) {
    auto first = LoadPrimitive<InstanceType>();
    auto last = LoadPrimitive<InstanceType>();
    return HCheckInstanceTypeShim(base_shim, first, last);
  } else {
    auto mask = LoadPrimitive<uint8_t>();
    auto tag = LoadPrimitive<uint8_t>();
    return HCheckInstanceTypeShim(base_shim, mask, tag);
  }
}


void LChunkSaver::SaveHLoadNamedFieldShim(HLoadNamedFieldShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<uint32_t>(shim->value());
}


HLoadNamedFieldShim LChunkLoader::LoadHLoadNamedFieldShim() {
  auto base_shim = LoadHValueShim();
  auto value = LoadPrimitive<uint32_t>();
  return HLoadNamedFieldShim(base_shim, value);
}


void LChunkSaver::SaveHBoundsCheckShim(HBoundsCheckShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<bool>(shim->skip_check());
  SavePrimitive<bool>(shim->allow_equality());
  SaveHValueShim(shim->index());
  SaveHValueShim(shim->length());
}


HBoundsCheckShim LChunkLoader::LoadHBoundsCheckShim() {
  auto base_shim = LoadHValueShim();
  bool skip_check = LoadBool();
  bool allow_equality = LoadBool();
  HValueShim index = LoadHValueShim();
  HValueShim length = LoadHValueShim();
  return HBoundsCheckShim(base_shim, skip_check, allow_equality,
                          index, length);
}


void LChunkSaver::SaveHDoubleToIShim(HDoubleToIShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<bool>(shim->CanTruncateToInt32());
}


HDoubleToIShim LChunkLoader::LoadHDoubleToIShim() {
  auto base_shim = LoadHValueShim();
  bool can_truncate_to_int32 = LoadBool();
  return HDoubleToIShim(base_shim, can_truncate_to_int32);
}


void LChunkSaver::SaveHStoreNamedFieldShim(HStoreNamedFieldShim* shim) {
  SaveHLoadNamedFieldShim(shim);
  SavePrimitive<bool>(shim->NeedsWriteBarrier());
  SavePrimitive<bool>(shim->NeedsWriteBarrierForMap());
  SaveHValueShim(shim->value());
  SavePrimitive<PointersToHereCheck>(shim->PointersToHereCheckForValue());
  SavePrimitive<uint32_t>(shim->bit_field_);

  if (!shim->transition_map().is_null()) {
    SaveTrue();
    SaveMap(*shim->transition_map());
  } else {
    SaveFalse();
  }
}


HStoreNamedFieldShim LChunkLoader::LoadHStoreNamedFieldShim() {
  auto base_shim = LoadHLoadNamedFieldShim();
  bool needs_write_barrier = LoadBool();
  bool needs_write_barrier_for_map = LoadBool();
  HValueShim value = LoadHValueShim();
  auto pointers_to_here_check_for_value =
    LoadPrimitive<PointersToHereCheck>();
  auto bit_field = LoadPrimitive<uint32_t>();

  Handle<Map> transition_map;
  if (LoadBool()) {
    RETURN_VALUE_ON_FAIL(HStoreNamedFieldShim(/* failure */),
                         transition_map = LoadMap());
  }

  return HStoreNamedFieldShim(base_shim,
                              needs_write_barrier,
                              needs_write_barrier_for_map,
                              transition_map,
                              value,
                              pointers_to_here_check_for_value,
                              bit_field);
}


void LChunkSaver::SaveHStoreNamedGenericShim(HStoreNamedGenericShim* shim) {
  SaveHValueShim(shim);
  SaveHeapObject(*shim->name());
  SavePrimitive<StrictMode>(shim->strict_mode());
}


HStoreNamedGenericShim LChunkLoader::LoadHStoreNamedGenericShim() {
  auto base_shim = LoadHValueShim();
  Handle<Object> name = LoadHeapObject();
  auto strict_mode = LoadPrimitive<StrictMode>();
  return HStoreNamedGenericShim(base_shim, name, strict_mode);
}


void LChunkSaver::SaveHLoadNamedGenericShim(HLoadNamedGenericShim* shim) {
  SaveHValueShim(shim);
  SaveHeapObject(*shim->name());
}


HLoadNamedGenericShim LChunkLoader::LoadHLoadNamedGenericShim() {
  auto base_shim = LoadHValueShim();
  Handle<Object> name = LoadHeapObject();
  return HLoadNamedGenericShim(base_shim, name);
}


void LChunkSaver::SaveHAllocateShim(HAllocateShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<HAllocateShim::Flags>(shim->flags_);
}


HAllocateShim LChunkLoader::LoadHAllocateShim() {
  auto base_shim = LoadHValueShim();
  auto flags = LoadPrimitive<HAllocateShim::Flags>();
  return HAllocateShim(base_shim, flags);
}


void LChunkSaver::SaveHBranchShim(HBranchShim* shim) {
  SaveHUnaryControlInstructionShim(shim);
  SavePrimitive<byte>(shim->expected_input_types().ToByte());
}


HBranchShim LChunkLoader::LoadHBranchShim() {
  auto base_shim = LoadHUnaryControlInstructionShim();
  auto bits = LoadPrimitive<byte>();
  return HBranchShim(base_shim, ToBooleanStub::Types(bits));
}


void LChunkSaver::SaveHTransitionElementsKindShim(HTransitionElementsKindShim* shim) {
  SaveHValueShim(shim);
  SaveMap(*shim->original_map());
  SaveMap(*shim->transitioned_map());
  SavePrimitive<ElementsKind>(shim->from_kind());
  SavePrimitive<ElementsKind>(shim->to_kind());
}


HTransitionElementsKindShim LChunkLoader::LoadHTransitionElementsKindShim() {
  auto base_shim = LoadHValueShim();
  Handle<Map> original_map, transitioned_map;
  RETURN_VALUE_ON_FAIL(HTransitionElementsKindShim(/* failure */),
                       original_map = LoadMap());
  RETURN_VALUE_ON_FAIL(HTransitionElementsKindShim(/* failure */),
                       transitioned_map = LoadMap());
  auto from_kind = LoadPrimitive<ElementsKind>();
  auto to_kind = LoadPrimitive<ElementsKind>();
  return HTransitionElementsKindShim(base_shim, original_map,
                                     transitioned_map, from_kind, to_kind);
}


void LChunkSaver::SaveHLoadRootShim(HLoadRootShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<Heap::RootListIndex>(shim->index());
}


HLoadRootShim LChunkLoader::LoadHLoadRootShim() {
  auto base_shim = LoadHValueShim();
  auto index = LoadPrimitive<Heap::RootListIndex>();
  return HLoadRootShim(base_shim, index);
}


void LChunkSaver::SaveHStringAddShim(HStringAddShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<StringAddFlags>(shim->flags());
  SavePrimitive<PretenureFlag>(shim->pretenure_flag());
}


HStringAddShim LChunkLoader::LoadHStringAddShim() {
  auto base_shim = LoadHValueShim();
  auto flags = LoadPrimitive<StringAddFlags>();
  auto pretenure_flag = LoadPrimitive<PretenureFlag>();
  return HStringAddShim(base_shim, flags, pretenure_flag);
}


void LChunkSaver::SaveHLoadGlobalGenericShim(HLoadGlobalGenericShim* shim) {
  SaveHValueShim(shim);

  DCHECK(shim->name()->IsName());
  SaveName(Name::cast(*shim->name()));

  SavePrimitive<bool>(shim->for_typeof());
}


HLoadGlobalGenericShim LChunkLoader::LoadHLoadGlobalGenericShim() {
  auto base_shim = LoadHValueShim();
  Handle<Object> name = LoadName();
  bool for_typeof = LoadPrimitive<bool>();
  return HLoadGlobalGenericShim(base_shim, name, for_typeof);
}


void LChunkSaver::SaveHForInCacheArrayShim(HForInCacheArrayShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<int>(shim->idx());
}


HForInCacheArrayShim LChunkLoader::LoadHForInCacheArrayShim() {
  auto base_shim = LoadHValueShim();
  auto idx = LoadPrimitive<int>();
  return HForInCacheArrayShim(base_shim, idx);
}


void LChunkSaver::SaveHRegExpLiteralShim(HRegExpLiteralShim* shim) {
  SaveHValueShim(shim);
  SaveFixedArray(*shim->literals());
  SavePrimitive<int>(shim->literal_index());
  SaveString(*shim->pattern());
  SaveString(*shim->flags());
}


HRegExpLiteralShim LChunkLoader::LoadHRegExpLiteralShim() {
  auto base_shim = LoadHValueShim();
  Handle<FixedArray> literals = LoadFixedArray();
  int literal_index = LoadPrimitive<int>();
  Handle<String> pattern = LoadString();
  Handle<String> flags = LoadString();
  return HRegExpLiteralShim(base_shim, literals, literal_index,
                            pattern, flags);
}


void LChunkSaver::SaveHArgumentsElementsShim(HArgumentsElementsShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<bool>(shim->from_inlined());
}


HArgumentsElementsShim LChunkLoader::LoadHArgumentsElementsShim() {
  auto base_shim = LoadHValueShim();
  auto from_inlined = LoadPrimitive<bool>();
  return HArgumentsElementsShim(base_shim, from_inlined);
}


void LChunkSaver::SaveHWrapReceiverShim(HWrapReceiverShim* shim) {
  SaveHValueShim(shim);
  SavePrimitive<bool>(shim->known_function());
}


HWrapReceiverShim LChunkLoader::LoadHWrapReceiverShim() {
  auto base_shim = LoadHValueShim();
  auto known_function = LoadPrimitive<bool>();
  return HWrapReceiverShim(base_shim, known_function);
}


void LChunkSaver::SaveHInstanceOfKnownGlobalShim(HInstanceOfKnownGlobalShim* shim) {
  SaveHValueShim(shim);
  SaveJSFunction(*shim->function());
}


HInstanceOfKnownGlobalShim LChunkLoader::LoadHInstanceOfKnownGlobalShim() {
  auto base_shim = LoadHValueShim();
  auto function = LoadJSFunction();
  RETURN_VALUE_ON_FAIL(HInstanceOfKnownGlobalShim(/* failure */));
  return HInstanceOfKnownGlobalShim(base_shim, function);
}


void LChunkSaver::SaveHTypeofIsAndBranchShim(HTypeofIsAndBranchShim* shim) {
  SaveHControlInstructionShim(shim);
  SaveString(*shim->type_literal());
}


HTypeofIsAndBranchShim LChunkLoader::LoadHTypeofIsAndBranchShim() {
  auto base_shim = LoadHControlInstructionShim();
  auto type_literal = LoadString();
  return HTypeofIsAndBranchShim(base_shim, type_literal);
}

} }  // namespace v8::internal
