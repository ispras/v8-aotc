#ifndef V8_SAVELOAD_H_
#define V8_SAVELOAD_H_

#include "src/list.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

template<typename T> void SavePrimitive(List<char>&, const T);


template<typename T>
T LoadPrimitive(const char** bytes) {
  T value = *(reinterpret_cast<const T*>(*bytes));
  *bytes += sizeof(T);
  return value;
}


template<typename T>
void SavePrimitiveArray(List<char>& bytes, Vector<const T> array,
                        bool with_length = true) {
  if (with_length) {
    SavePrimitive<size_t>(bytes, array.length());
  }
  bytes.AddAll(Vector<const char>(
    reinterpret_cast<const char*>(array.start()),
    sizeof(T) * array.length()
  ));
}


template<typename T>
void SavePrimitive(List<char>& bytes, const T value) {
  SavePrimitiveArray(bytes, Vector<const T>(&value, 1), false);
}


template<typename T>
Vector<const T> LoadPrimitiveArray(const char** bytes) {
  auto length = LoadPrimitive<size_t>(bytes);
  Vector<const T> array(reinterpret_cast<const T*>(*bytes), length);
  *bytes += sizeof(T) * length;
  return array;
}


inline void SaveTrue(List<char>& bytes) {
  bytes.Add(1);
}


inline void SaveFalse(List<char>& bytes) {
  bytes.Add(0);
}


inline bool LoadBool(const char** bytes) {
  return LoadPrimitive<char>(bytes);
}

} }  // namespace v8::internal

#endif  // V8_SAVELOAD_H_
