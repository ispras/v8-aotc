#include "src/code-block-database.h"

#include "src/flags.h"
#include "src/list-inl.h"
#include "src/saveload.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

void CodeBlockDatabase::Read(const char* filename) {
  // We'll leave the bright idea of loading the database from
  // several files until the next time.
  DCHECK(!source_);
  source_ = filename;

  bool exists;
  buffer_ = ReadFile(filename, &exists, true);
  CHECK(exists);
  const char* read_pointer = buffer_.start();

  auto number_of_blocks = LoadPrimitive<size_t>(&read_pointer);
  code_blocks_.Allocate(number_of_blocks);

  for (size_t i = 0; i < number_of_blocks; ++i) {
    DCHECK(read_pointer + 2 * sizeof(int) <= buffer_.end());
    auto start_position = LoadPrimitive<int>(&read_pointer);
    auto size = LoadPrimitive<size_t>(&read_pointer);

    DCHECK(read_pointer + size <= buffer_.end());
    auto start = read_pointer - buffer_.start();
    Vector<const char> code = buffer_.SubVector(start, start + size);
    code_blocks_[i] = CodeBlock(start_position, code, false);
    read_pointer += size;
  }
}


void CodeBlockDatabase::Write(const char* filename) const {
  List<char> data;
  SavePrimitive<size_t>(data, code_blocks_.length());

  for (int i = 0; i < code_blocks_.length(); ++i) {
    SavePrimitive<int>(data, code_blocks_[i].StartPosition());
    SavePrimitive<size_t>(data, code_blocks_[i].Code().length());
    data.AddAll(code_blocks_[i].Code());
  }

  Vector<const char> buffer = data.ToConstVector();
  WriteChars(filename, buffer.start(), buffer.length(), true);

  if (FLAG_trace_saveload) {
    PrintF("[code block database saved to \"%s\"]\n", filename);
  }
}


void CodeBlockDatabase::SetCode(int start_position, Vector<const char> code) {
  for (CodeBlock& code_block: code_blocks_) {
    if (code_block.StartPosition() == start_position) {
      code_block.SetCode(code, true);
      return;
    }
  }
  code_blocks_.Add({ start_position, code, true });
}


bool CodeBlockDatabase::HasCode(int start_position) const {
  for (CodeBlock& code_block: code_blocks_) {
    if (code_block.StartPosition() == start_position) {
      return true;
    }
  }
  return false;
}


Vector<const char> CodeBlockDatabase::GetCode(int start_position) const {
  for (CodeBlock& code_block: code_blocks_) {
    if (code_block.StartPosition() == start_position) {
      return code_block.Code();
    }
  }
  UNREACHABLE();
  return Vector<const char>();
}


bool CodeBlockDatabase::RemoveCode(int start_position) {
  for (int index = 0; index < code_blocks_.length(); ++index) {
    if (code_blocks_[index].StartPosition() == start_position) {
      code_blocks_.Remove(index);
      return true;
    }
  }
  return false;
}

} }  // namespace v8::internal
