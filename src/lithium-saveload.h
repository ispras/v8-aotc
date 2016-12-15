#ifndef V8_LITHIUM_SAVELOAD_H_
#define V8_LITHIUM_SAVELOAD_H_

#include "src/lithium.h"
#include "src/saveload.h"

#include "src/list.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LOperand;
class LEnvironment;
class LPointerMap;

#define RETURN_ON_FAIL(expr) {                  \
  expr;                                         \
  if (LastStatus() != SUCCEEDED) {              \
    return;                                     \
  }                                             \
}

#define RETURN_VALUE_ON_FAIL_1(value) {         \
  if (LastStatus() != SUCCEEDED) {              \
    return value;                               \
  }                                             \
}

#define RETURN_VALUE_ON_FAIL_2(value, expr) {   \
  expr;                                         \
  RETURN_VALUE_ON_FAIL_1(value)                 \
}

#define RETURN_VALUE_ON_FAIL_DISPATCH(_1, _2, name, ...) name
#define RETURN_VALUE_ON_FAIL(...)                                       \
  RETURN_VALUE_ON_FAIL_DISPATCH(__VA_ARGS__,                            \
      RETURN_VALUE_ON_FAIL_2,                                           \
      RETURN_VALUE_ON_FAIL_1,                                           \
  )(__VA_ARGS__)


class LChunkSaveloadBase BASE_EMBEDDED {
 public:
  LChunkSaveloadBase()
      : status_(SUCCEEDED),
        reason_(nullptr) {}

  enum Status {
    FAILED,
    SUCCEEDED
  };

  Status LastStatus() const { return status_; }

  void Fail(const char* reason) {
    status_ = FAILED;
    reason_ = reason;
  }

  const char* Reason() const { return reason_; }

 private:
  Status status_;
  const char* reason_;
};


class LChunkSaverBase : public LChunkSaveloadBase {
 public:
  LChunkSaverBase(List<char>& bytes, CompilationInfo* info)
      : bytes_(bytes),
        info_(info) {}

  void SaveBitVector(const BitVector*);
  void SaveObject(Object*); // Handles heap objects, SMIs, and nulls.

  // Heap.
  void SaveHeapObject(Object*);
  void SaveOddball(Oddball*);
  void SaveHeapNumber(HeapNumber*);
  void SaveName(Name*);
  void SaveSymbol(Symbol*);
  void SaveString(String*, int offset = 0, int length = -1);
  void SaveJSRegExp(JSRegExp*);
  void SaveFixedArrayBase(FixedArrayBase*);
  void SaveFixedArray(FixedArray*);
  void SaveFixedDoubleArray(FixedDoubleArray*);
  void SaveJSArray(JSArray*);
  void SaveJSArrayBuffer(JSArrayBuffer*);
  void SaveJSTypedArray(JSTypedArray*);
  void SaveTypeFeedbackVector(TypeFeedbackVector*);
  void SaveSharedFunctionInfo(SharedFunctionInfo*);
  void SaveJSFunction(JSFunction*);
  void SaveMap(Map*);
  void SaveJSObject(JSObject*);
  void SaveJSValue(JSValue*);
  void SaveExecutableAccessorInfo(ExecutableAccessorInfo*);
  void SaveForeign(Foreign*);

  // Crankshaft-specific.
  void SaveRepresentation(Representation representation);
  void SaveHType(HType htype);

  // Lithium.
  void SaveLOperand(LOperand*);
  void ConditionallySaveLOperand(LOperand*);
  void SavePointerMap(LPointerMap*);
  void SaveEnvironment(LEnvironment*);

  List<char>& bytes() { return bytes_; }
  CompilationInfo* info() { return info_; }
  Isolate* isolate() { return info_->isolate(); }
  Zone* zone() { return info_->zone(); }

 protected:
  template<typename T>
  void SavePrimitiveArray(Vector<const T> array) {
    internal::SavePrimitiveArray<T>(bytes_, array);
  }

  template<typename T>
  void SavePrimitive(T value) {
    internal::SavePrimitive<T>(bytes_, value);
  }

  void SaveTrue() {
    internal::SaveTrue(bytes_);
  }

  void SaveFalse() {
    internal::SaveFalse(bytes_);
  }

  void Synchronize() {
#ifdef DEBUG
    internal::SavePrimitive<int>(bytes_, bytes_.length());
#endif
  }

 private:
  void SaveFixedArrayData(FixedArray*);

  class MapSaver;
  friend class MapSaver;

  List<char>& bytes_;
  CompilationInfo* info_;
  List<Map*> map_cache_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LChunkSaverBase);
};


class LChunkLoaderBase : public LChunkSaveloadBase {
 public:
  LChunkLoaderBase(const List<char>& bytes, CompilationInfo* info)
      : chunk_(nullptr),
        storage_(bytes),
        bytes_(storage_.begin()),
        info_(info) {}

  void LoadBitVector(BitVector*);
  Handle<Object> LoadObject(); // Handles heap objects, SMIs, and nulls.

  // TODO: All these functions should return a MaybeHandle.

  // Heap.
  Handle<Object> LoadHeapObject();
  Handle<Oddball> LoadOddball();
  Handle<HeapNumber> LoadHeapNumber();
  Handle<Name> LoadName();
  Handle<Symbol> LoadSymbol();
  Handle<String> LoadString();
  Handle<JSRegExp> LoadJSRegExp();
  Handle<FixedArrayBase> LoadFixedArrayBase();
  Handle<FixedArray> LoadFixedArray();
  Handle<FixedDoubleArray> LoadFixedDoubleArray();
  Handle<JSArray> LoadJSArray();
  Handle<JSArrayBuffer> LoadJSArrayBuffer();
  Handle<JSTypedArray> LoadJSTypedArray();
  Handle<TypeFeedbackVector> LoadTypeFeedbackVector();
  Handle<SharedFunctionInfo> LoadSharedFunctionInfo();
  Handle<JSFunction> LoadJSFunction();
  Handle<Map> LoadMap();
  Handle<JSObject> LoadJSObject();
  Handle<JSValue> LoadJSValue();
  Handle<ExecutableAccessorInfo> LoadExecutableAccessorInfo();
  Handle<Foreign> LoadForeign();

  // Crankshaft-specific.
  Representation LoadRepresentation();
  HType LoadHType();

  // Lithium.
  LOperand* LoadLOperand();
  LOperand* ConditionallyLoadLOperand();
  LEnvironment* LoadEnvironment();
  LPointerMap* LoadPointerMap();

  LChunk* chunk() { return chunk_; }
  const char* start() const { return storage_.begin(); }
  const char** bytes() { return &bytes_; }
  CompilationInfo* info() { return info_; }
  Isolate* isolate() { return info_->isolate(); }
  Zone* zone() { return info_->zone(); }

 protected:
  void InitializeChunk();

  template<typename T>
  Vector<const T> LoadPrimitiveArray() {
    return internal::LoadPrimitiveArray<T>(&bytes_);
  }

  template<typename T>
  T LoadPrimitive() {
    return internal::LoadPrimitive<T>(&bytes_);
  }

  bool LoadBool() {
    return internal::LoadBool(&bytes_);
  }

  void Synchronize() {
#ifdef DEBUG
    int offset = *bytes() - start();
    int saved_offset = internal::LoadPrimitive<int>(&bytes_);
    CHECK(offset == saved_offset);
#endif
  }

 private:
  void LoadFixedArrayData(FixedArray*);

  class MapLoader;
  friend class MapLoader;

  LChunk* chunk_;
  const List<char>& storage_;
  const char* bytes_;
  CompilationInfo* info_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LChunkLoaderBase);
};


// We need to widen the definition of instance types a bit.
enum {
  NATIVE_CONTEXT_TYPE = LAST_TYPE + 1
};

struct DetailedInstanceType {
  DetailedInstanceType(Object* object) {
    if (object->IsNativeContext()) {
      value = NATIVE_CONTEXT_TYPE;
      return;
    }

    DCHECK(object->IsHeapObject());
    value =
      static_cast<int>(HeapObject::cast(object)->map()->instance_type());
  }

  void Normalize() {
    if (value < FIRST_NONSTRING_TYPE) {
      value = STRING_TYPE;
    }
  }

  operator int() const { return value; }

  int value;
};

} }  // namespace v8::internal

#endif  // V8_LITHIUM_SAVELOAD_H_
