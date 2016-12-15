#ifndef V8_LITHIUM_SAVELOAD_X64_H_
#define V8_LITHIUM_SAVELOAD_X64_H_

#include "src/lithium-saveload.h"
#include "src/x64/lithium-x64.h"
#include "src/x64/hydrogen-shim-x64.h"

namespace v8 {
namespace internal {

#define LITHIUM_INSTRUCTIONS_FOR_WHICH_SAVELOAD_IS_YET_TO_BE_IMPLEMENTED(V) \
  V(AllocateBlockContext)                                               \
  V(CallStub)                                                           \
  V(ClampDToUint8)                                                      \
  V(ClampIToUint8)                                                      \
  V(ClampTToUint8)                                                      \
  V(DateField)                                                          \
  V(DebugBreak)                                                         \
  V(GetCachedArrayIndex)                                                \
  V(SeqStringGetChar)                                                   \
  V(SeqStringSetChar)                                                   \
  V(StoreCodeEntry)                                                     \
  V(StoreFrameContext)                                                  \
  V(TailCallThroughMegamorphicCache)                                    \
  V(TrapAllocationMemento)                                              \

class LChunkSaver : public LChunkSaverBase {
 public:
  LChunkSaver(List<char>& bytes, CompilationInfo* info)
    : LChunkSaverBase(bytes, info),
      external_reference_encoder_(new ExternalReferenceEncoder(isolate())) {}

  void Save(const LChunk*);
  void SaveInstruction(const LInstruction*);

  template<int R>
  void SaveTemplateResultInstruction(const LTemplateResultInstruction<R>*);
  template<int R, int I, int T>
  void SaveTemplateInstruction(const LTemplateInstruction<R, I, T>*);

#define DECLARE_LITHIUM_INSTRUCTION_SAVE(type)  \
  void SaveL##type(const L##type*);
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_LITHIUM_INSTRUCTION_SAVE)
#undef DECLARE_LITHIUM_INSTRUCTION_SAVE

  void SaveHydrogenShim(const LInstruction*);

#define DECLARE_HCONSTANT_SHIM_SAVE(Constant)      \
  void SaveHydrogenShim(const L##Constant*);
  LITHIUM_CONSTANT_INSTRUCTION_LIST(DECLARE_HCONSTANT_SHIM_SAVE)
#undef DECLARE_HCONSTANT_SHIM_SAVE

#define DECLARE_HYDROGEN_SHIM_SAVE(type, owner)  \
  void SaveHydrogenShim(const owner*);
  HYDROGEN_CONCRETE_SHIM_OWNER_LIST(DECLARE_HYDROGEN_SHIM_SAVE)
#undef DECLARE_HYDROGEN_SHIM_SAVE

 private:
  void SavePlatformChunk(const LPlatformChunk*);
  void SaveBasicBlocks(const ZoneList<HBasicBlock*>*);
  void SaveConstants(const LChunk*);
  void SaveInstructions(const ZoneList<LInstruction*>*);
  void SaveLGap(const LGap*);

#define DECLARE_HYDROGEN_SHIM_VALUE_SAVE(type)  \
  void Save##type(type*);
  HYDROGEN_SHIM_LIST(DECLARE_HYDROGEN_SHIM_VALUE_SAVE)
#undef DECLARE_HYDROGEN_SHIM_VALUE_SAVE

  ExternalReferenceEncoder* external_reference_encoder_;
};


template<>
inline void LChunkSaver::SaveTemplateResultInstruction(const LTemplateResultInstruction<0>* instruction) {
  // No data.
}


template<>
inline void LChunkSaver::SaveTemplateResultInstruction(const LTemplateResultInstruction<1>* instruction) {
  ConditionallySaveLOperand(instruction->result());
}


template<int R, int I, int T>
void LChunkSaver::SaveTemplateInstruction(const LTemplateInstruction<R, I, T>* instruction) {
  for (int i = 0; i < I; i++) {
    ConditionallySaveLOperand(instruction->InputAt(i));
  }
  for (int i = 0; i < T; i++) {
    ConditionallySaveLOperand(instruction->TempAt(i));
  }
}


class LChunkLoader : public LChunkLoaderBase {
 public:
  LChunkLoader(const List<char>& bytes, CompilationInfo* info)
    : LChunkLoaderBase(bytes, info),
      external_reference_decoder_(new ExternalReferenceDecoder(isolate())) {}

  LChunk* Load();
  LInstruction* LoadInstruction();

  template<int R>
  void LoadTemplateResultInstruction(LTemplateResultInstruction<R>*);

  #define DECLARE_LITHIUM_INSTRUCTION_LOAD(type)  \
  L##type* LoadL##type();
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_LITHIUM_INSTRUCTION_LOAD)
#undef DECLARE_LITHIUM_INSTRUCTION_LOAD

  HValueShim* LoadHydrogenShim(const LInstruction*);

#define DECLARE_HCONSTANT_SHIM_LOAD(Constant)           \
  HValueShim* LoadHydrogenShim(const L##Constant*);
  LITHIUM_CONSTANT_INSTRUCTION_LIST(DECLARE_HCONSTANT_SHIM_LOAD)
#undef DECLARE_HCONSTANT_SHIM_LOAD

#define DECLARE_HYDROGEN_SHIM_LOAD(type, owner) \
  HValueShim* LoadHydrogenShim(const owner*);
  HYDROGEN_CONCRETE_SHIM_OWNER_LIST(DECLARE_HYDROGEN_SHIM_LOAD)
#undef DECLARE_HYDROGEN_SHIM_LOAD

 private:
  void LoadPlatformChunk(LPlatformChunk*);
  void LoadBasicBlocks();
  void LoadConstants();
  void LoadInstructions();
  void LoadLGap(LGap* instruction);

#define DECLARE_HYDROGEN_SHIM_VALUE_LOAD(type) \
  type Load##type();
  HYDROGEN_SHIM_LIST(DECLARE_HYDROGEN_SHIM_VALUE_LOAD)
#undef DECLARE_HYDROGEN_SHIM_VALUE_LOAD

  ExternalReferenceDecoder* external_reference_decoder_;
};


template<>
inline void LChunkLoader::LoadTemplateResultInstruction(LTemplateResultInstruction<0>* instruction) {
  // No data.
}


template<>
inline void LChunkLoader::LoadTemplateResultInstruction(LTemplateResultInstruction<1>* instruction) {
  LOperand* result = ConditionallyLoadLOperand();
  instruction->set_result(result);
}

} }  // namespace v8::internal

#endif  // V8_LITHIUM_SAVELOAD_X64_H_
