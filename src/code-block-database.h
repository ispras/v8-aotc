#ifndef V8_CODE_BLOCK_DATABASE_H_
#define V8_CODE_BLOCK_DATABASE_H_

#include "src/list.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

class CodeBlockDatabase {
 public:
  CodeBlockDatabase(const char* filename = nullptr)
      : source_(nullptr) {
    if (filename) {
      Read(filename);
    }
  }

  ~CodeBlockDatabase() {
    for (CodeBlock& code_block: code_blocks_) {
      code_block.DisposeIfNeeded();
    }
    buffer_.Dispose();
  }

  const char* Source() const { return source_; }

  void Read(const char* filename);
  void Write(const char* filename) const;

  void SetCode(int start_position, Vector<const char> code);
  bool HasCode(int start_position) const;
  Vector<const char> GetCode(int start_position) const;
  bool RemoveCode(int start_position);

 private:
  class CodeBlock {
   public:
    CodeBlock(int start_position,
              Vector<const char> code,
              bool managed)
        : managed_(managed),
          start_position_(start_position),
          code_(code) {}

    int StartPosition() const { return start_position_; }
    Vector<const char> Code() const { return code_; }

    void SetCode(Vector<const char> code, bool managed) {
      DisposeIfNeeded();
      managed_ = managed;
      code_ = code;
    }

    void DisposeIfNeeded() {
      if (managed_) {
        code_.Dispose();
      }
    }

   private:
    bool managed_;
    int start_position_;
    Vector<const char> code_;
  };

  const char* source_;
  List<CodeBlock> code_blocks_;
  Vector<const char> buffer_;

  DISALLOW_COPY_AND_ASSIGN(CodeBlockDatabase);
};

} }  // namespace v8::internal

#endif  // V8_CODE_BLOCK_DATABASE_H_
