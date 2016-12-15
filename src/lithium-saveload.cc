#include "src/lithium-saveload.h"

#if V8_TARGET_ARCH_X64
#include "src/x64/lithium-x64.h"  // NOLINT
#include "src/x64/lithium-saveload-x64.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

// TODO: Fix PretenureFlag for heap allocations.

void LChunkSaverBase::SaveBitVector(const BitVector* bit_vector) {
  SavePrimitive<int>(bit_vector->length_);
  SavePrimitive<int>(bit_vector->data_length_);

  for (int i = 0; i < bit_vector->data_length_; ++i) {
    SavePrimitive<uintptr_t>(bit_vector->data_[i]);
  }
}


void LChunkLoaderBase::LoadBitVector(BitVector* bit_vector) {
  DCHECK(!bit_vector->data_);
  bit_vector->length_ = LoadPrimitive<int>();
  bit_vector->data_length_ = LoadPrimitive<int>();
  bit_vector->data_ = zone()->NewArray<uintptr_t>(bit_vector->data_length_);

  for (int i = 0; i < bit_vector->data_length_; ++i) {
    bit_vector->data_[i] = LoadPrimitive<uintptr_t>();
  }
}


enum ObjectType {
  HEAP_OBJECT_TYPE,
  SMI_TYPE,
  NULL_TYPE
};


void LChunkSaverBase::SaveObject(Object* object) {
  if (!object) {
    SavePrimitive<ObjectType>(NULL_TYPE);
  } else if (object->IsHeapObject()) {
    SavePrimitive<ObjectType>(HEAP_OBJECT_TYPE);
    SaveHeapObject(object);
  } else {
    SavePrimitive<ObjectType>(SMI_TYPE);
    int value = Smi::cast(object)->value();
    SavePrimitive<int>(value);
  }
}


Handle<Object> LChunkLoaderBase::LoadObject() {
  auto type = LoadPrimitive<ObjectType>();
  switch (type) {
    case HEAP_OBJECT_TYPE:
      return LoadHeapObject();

    case SMI_TYPE: {
      auto value = LoadPrimitive<int>();
      return Handle<Smi>(Smi::FromInt(value), isolate());
    }

    default: break;
  }

  DCHECK(type == NULL_TYPE);
  return Handle<Object>::null();
}


void LChunkSaverBase::SaveHeapObject(Object* object) {
  DCHECK(object->IsHeapObject());

  DetailedInstanceType type(object);
  type.Normalize();
  SavePrimitive<DetailedInstanceType>(type);

  switch (type) {
    case ODDBALL_TYPE:
      SaveOddball(Oddball::cast(object));
      return;

    case HEAP_NUMBER_TYPE:
      SaveHeapNumber(HeapNumber::cast(object));
      return;

    case SYMBOL_TYPE:
      SaveSymbol(Symbol::cast(object));
      return;

    case STRING_TYPE:
      SaveString(String::cast(object));
      return;

    case JS_REGEXP_TYPE:
      SaveJSRegExp(JSRegExp::cast(object));
      return;

    case FIXED_ARRAY_TYPE:
      SaveFixedArray(FixedArray::cast(object));
      return;

    case FIXED_DOUBLE_ARRAY_TYPE:
      SaveFixedDoubleArray(FixedDoubleArray::cast(object));
      return;

    case JS_ARRAY_TYPE:
      SaveJSArray(JSArray::cast(object));
      return;

    case JS_ARRAY_BUFFER_TYPE:
      SaveJSArrayBuffer(JSArrayBuffer::cast(object));
      return;

    case JS_TYPED_ARRAY_TYPE:
      SaveJSTypedArray(JSTypedArray::cast(object));
      return;

    case JS_FUNCTION_TYPE:
      SaveJSFunction(JSFunction::cast(object));
      return;

    case SHARED_FUNCTION_INFO_TYPE:
      SaveSharedFunctionInfo(SharedFunctionInfo::cast(object));
      return;

    case MAP_TYPE:
      SaveMap(Map::cast(object));
      return;

    case JS_OBJECT_TYPE:
      SaveJSObject(JSObject::cast(object));
      return;

    case JS_VALUE_TYPE:
      SaveJSValue(JSValue::cast(object));
      return;

    case EXECUTABLE_ACCESSOR_INFO_TYPE:
      SaveExecutableAccessorInfo(ExecutableAccessorInfo::cast(object));
      return;

    case FOREIGN_TYPE:
      SaveForeign(Foreign::cast(object));
      return;

    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_BUILTINS_OBJECT_TYPE:
    case NATIVE_CONTEXT_TYPE:
    case ALLOCATION_SITE_TYPE:
    case ACCESSOR_PAIR_TYPE:
      return;

    default:
      UNREACHABLE();
  }
}


Handle<Object> LChunkLoaderBase::LoadHeapObject() {
  auto type = LoadPrimitive<DetailedInstanceType>();

  switch (type) {
    case ODDBALL_TYPE: return LoadOddball();
    case HEAP_NUMBER_TYPE: return LoadHeapNumber();
    case SYMBOL_TYPE: return LoadSymbol();
    case STRING_TYPE: return LoadString();
    case JS_REGEXP_TYPE: return LoadJSRegExp();
    case FIXED_ARRAY_TYPE: return LoadFixedArray();
    case FIXED_DOUBLE_ARRAY_TYPE: return LoadFixedDoubleArray();
    case JS_ARRAY_TYPE: return LoadJSArray();
    case JS_ARRAY_BUFFER_TYPE: return LoadJSArrayBuffer();
    case JS_TYPED_ARRAY_TYPE: return LoadJSTypedArray();
    case JS_FUNCTION_TYPE: return LoadJSFunction();
    case SHARED_FUNCTION_INFO_TYPE: return LoadSharedFunctionInfo();
    case MAP_TYPE: return LoadMap();
    case JS_OBJECT_TYPE: return LoadJSObject();
    case JS_VALUE_TYPE: return LoadJSValue();
    case EXECUTABLE_ACCESSOR_INFO_TYPE: return LoadExecutableAccessorInfo();
    case FOREIGN_TYPE: return LoadForeign();

    case JS_GLOBAL_OBJECT_TYPE:
      return isolate()->global_object();

    case JS_GLOBAL_PROXY_TYPE:
      return handle(isolate()->global_proxy());

    case JS_BUILTINS_OBJECT_TYPE:
      return isolate()->js_builtins_object();

    case NATIVE_CONTEXT_TYPE:
      return isolate()->native_context();

    case ALLOCATION_SITE_TYPE:
      // Will be created lazily at runtime.
      return isolate()->factory()->undefined_value();

    case ACCESSOR_PAIR_TYPE:
      // FIXME
      return isolate()->factory()->null_value();

    default: break;
  }

  UNREACHABLE();
  return Handle<Object>::null();
}


void LChunkSaverBase::SaveOddball(Oddball* oddball) {
  SavePrimitive<byte>(oddball->kind());
}


Handle<Oddball> LChunkLoaderBase::LoadOddball() {
  auto kind = LoadPrimitive<byte>();

  Factory* factory = isolate()->factory();

  switch (kind) {
    case Oddball::kUndefined: return factory->undefined_value();
    case Oddball::kTheHole: return factory->the_hole_value();
    case Oddball::kNull: return factory->null_value();
    case Oddball::kTrue: return factory->true_value();
    case Oddball::kFalse: return factory->false_value();
    case Oddball::kUninitialized: return factory->uninitialized_value();
    case Oddball::kException: return factory->exception();
  }

  UNREACHABLE();
  return Handle<Oddball>::null();
}


void LChunkSaverBase::SaveHeapNumber(HeapNumber* number) {
  SavePrimitive<double>(number->value());
}


Handle<HeapNumber> LChunkLoaderBase::LoadHeapNumber() {
  return isolate()->factory()->NewHeapNumber(LoadPrimitive<double>());
}


void LChunkSaverBase::SaveName(Name* name) {
  if (name->IsString()) {
    SavePrimitive<bool>(true);
    SaveString(String::cast(name));
  } else if (name->IsSymbol()) {
    SavePrimitive<bool>(false);
    SaveSymbol(Symbol::cast(name));
  } else {
    UNREACHABLE();
  }
}


Handle<Name> LChunkLoaderBase::LoadName() {
  bool is_string = LoadBool();
  if (is_string) {
    return LoadString();
  } else {
    return LoadSymbol();
  }
}


void LChunkSaverBase::SaveSymbol(Symbol* symbol) {
  Heap::RootListIndex root_index = isolate()->heap()->RootIndex(symbol);
  SavePrimitive<Heap::RootListIndex>(root_index);
  if (root_index != Heap::kNotFound) {
    // Root symbol.
    return;
  }

  // Can't allocate on heap during save, so we reimplement parts of
  // JSReceiver::GetKeys in-place.
  Handle<JSObject> registry = isolate()->GetSymbolRegistry();
  DCHECK(registry->HasFastProperties());

  // From GetEnumPropertyKeys in object.cc:
  for (int key_index = 0;
       key_index < registry->map()->NumberOfOwnDescriptors(); ++key_index) {
    PropertyDetails details =
      registry->map()->instance_descriptors()->GetDetails(key_index);
    Name* key =
      Name::cast(registry->map()->instance_descriptors()->GetKey(key_index));
    if (details.IsDontEnum() || key->IsSymbol()) {
      continue;
    }

    Handle<JSObject> table = Handle<JSObject>::cast(
        Object::GetProperty(registry, handle(key)).ToHandleChecked());
    DCHECK(!table->HasFastProperties());
    NameDictionary* names = table->property_dictionary();

    // From NameDictionary::CopyEnumKeysTo:
    for (int name_index = 0; name_index < names->Capacity(); ++name_index) {
      Object* name = names->KeyAt(name_index);
      if (!names->IsKey(name) || name->IsSymbol()) {
        continue;
      }

      PropertyDetails details = names->DetailsAt(name_index);
      if (details.IsDeleted() || details.IsDontEnum()) {
        continue;
      }

      Symbol* entry = *Handle<Symbol>::cast(
          Object::GetProperty(table, handle(Name::cast(name)))
          .ToHandleChecked());
      if (symbol == entry) {
        SaveString(String::cast(key));
        SaveString(String::cast(name));
        return;
      }
    }
  }

  // Symbol not found.
  UNREACHABLE();
}


Handle<Symbol> LChunkLoaderBase::LoadSymbol() {
  auto root_index = LoadPrimitive<Heap::RootListIndex>();
  if (root_index != Heap::kNotFound) {
    DCHECK(0 <= root_index && root_index < Heap::kNotFound);
    Object* root = isolate()->heap()->root(root_index);
    DCHECK(root->IsSymbol());
    return handle(Symbol::cast(root));
  }

  Handle<String> registry_key = LoadString();
  Handle<String> symbol_name = LoadString();

  Handle<JSObject> registry = isolate()->GetSymbolRegistry();
  Handle<Object> table = Handle<JSObject>::cast(
      Object::GetProperty(registry, registry_key).ToHandleChecked());
  Handle<Symbol> symbol = Handle<Symbol>::cast(
      Object::GetProperty(table, symbol_name).ToHandleChecked());
  return symbol;
}


void LChunkSaverBase::SaveString(String* string, int offset, int length) {
  int byte_length;
  SmartArrayPointer<char> str =
    string->ToCString(ALLOW_NULLS, FAST_STRING_TRAVERSAL,
                      offset, length, &byte_length);
  SavePrimitiveArray(Vector<const char>(str.get(), byte_length));

  SavePrimitive<bool>(string->IsInternalizedString());
}


Handle<String> LChunkLoaderBase::LoadString() {
  Vector<const char> str = LoadPrimitiveArray<const char>();

  bool internalize = LoadBool();
  return internalize
    ? isolate()->factory()->InternalizeUtf8String(str)
    : isolate()->factory()->NewStringFromUtf8(str).ToHandleChecked();
}


void LChunkSaverBase::SaveJSRegExp(JSRegExp* regexp) {
  SaveString(regexp->Pattern());

  JSRegExp::Flags flags = regexp->GetFlags();

  // The rest of this function saves `flags` as a String instance.
  // Couldn't find a reusable implementation of the conversion across V8.

  SavePrimitive<size_t>(flags.is_global() + flags.is_ignore_case() +
                        flags.is_multiline() + flags.is_sticky());

  if (flags.is_global()) {
    SavePrimitive<char>('g');
  }
  if (flags.is_ignore_case()) {
    SavePrimitive<char>('i');
  }
  if (flags.is_multiline()) {
    SavePrimitive<char>('m');
  }
  if (flags.is_sticky()) {
    SavePrimitive<char>('y');
  }

  // Not internalized.
  SaveFalse();
}


Handle<JSRegExp> LChunkLoaderBase::LoadJSRegExp() {
  Handle<String> pattern = LoadString();
  Handle<String> flags = LoadString();
  return Execution::NewJSRegExp(pattern, flags).ToHandleChecked();
}


void LChunkSaverBase::SaveFixedArrayBase(FixedArrayBase* array) {
  InstanceType type = array->map()->instance_type();
  SavePrimitive<InstanceType>(type);

  switch (type) {
    case FIXED_ARRAY_TYPE:
      SaveFixedArray(FixedArray::cast(array));
      return;

    case FIXED_DOUBLE_ARRAY_TYPE:
      SaveFixedDoubleArray(FixedDoubleArray::cast(array));
      return;

    default:
      UNREACHABLE();
  }
}


Handle<FixedArrayBase> LChunkLoaderBase::LoadFixedArrayBase() {
  auto type = LoadPrimitive<InstanceType>();

  switch (type) {
    case FIXED_ARRAY_TYPE: return LoadFixedArray();
    case FIXED_DOUBLE_ARRAY_TYPE: return LoadFixedDoubleArray();
    default: break;
  }

  UNREACHABLE();
  return Handle<FixedArrayBase>::null();
}


void LChunkSaverBase::SaveFixedArray(FixedArray* array) {
  DCHECK(!array->IsContext());
  SavePrimitive<int>(array->length());
  SaveFixedArrayData(array);
}


Handle<FixedArray> LChunkLoaderBase::LoadFixedArray() {
  auto length = LoadPrimitive<int>();
  Handle<FixedArray> array = isolate()->factory()->NewFixedArray(length);
  LoadFixedArrayData(*array);
  return array;
}


void LChunkSaverBase::SaveFixedArrayData(FixedArray* array) {
  for (int i = 0; i < array->length(); ++i) {
    Object* item = array->get(i);
    SaveObject(item);
    RETURN_ON_FAIL();
  }
}


void LChunkLoaderBase::LoadFixedArrayData(FixedArray* array) {
  for (int i = 0; i < array->length(); ++i) {
    Handle<Object> item = LoadObject();
    RETURN_ON_FAIL();
    array->set(i, !item.is_null() ? *item : nullptr);
  }
}


void LChunkSaverBase::SaveFixedDoubleArray(FixedDoubleArray* array) {
  SavePrimitive<int>(array->length());
  for (int i = 0; i < array->length(); ++i) {
    double item = array->get_scalar(i);
    SavePrimitive<double>(item);
  }
}


Handle<FixedDoubleArray> LChunkLoaderBase::LoadFixedDoubleArray() {
  auto length = LoadPrimitive<int>();

  auto array_base = isolate()->factory()->NewFixedDoubleArray(length);
  DCHECK(array_base->IsFixedDoubleArray());
  Handle<FixedDoubleArray> array = Handle<FixedDoubleArray>::cast(array_base);

  for (int i = 0; i < length; ++i) {
    auto item = LoadPrimitive<double>();
    array->set(i, item);
  }

  return array;
}


void LChunkSaverBase::SaveJSArray(JSArray* js_array) {
  SavePrimitive<ElementsKind>(js_array->map()->elements_kind());
  SaveObject(js_array->length());
  RETURN_ON_FAIL();

  if (js_array->length()) {
    SaveFixedArrayBase(js_array->elements());
  }
}


Handle<JSArray> LChunkLoaderBase::LoadJSArray() {
  auto elements_kind = LoadPrimitive<ElementsKind>();
  Handle<Object> maybe_length = LoadObject();
  RETURN_VALUE_ON_FAIL(Handle<JSArray>::null());

  if (!maybe_length.is_null()) {
    DCHECK(maybe_length->IsSmi());
    int length = Smi::cast(*maybe_length)->value();
    Handle<FixedArrayBase> elements = LoadFixedArrayBase();
    RETURN_VALUE_ON_FAIL(Handle<JSArray>::null());
    return isolate()->factory()->
      NewJSArrayWithElements(elements, elements_kind, length);
  } else {
    return isolate()->factory()->NewJSArray(elements_kind);
  }
}


void LChunkSaverBase::SaveJSArrayBuffer(JSArrayBuffer* buffer) {
  char* data = static_cast<char*>(buffer->backing_store());
  size_t size = Smi::cast(buffer->byte_length())->value();

  SavePrimitiveArray(Vector<const char>(data, size));
}


Handle<JSArrayBuffer> LChunkLoaderBase::LoadJSArrayBuffer() {
  Vector<const char> data = LoadPrimitiveArray<char>();

  Handle<JSArrayBuffer> buffer = isolate()->factory()->NewJSArrayBuffer();
  Runtime::SetupArrayBufferAllocatingData(isolate(), buffer,
                                          data.length(), false);
  memcpy(buffer->backing_store(), data.start(), data.length());

  return buffer;
}


void LChunkSaverBase::SaveJSTypedArray(JSTypedArray* typed_array) {
  SavePrimitive<ExternalArrayType>(typed_array->type());
  SaveJSArrayBuffer(*typed_array->GetBuffer());
  RETURN_ON_FAIL();

  SavePrimitive<size_t>(Smi::cast(typed_array->byte_offset())->value());
  SavePrimitive<size_t>(Smi::cast(typed_array->length())->value());
}


Handle<JSTypedArray> LChunkLoaderBase::LoadJSTypedArray() {
  auto type = LoadPrimitive<ExternalArrayType>();
  Handle<JSArrayBuffer> buffer = LoadJSArrayBuffer();
  auto byte_offset = LoadPrimitive<size_t>();
  auto length = LoadPrimitive<size_t>();
  return isolate()->factory()->NewJSTypedArray(type, buffer,
                                               byte_offset, length);
}


void LChunkSaverBase::SaveTypeFeedbackVector(TypeFeedbackVector* vector) {
  SavePrimitive<int>(vector->Slots());
  SavePrimitive<int>(vector->ICSlots());
}


Handle<TypeFeedbackVector> LChunkLoaderBase::LoadTypeFeedbackVector() {
  auto slot_count = LoadPrimitive<int>();
  auto ic_slot_count = LoadPrimitive<int>();
  return isolate()->factory()->NewTypeFeedbackVector(slot_count, ic_slot_count);
}


// Relocation types for JSFunction and SharedFunctionInfo.
enum FunctionRelocationType {
  // Common.
  kByBuiltinFunctionId,

  // SharedFunctionInfo only.
  kByPathFromRoot,

  // JSFunction only.
  kByNameInContextChain,
  kByNameInGlobalObject,
  kByStartPosition,
  kByOriginalName,
  kByGlobalConstructor,
  kByGlobalPrototype
};


// Not all BuiltinFunctionIds are of value for us.  This function checks if
// BuiltinFunctionId is "indexed", that is it has enough associated
// information about the function is encodes to uniquely identify that
// function.
static bool HasIndexedBuiltinFunctionId(SharedFunctionInfo* shared_info) {
  if (!shared_info->HasBuiltinFunctionId()) {
    return false;
  }
  int id = shared_info->builtin_function_id();

  // Assuming the "indexed" part starts at index 1.
#define PLUS_ONE(_0, _1, _2) +1
  int numberOfIds = FUNCTIONS_WITH_ID_LIST(PLUS_ONE);
#undef PLUS_ONE
  return 1 <= id && id < 1 + numberOfIds;
}


void LChunkSaverBase::SaveSharedFunctionInfo(SharedFunctionInfo* shared_info) {
  if (shared_info->native()) {
    DCHECK(HasIndexedBuiltinFunctionId(shared_info));
    SavePrimitive<FunctionRelocationType>(kByBuiltinFunctionId);
    SavePrimitive(shared_info->builtin_function_id());
    return;
  } else {
    SavePrimitive<FunctionRelocationType>(kByPathFromRoot);
    // Continue.
  }

  // Path to shared_info from the root of the SFI tree, in reverse order.
  List<int> path;
#ifdef DEBUG
  int path_length = 0;
#endif

  for (SharedFunctionInfo* parent_info = shared_info->outer_info(); parent_info;
       shared_info = parent_info, parent_info = parent_info->outer_info()) {
    for (int i = 0; i < parent_info->inner_infos()->length(); ++i) {
      SharedFunctionInfo* child_info = SharedFunctionInfo::cast(
          parent_info->inner_infos()->get(i));
      if (child_info->start_position() == shared_info->start_position()) {
        path.Add(i);
        break;
      }
    }

    DCHECK(++path_length == path.length());
  }

  // Write path backwards.
  for (int i = path.length() - 1; i >= 0; --i) {
    SavePrimitive<int>(path[i]);
  }
  SavePrimitive<int>(-1);
}


Handle<SharedFunctionInfo> LChunkLoaderBase::LoadSharedFunctionInfo() {
  auto relocation = LoadPrimitive<FunctionRelocationType>();
  switch (relocation) {
    case kByBuiltinFunctionId: {
      auto id = LoadPrimitive<BuiltinFunctionId>();
      switch (id) {
#define DEFINE_CASE(holder_expr, function_name, builtin)                \
        case k##builtin:                                                \
          return handle(ResolveBuiltinFunction(                         \
            isolate()->native_context(), #holder_expr, #function_name)  \
              ->shared());
        FUNCTIONS_WITH_ID_LIST(DEFINE_CASE)
#undef DEFINE_CASE
        default: break;
      }
      UNREACHABLE();
    }

    case kByPathFromRoot: break;
    default: UNREACHABLE();
  }

  DCHECK(relocation == kByPathFromRoot);

  SharedFunctionInfo* root_info = *info()->shared_info();
  while (root_info->outer_info()) {
    root_info = root_info->outer_info();
  }

  SharedFunctionInfo* shared_info = root_info;

  // Traverse down the SFI tree.
  for (int index = LoadPrimitive<int>(); index >= 0; index = LoadPrimitive<int>()) {
    DCHECK(index < shared_info->inner_infos()->length());
    auto inner_info = shared_info->inner_infos()->get(index);
    if (inner_info->IsUndefined()) {
      // TODO: Force FullCode-compilation instead.
      Fail("could not reach SharedFunctionInfo - may not be compiled yet");
      return Handle<SharedFunctionInfo>::null();
    }
    shared_info = SharedFunctionInfo::cast(inner_info);
  }

  return Handle<SharedFunctionInfo>(shared_info);
}


void LChunkSaverBase::SaveJSFunction(JSFunction* function) {
  // Check for builtin function ID.
  if (HasIndexedBuiltinFunctionId(function->shared())) {
    SavePrimitive<FunctionRelocationType>(kByBuiltinFunctionId);
    SavePrimitive(function->shared()->builtin_function_id());
    return;
  }

  // Try to save by name.
  if (function->shared()->name()->IsString()) {
    Handle<String> name(String::cast(function->shared()->name()));
    if (!name->IsInternalizedString()) {
      name = isolate()->factory()->InternalizeString(name);
    }
    if (name->length()) {
      // Check that this name is enough to recover.
      int slot_index;
      PropertyAttributes attributes;
      BindingFlags binding_flags;
      Handle<Object> container =
        info()->closure()->context()->Lookup(name, FOLLOW_CHAINS, &slot_index,
                                             &attributes, &binding_flags);
      if (!container.is_null() && container->IsContext()) {
        Object* slot = Context::cast(*container)->get(slot_index);
        if (slot == function) {
          SavePrimitive<FunctionRelocationType>(kByNameInContextChain);
          SaveString(*name);
          return;
        }
      } else {
        Handle<GlobalObject> global_object(isolate()->global_object());
        Handle<Object> object;
        if (Object::GetProperty(global_object, name).ToHandle(&object) &&
            *object == function) {
          SavePrimitive<FunctionRelocationType>(kByNameInGlobalObject);
          SaveString(*name);
          return;
        }
      }
    }
  }

  // Check for start position.
  //
  // TODO: Empty functions (native_context()->closure()) are saved by start
  // position, but not when compared and saved/loaded in a separate case
  // explicitly (segmentation fault, check out octane/code-load and
  // kraken/audio-beat-detection).
  if (!function->shared()->native() && !function->IsBuiltin()) {
    if (!function->context()->IsNativeContext()) {
      // Inner functions are impossible to recover by start position
      // since there may be many instances for the same position.

      Fail("refs to inner JSFs");
      return;
    }

    SavePrimitive<FunctionRelocationType>(kByStartPosition);
    SavePrimitive<int>(function->shared()->start_position());
    return;
  }

  // Search in global constructors and their prototypes.
  {
    // Note: we can't use JSReceiver::GetKeys because we can't allocate
    // anything on the heap during save.
    Handle<GlobalObject> global_object = isolate()->global_object();
    DCHECK(!global_object->HasFastProperties());
    NameDictionary* keys = global_object->property_dictionary();

    for (int key_index = 0; key_index < keys->Capacity(); ++key_index) {
      Handle<Object> key(keys->KeyAt(key_index), isolate());
      if (!keys->IsKey(*key) || key->IsSymbol()) {
        continue;
      }

      PropertyDetails details = keys->DetailsAt(key_index);
      // Global constructors (e.g. Array) are DontEnum, so we include them
      // by dropping this check here.
      if (details.IsDeleted()) {
        continue;
      }

      Handle<Object> entry =
        Object::GetProperty(global_object, Handle<Name>::cast(key))
        .ToHandleChecked();

      if (!entry->IsJSFunction()) {
        continue;
      }

      Handle<JSFunction> constructor = Handle<JSFunction>::cast(entry);

      // Check instance properties.
      Handle<DescriptorArray> instance_descriptors(
          constructor->map()->instance_descriptors());
      for (int index = 0;
           index < instance_descriptors->number_of_descriptors(); ++index) {
        Handle<Object> func(instance_descriptors->GetValue(index), isolate());
        if (func->IsJSFunction() &&
            Handle<JSFunction>::cast(func)->shared() == function->shared()) {
          SavePrimitive<FunctionRelocationType>(kByGlobalConstructor);
          SaveString(String::cast(*key));
          SavePrimitive<int>(index);
          return;
        }
      }

      // Check prototype properties.
      if (constructor->has_prototype() &&
          constructor->prototype()->IsJSObject()) {
        Handle<JSObject> prototype(JSObject::cast(constructor->prototype()));
        Handle<DescriptorArray> prototype_descriptors(
            prototype->map()->instance_descriptors());
        for (int index = 0;
             index < prototype_descriptors->number_of_descriptors();
             ++index) {
          Handle<Object> func(prototype_descriptors->GetValue(index),
                              isolate());
          if (func->IsJSFunction() &&
              Handle<JSFunction>::cast(func)->shared() == function->shared()) {
            SavePrimitive<FunctionRelocationType>(kByGlobalPrototype);
            SaveString(String::cast(*key));
            SavePrimitive<int>(index);
            return;
          }
        }
      }
    }
  }

  // Try to extract the original name from function sources.
  {
    auto script = Script::cast(function->shared()->script());
    Handle<String> source(String::cast(script->source()));

    int end_position = function->shared()->start_position();
    while (isspace(source->Get(end_position - 1))) {
      end_position -= 1;
    }

    int start_position = end_position;
    while (isalnum(source->Get(start_position - 1)) ||
           source->Get(start_position - 1) == '_') {
      start_position -= 1;
    }

    // ... function () { /*..*/ } ...
    bool is_function_token = false;
    if (end_position - start_position == 8) {
      is_function_token = true;
      for (int i = 0; i < 8; ++i) {
        if ("function"[i] != source->Get(start_position + i)) {
          is_function_token = false;
          break;
        }
      }
    }

    if (!is_function_token) {
      SavePrimitive<FunctionRelocationType>(kByOriginalName);
      SaveString(*source, start_position, end_position - start_position);
      return;
    }
  }

  Fail("could not save a JSF");
}


Handle<JSFunction> LChunkLoaderBase::LoadJSFunction() {
  auto relocation = LoadPrimitive<FunctionRelocationType>();

  switch (relocation) {
    case kByNameInContextChain: {
      Handle<String> name = LoadString();

      int slot_index;
      PropertyAttributes attributes;
      BindingFlags binding_flags;
      Handle<Object> container =
        info()->closure()->context()->Lookup(name, FOLLOW_CHAINS, &slot_index,
                                             &attributes, &binding_flags);
      DCHECK(!container.is_null() && container->IsContext());

      Handle<Object> slot(Context::cast(*container)->get(slot_index),
                          isolate());
      DCHECK(slot->IsJSFunction());
      return Handle<JSFunction>::cast(slot);
    }

    case kByNameInGlobalObject: {
      Handle<String> name = LoadString();
      Handle<GlobalObject> global_object(isolate()->global_object());
      Handle<Object> object =
        Object::GetProperty(global_object, name).ToHandleChecked();
      DCHECK(object->IsJSFunction());
      return Handle<JSFunction>::cast(object);
    }

    case kByStartPosition: {
      auto start_position = LoadPrimitive<int>();
      Handle<JSFunction> function =
          isolate()->GetJSFunctionByStartPosition(start_position);
      if (function.is_null()) {
        Fail("function not found by start position");
      }
      return function;
    }

    case kByBuiltinFunctionId: {
      auto id = LoadPrimitive<BuiltinFunctionId>();

      switch (id) {
#define DEFINE_CASE(holder_expr, function_name, builtin)                \
        case k##builtin:                                                \
          return ResolveBuiltinFunction(isolate()->native_context(),    \
                                        #holder_expr, #function_name);
        FUNCTIONS_WITH_ID_LIST(DEFINE_CASE)
#undef DEFINE_CASE
        default: UNREACHABLE();
      }
      break;
    }

    case kByGlobalConstructor:
    case kByGlobalPrototype: {
      Handle<String> key = LoadString();
      int descriptor_index = LoadPrimitive<int>();

      Handle<JSFunction> constructor = Handle<JSFunction>::cast(
          Object::GetProperty(isolate()->global_object(), key)
          .ToHandleChecked());
      Handle<DescriptorArray> descriptors(relocation == kByGlobalConstructor
          ? constructor->map()->instance_descriptors()
          : JSObject::cast(constructor->prototype())
              ->map()->instance_descriptors());

      DCHECK(descriptor_index < descriptors->number_of_descriptors());
      return Handle<JSFunction>(JSFunction::cast(
          descriptors->GetValue(descriptor_index)));
    }

    case kByOriginalName: {
      Handle<String> name = LoadString();
      Handle<GlobalObject> builtins(isolate()->js_builtins_object());
      LookupIterator lookup(builtins, name,
                            LookupIterator::OWN_SKIP_INTERCEPTOR);
      Handle<PropertyCell> cell = lookup.GetPropertyCell();
      DCHECK(cell->type()->IsConstant());
      Handle<Object> object = cell->type()->AsConstant()->Value();
      DCHECK(object->IsJSFunction());
      return handle(JSFunction::cast(*object));
    }

    default: UNREACHABLE();
  }

  return Handle<JSFunction>::null();
}


class LChunkSaverBase::MapSaver {
 public:
  MapSaver(LChunkSaverBase* saver)
      : saver_(saver) {}

  Isolate* isolate() { return saver_->isolate(); }

  void SaveMap(Map* map) {
    if (!CreateMapChain(map)) {
      DCHECK(saver_->LastStatus() == LChunkSaverBase::FAILED);
      return;
    }

    // Save root map.
    Map* root_map = chain_.last();
    auto root_index = GetStableRootIndex(root_map);
    saver_->SavePrimitive<Heap::RootListIndex>(root_index);
    if (root_index == Heap::kNotFound) {
      EnterMap(root_map);
      ExitMap(root_map);
    }

    // #transitions = #maps - 1
    saver_->SavePrimitive<int>(chain_.length() - 1);

    for (int i = chain_.length() - 1; i > 0; --i) {
      Map* parent = chain_[i];
      map = chain_[i - 1];

      if (saver_->LastStatus() != SUCCEEDED) {
        return;
      }
      EnterMap(map);

      if (!map->NumberOfOwnDescriptors()) {
        // Just copy the thing.
        saver_->SaveTrue();
        ExitMap(map);
        continue;
      }
      saver_->SaveFalse();

      DescriptorArray* descriptors = map->instance_descriptors();
      Name* key;

      if (parent->NumberOfOwnDescriptors() + 1 ==
          map->NumberOfOwnDescriptors()) {
        key = descriptors->GetKey(parent->NumberOfOwnDescriptors());
      } else {
        int transition_index = parent->SearchTransitionForTarget(handle(map));
        if (transition_index == TransitionArray::kNotFound) {
          // Transition to child map was overwritten, but we currently
          // have no idea how to recover it.
          UNIMPLEMENTED();
        }
        key = parent->transitions()->GetKey(transition_index);
      }

      if (TransitionArray::IsSpecialTransition(key)) {
        saver_->SaveTrue();
        saver_->SaveName(key);
        ExitMap(map);
        continue;
      }
      saver_->SaveFalse();

      int descriptor_index =
        descriptors->Search(key, map->NumberOfOwnDescriptors());

      DCHECK(descriptor_index != DescriptorArray::kNotFound &&
             parent->NumberOfOwnDescriptors() <= descriptor_index &&
             descriptor_index < map->NumberOfOwnDescriptors());
      saver_->SavePrimitive<int>(descriptor_index);
      ExitMap(map);
    }
  }

 private:
  bool CreateMapChain(Map* map) {
    for (Object* next = map; next->IsMap(); next = map->GetBackPointer()) {
      map = Map::cast(next);
      chain_.Add(map);

      // Saving cyclic map chains is possible but results in too much of
      // an overhead for the normal case.
      if (saver_->map_cache_.Contains(map)) {
        saver_->Fail("cyclic map chains");
        return false;
      }
    }

    return true;
  }

  Heap::RootListIndex GetStableRootIndex(Object* object) {
    Heap::RootListIndex index = isolate()->heap()->RootIndex(object);

    // Ignore cache roots.
    if (index != Heap::kNotFound &&
        ((Heap::kNumberStringCacheRootIndex <= index &&
          index <= Heap::kRegExpMultipleCacheRootIndex) ||
         index == Heap::kNonMonomorphicCacheRootIndex ||
         index == Heap::kPolymorphicCodeCacheRootIndex ||
         index == Heap::kNativesSourceCacheRootIndex)) {
      return Heap::kNotFound;
    }

    return index;
  }

  void EnterMap(Map* map) {
    // Push this map to the stack, so that we can detect cycles between maps.
    saver_->map_cache_.Add(map);

    SavePropertiesOnEnter(map);
  }

  // To save a map:
  //   1. Register map in the map cache.
  //   2. Save essential properties (mainly integers);
  //   3. Save map layout (a path down an implicit decision tree);
  //   4. Complete saving properties; on this stage recursive save-maps can
  //      be triggered and references to previously saved maps might occur.
  //
  // To load a map:
  //   1. Load essential properties (integers).
  //   2. Load map layout following LoadMap's algorithm.
  //   3. Create a map and register it in the map cache.
  //   4. Complete loading properties. Resolve references to previously
  //      loaded maps (including the one still being loaded!).
  //
  // It is crucical to get the new map in the cache before step 4, and for
  // that we need some properties loaded to create that map with. This is the
  // main reason properties are split into two groups.
  //
  // Also, at step 1 properties are loaded into a separate set of variables,
  // and we need some postprocessing to set these properties on a new map.
  //
  void SavePropertiesOnEnter(Map* map) {
    saver_->Synchronize();

    saver_->SavePrimitive<InstanceType>(map->instance_type());
    saver_->SavePrimitive<int>(map->instance_size());
    saver_->SavePrimitive<int>(map->inobject_properties());
    saver_->SavePrimitive<int>(map->unused_property_fields());
    saver_->SavePrimitive<byte>(map->bit_field());
    saver_->SavePrimitive<byte>(map->bit_field2());
    saver_->SavePrimitive<uint32_t>(map->bit_field3());

    saver_->SavePrimitive<int>(map->NumberOfOwnDescriptors());
    DescriptorArray* descriptors = map->instance_descriptors();

    for (int i = 0; i < map->NumberOfOwnDescriptors(); ++i) {
      saver_->SaveName(descriptors->GetKey(i));
      saver_->SavePrimitive<PropertyDetails>(descriptors->GetDetails(i));
    }
  }

  void ExitMap(Map* map) {
    SavePropertiesOnExit(map);

    DCHECK(saver_->map_cache_.last() == map);
    saver_->map_cache_.RemoveLast();
  }

  void SavePropertiesOnExit(Map* map) {
    for (int i = 0; i < map->NumberOfOwnDescriptors(); ++i) {
      saver_->SaveObject(map->instance_descriptors()->GetValue(i));
    }

    // Missing prototypes cause obscure errors due to missing toString
    // and valueOf.  Restored constructors and prototypes also facilitate
    // replacing created maps with existing maps.

    if (map->constructor()->IsJSFunction()) {
      Handle<JSFunction> constructor(JSFunction::cast(map->constructor()));
      if (constructor->has_prototype() &&
          constructor->prototype() == map->prototype()) {
        saver_->SaveTrue();
        saver_->SaveJSFunction(*constructor);
        return;
      }
    }

    // Failed to save the constructor.
    saver_->SaveFalse();
    saver_->SaveObject(map->prototype());
  }

  LChunkSaverBase* saver_;
  List<Map*> chain_;  // Chain of maps, from the leaf map up to the root.
};


void LChunkSaverBase::SaveMap(Map* map) {
  MapSaver(this).SaveMap(map);
  Synchronize();
}


class LChunkLoaderBase::MapLoader {
 public:
  MapLoader(LChunkLoaderBase* loader)
      : loader_(loader),
        has_branched_(false) {}

  Isolate* isolate() { return loader_->isolate(); }
  CompilationInfo* info() { return loader_->info(); }

  MaybeHandle<Map> LoadMap() {
    Handle<Map> map;

    auto root_index = loader_->LoadPrimitive<Heap::RootListIndex>();
    if (root_index != Heap::kNotFound) {
      // Load first map from the roots.
      Object* root = isolate()->heap()->root(root_index);
      DCHECK(root->IsMap());
      map = handle(Map::cast(root));
    } else {
      // Create new root map.
      LoadPropertiesOnEnter();
      ElementsKind elements_kind = Map::ElementsKindBits::decode(bit_field2_);
      map = isolate()->factory()->
          NewMap(instance_type_, instance_size_, elements_kind);
      LoadPropertiesOnExit(map);
      if (loader_->LastStatus() != SUCCEEDED) return MaybeHandle<Map>();
      has_branched_ = true;
    }

    // Start transitioning down to the leaf map.
    auto number_of_map_transitions = loader_->LoadPrimitive<int>();
    for (int map_transition_index = 0;
         map_transition_index < number_of_map_transitions;
         ++map_transition_index) {

      LoadPropertiesOnEnter();

      bool copy_and_drop = loader_->LoadBool();
      if (copy_and_drop) {
        Handle<Map> child = Map::CopyDropDescriptors(map);
        DCHECK(!map->is_prototype_map());
        child->SetBackPointer(*map);
        map = child;
        LoadPropertiesOnExit(map);
        if (loader_->LastStatus() != SUCCEEDED) return MaybeHandle<Map>();
        has_branched_ = true;
        continue;
      }

      bool is_special_transition = loader_->LoadBool();
      if (is_special_transition) {
        Handle<Name> key = loader_->LoadName();
        DCHECK(TransitionArray::IsSpecialTransition(*key));

        Handle<Symbol> symbol = Handle<Symbol>::cast(key);
        int transition_index = map->SearchSpecialTransition(*symbol);

        if (transition_index != DescriptorArray::kNotFound) {
          map = handle(map->GetTransition(transition_index));
        } else {
          Handle<Map> child = Map::CopyDropDescriptors(map);
          Map::ConnectTransition(map, child, symbol, SPECIAL_TRANSITION);
          map = child;
          has_branched_ = true;
        }
        LoadPropertiesOnExit(map);
        if (loader_->LastStatus() != SUCCEEDED) return MaybeHandle<Map>();
        continue;
      }

      auto transition_descriptor_index = loader_->LoadPrimitive<int>();
      Handle<LayoutDescriptor> layout_descriptor(
          LayoutDescriptor::FastPointerLayout(), isolate());
      Handle<Name> transition_key(
          descriptors_->GetKey(transition_descriptor_index));
      map = Map::CopyReplaceDescriptors(map, descriptors_,
                                        layout_descriptor, INSERT_TRANSITION,
                                        transition_key, "LoadMap",
                                        SIMPLE_PROPERTY_TRANSITION);
      LoadPropertiesOnExit(map);
      if (loader_->LastStatus() != SUCCEEDED) return MaybeHandle<Map>();
    }

    if (FLAG_trace_saveload) {
      PrintF("[map loaded, function: ");
      info()->shared_info()->ShortPrint();
      PrintF(" at %d]\n", info()->shared_info()->start_position());
    }

    Handle<Map> deduplicated;
    if (TryDeduplicate(map).ToHandle(&deduplicated)) {
      return deduplicated;
    } else {
      return map;
    }
  }

 private:
  void LoadPropertiesOnEnter() {
    loader_->Synchronize();

    instance_type_ = loader_->LoadPrimitive<InstanceType>();
    instance_size_ = loader_->LoadPrimitive<int>();
    inobject_properties_ = loader_->LoadPrimitive<int>();
    unused_property_fields_ = loader_->LoadPrimitive<int>();
    bit_field_ = loader_->LoadPrimitive<byte>();
    bit_field2_ = loader_->LoadPrimitive<byte>();
    bit_field3_ = loader_->LoadPrimitive<uint32_t>();

    int number_of_descriptors = loader_->LoadPrimitive<int>();

    if (number_of_descriptors) {
      descriptors_ = DescriptorArray::Allocate(isolate(),
                                              number_of_descriptors);

      for (int descriptor_number = 0;
           descriptor_number < number_of_descriptors; ++descriptor_number) {
        // We don't load values yet, in order not to trigger recursive map
        // loads.
        Handle<Name> key = loader_->LoadName();
        Handle<Object> value =
            isolate()->factory()->NewRawOneByteString(0).ToHandleChecked();
        PropertyDetails details = loader_->LoadPrimitive<PropertyDetails>();

        Descriptor descriptor(key, value, details);
        descriptors_->Set(descriptor_number, &descriptor);
      }

      // Sort() works on keys, so the order won't change.
      descriptors_->Sort();
    }
    else {
      descriptors_ = isolate()->factory()->empty_descriptor_array();
    }
  }

  void LoadPropertiesOnExit(Handle<Map> map) {
    map->set_instance_type(instance_type_);
    map->set_instance_size(instance_size_);
    map->set_inobject_properties(inobject_properties_);
    map->set_unused_property_fields(unused_property_fields_);
    map->set_bit_field(bit_field_);
    map->set_bit_field2(bit_field2_);
    map->set_bit_field3(bit_field3_);

    for (int descriptor_number = 0;
         descriptor_number < map->NumberOfOwnDescriptors();
         ++descriptor_number) {
      Handle<Object> value = loader_->LoadObject();
      if (loader_->LastStatus() != SUCCEEDED) return;
      descriptors_->SetValue(descriptor_number, *value);
    }

    map->set_instance_descriptors(*descriptors_);
    map->SetNumberOfOwnDescriptors(descriptors_->number_of_descriptors());

    bool has_constructor = loader_->LoadBool();
    MaybeHandle<Object> maybe_constructor, maybe_prototype;

    if (has_constructor) {
      Handle<JSFunction> constructor_fn = loader_->LoadJSFunction();
      if (loader_->LastStatus() != SUCCEEDED) return;
      if (!constructor_fn.is_null() && constructor_fn->has_prototype()) {
        DCHECK(constructor_fn->prototype()->IsJSObject());
        maybe_constructor = constructor_fn;
        maybe_prototype =
            handle(JSObject::cast(constructor_fn->prototype()));
      }
    }
    else {
      maybe_prototype = loader_->LoadObject();
      if (loader_->LastStatus() != SUCCEEDED) return;
    }

    if (!maybe_constructor.is_null()) {
      Handle<Object> constructor = maybe_constructor.ToHandleChecked();
      map->set_constructor(*constructor);
    }
    if (!maybe_prototype.is_null()) {
      Handle<Object> prototype = maybe_prototype.ToHandleChecked();
      map->set_prototype(*prototype);
    }
  }

  // Try to deduplicate maps by replacing new map with some preexisting map.
  MaybeHandle<Map> TryDeduplicate(Handle<Map> map) {
    if (!has_branched_) {
      // Deduplication is not needed.
      return map;
    }

    Handle<FixedArray> keys =
      JSReceiver::GetKeys(isolate()->global_object(),
                          JSReceiver::OWN_ONLY).ToHandleChecked();
    for (int i = 0; i < keys->length(); ++i) {
      Handle<String> key(String::cast(keys->get(i)));
      Handle<Object> property =
        Object::GetProperty(isolate()->global_object(), key)
        .ToHandleChecked();

      if (!property->IsHeapObject()) {
        continue;
      }
      Handle<Map> property_map(HeapObject::cast(*property)->map());

      if (map->EquivalentToForDeduplication(*property_map)) {
        if (FLAG_trace_saveload) {
          PrintF("[deduplicate map, function: ");
          info()->shared_info()->ShortPrint();
          PrintF(" at %d]\n", info()->shared_info()->start_position());
        }
        return property_map;
      }
    }

    // Deduplication failed.
    return MaybeHandle<Map>();
  }

  // Properties.
  InstanceType instance_type_;
  int instance_size_;
  int inobject_properties_;
  int unused_property_fields_;
  byte bit_field_;
  byte bit_field2_;
  uint32_t bit_field3_;
  Handle<DescriptorArray> descriptors_;

  LChunkLoaderBase* loader_;
  // Whether new root map or some intermediate maps were created
  // during the process of loading the map.
  bool has_branched_;
};


Handle<Map> LChunkLoaderBase::LoadMap() {
  Handle<Map> map;
  if (MapLoader(this).LoadMap().ToHandle(&map)) {
    Synchronize();
    return map;
  }

  Fail("deduplication failed");
  return Handle<Map>::null();
}


void LChunkSaverBase::SaveJSObject(JSObject* js_object) {
  DCHECK(js_object->HasFastProperties());
  RETURN_ON_FAIL(SaveMap(js_object->map()));
  RETURN_ON_FAIL(SaveFixedArrayBase(js_object->elements()));
  RETURN_ON_FAIL(SaveFixedArray(js_object->properties()));
}


Handle<JSObject> LChunkLoaderBase::LoadJSObject() {
  Handle<Map> map = LoadMap();
  RETURN_VALUE_ON_FAIL(Handle<JSObject>::null());
  Handle<JSObject> js_object =
    isolate()->factory()->NewJSObjectFromMap(map, NOT_TENURED, false);

  Handle<FixedArrayBase> elements = LoadFixedArrayBase();
  RETURN_VALUE_ON_FAIL(Handle<JSObject>::null());
  js_object->set_elements(*elements);

  Handle<FixedArray> properties = LoadFixedArray();
  RETURN_VALUE_ON_FAIL(Handle<JSObject>::null());
  js_object->set_properties(*properties);

  return js_object;
}


void LChunkSaverBase::SaveJSValue(JSValue* object) {
  SaveJSObject(object);
  SaveObject(object->value());
}


Handle<JSValue> LChunkLoaderBase::LoadJSValue() {
  // LoadJSObject allocates enough memory for JSValue instance based on
  // its map's instance_size.
  Handle<JSValue> object = Handle<JSValue>::cast(LoadJSObject());
  RETURN_VALUE_ON_FAIL(Handle<JSValue>::null());
  Handle<Object> value = LoadObject();
  RETURN_VALUE_ON_FAIL(Handle<JSValue>::null());
  object->set_value(*value);
  return object;
}


void LChunkSaverBase::SaveExecutableAccessorInfo(ExecutableAccessorInfo* info) {
  SaveObject(info->getter());
  SaveObject(info->setter());
  SaveObject(info->data());
}


Handle<ExecutableAccessorInfo> LChunkLoaderBase::LoadExecutableAccessorInfo() {
  Handle<ExecutableAccessorInfo> info =
    isolate()->factory()->NewExecutableAccessorInfo();

  Handle<Object> getter = LoadObject();
  RETURN_VALUE_ON_FAIL(Handle<ExecutableAccessorInfo>::null());
  info->set_getter(*getter);

  Handle<Object> setter = LoadObject();
  RETURN_VALUE_ON_FAIL(Handle<ExecutableAccessorInfo>::null());
  info->set_setter(*setter);

  Handle<Object> data = LoadObject();
  RETURN_VALUE_ON_FAIL(Handle<ExecutableAccessorInfo>::null());
  info->set_data(*data);

  return info;
}


void LChunkSaverBase::SaveForeign(Foreign* foreign) {
  intptr_t addr = reinterpret_cast<intptr_t>(foreign->foreign_address());
  Accessors::DescriptorId accessor_id = Accessors::descriptorCount;

  do {
#define DEFINE_BRANCH(name)                                             \
    if (addr == reinterpret_cast<intptr_t>(Accessors::name##Getter)) {  \
      accessor_id = Accessors::k##name##Getter;                         \
      break;                                                            \
    }                                                                   \
    if (addr == reinterpret_cast<intptr_t>(Accessors::name##Setter)) {  \
      accessor_id = Accessors::k##name##Setter;                         \
      break;                                                            \
    }
    ACCESSOR_INFO_LIST(DEFINE_BRANCH)
#undef DEFINE_BRANCH
  } while (false);

  if (accessor_id == Accessors::descriptorCount) {
    // Not an accessor.
    UNREACHABLE();
  }

  SavePrimitive<Accessors::DescriptorId>(accessor_id);
}


Handle<Foreign> LChunkLoaderBase::LoadForeign() {
  auto accessor_id = LoadPrimitive<Accessors::DescriptorId>();
  intptr_t addr;

  switch (accessor_id) {
#define DEFINE_CASE(name)                                               \
    case Accessors::k##name##Getter:                                    \
      addr = reinterpret_cast<intptr_t>(Accessors::name##Getter);       \
      break;                                                            \
    case Accessors::k##name##Setter:                                    \
      addr = reinterpret_cast<intptr_t>(Accessors::name##Setter);       \
      break;
    ACCESSOR_INFO_LIST(DEFINE_CASE)
#undef DEFINE_CASE
    default: UNREACHABLE();
  }

  return isolate()->factory()->NewForeign(reinterpret_cast<Address>(addr));
}


void LChunkSaverBase::SaveRepresentation(Representation representation) {
  SavePrimitive<Representation::Kind>(representation.kind());
}


Representation LChunkLoaderBase::LoadRepresentation() {
  auto kind = LoadPrimitive<Representation::Kind>();
  return Representation::FromKind(kind);
}


void LChunkSaverBase::SaveHType(HType htype) {
  SavePrimitive(static_cast<HType::Kind>(htype.kind_));
}


HType LChunkLoaderBase::LoadHType() {
  auto kind = LoadPrimitive<HType::Kind>();
  return HType(kind);
}


void LChunkSaverBase::SaveLOperand(LOperand* operand) {
  SavePrimitive<unsigned>(operand->value_);
}


LOperand* LChunkLoaderBase::LoadLOperand() {
  auto operand =  new(zone()) LOperand();
  operand->value_ = LoadPrimitive<unsigned>();
  return operand;
}


void LChunkSaverBase::ConditionallySaveLOperand(LOperand* operand) {
  if (operand) {
    SaveTrue();
    SaveLOperand(operand);
  } else {
    SaveFalse();
  }
}


LOperand* LChunkLoaderBase::ConditionallyLoadLOperand() {
  return LoadBool() ? LoadLOperand() : nullptr;
}


void LChunkSaverBase::SavePointerMap(LPointerMap* pointer_map) {
  SavePrimitive<int>(pointer_map->pointer_operands_.length());
  for (LOperand* operand: pointer_map->pointer_operands_) {
    ConditionallySaveLOperand(operand);
  }

  SavePrimitive<int>(pointer_map->untagged_operands_.length());
  for (LOperand* operand: pointer_map->untagged_operands_) {
    ConditionallySaveLOperand(operand);
  }

  SavePrimitive<int>(pointer_map->lithium_position_);
}


LPointerMap* LChunkLoaderBase::LoadPointerMap() {
  auto pointer_map = new(zone()) LPointerMap(zone());

  auto number_of_pointer_operands = LoadPrimitive<int>();
  for (int i = 0; i < number_of_pointer_operands; ++i) {
    LOperand* item = ConditionallyLoadLOperand();
    pointer_map->pointer_operands_.Add(item, zone());
  }

  auto number_of_untagged_operands = LoadPrimitive<int>();
  for (int i = 0; i < number_of_untagged_operands; ++i) {
    LOperand* item = ConditionallyLoadLOperand();
    pointer_map->untagged_operands_.Add(item, zone());
  }

  pointer_map->lithium_position_ = LoadPrimitive<int>();
  return pointer_map;
}


void LChunkLoaderBase::InitializeChunk() {
  chunk_ = new(zone()) LPlatformChunk(info());
}


void LChunkSaverBase::SaveEnvironment(LEnvironment* env) {
  if (env->outer()) {
    // TODO: Save common outer environments only once.
    SaveTrue();
    RETURN_ON_FAIL(SaveEnvironment(env->outer()));
  } else {
    SaveFalse();
  }

  SavePrimitive(env->frame_type());
  SavePrimitive(env->arguments_stack_height());
  SavePrimitive<int>(env->ast_id().ToInt());
  SavePrimitive(env->translation_size());
  SavePrimitive(env->parameter_count());
  SavePrimitive(env->has_been_used());

  if (env->closure().is_identical_to(info()->closure())) {
    SaveTrue();
  } else {
    SaveFalse();
    SaveJSFunction(*env->closure());
  }

  // Save values.
  int number_of_values = env->values()->length();
  SavePrimitive(number_of_values);
  for (int i = 0; i < number_of_values; ++i) {
    ConditionallySaveLOperand(env->values()->at(i));
    SavePrimitive(env->HasTaggedValueAt(i));
    SavePrimitive(env->HasUint32ValueAt(i));
  }

  // Save object mapping.
  SavePrimitive(env->object_mapping_.length());
  for (uint32_t value: env->object_mapping_) {
    SavePrimitive(value);
  }
}


LEnvironment* LChunkLoaderBase::LoadEnvironment() {
  LEnvironment* outer = nullptr;
  if (LoadBool()) {
    outer = LoadEnvironment();
  }

  FrameType frame_type = LoadPrimitive<FrameType>();
  int arguments_stack_height = LoadPrimitive<int>();
  BailoutId ast_id = BailoutId(LoadPrimitive<int>());
  int translation_size = LoadPrimitive<int>();
  int parameter_count = LoadPrimitive<int>();
  bool has_been_used = LoadPrimitive<bool>();

  Handle<JSFunction> closure = LoadBool()
    ? info()->closure()
    : LoadJSFunction();
  RETURN_VALUE_ON_FAIL(nullptr);

  LEnvironment* env =
    new(zone()) LEnvironment(closure,
                             frame_type,
                             ast_id,
                             parameter_count,
                             arguments_stack_height,
                             translation_size,
                             outer,
                             nullptr,
                             zone());

  if (has_been_used) {
    env->set_has_been_used();
  }

  // Load values.
  int number_of_values = LoadPrimitive<int>();
  for (int i = 0; i < number_of_values; ++i) {
    LOperand* value = ConditionallyLoadLOperand();
    Representation representation =
      LoadPrimitive<bool>()
      ? Representation::Tagged()
      : Representation::None();
    bool is_uint32 = LoadPrimitive<bool>();
    env->AddValue(value, representation, is_uint32);
  }

  // Load object mapping.
  int object_mapping_size = LoadPrimitive<int>();
  for (int i = 0; i < object_mapping_size; ++i) {
    auto value = LoadPrimitive<uint32_t>();
    env->object_mapping_.Add(value, zone());
  }

  return env;
}

} }  // namespace v8::internal
