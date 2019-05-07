#ifndef INTERNAL_PMARRAYBUFFER_H
#define INTERNAL_PMARRAYBUFFER_H

#include <stddef.h>
#include <sys/stat.h>
#include <memory>

#include "memorymanager.h"

namespace internal {
class PMArrayBuffer {
 public:
  PMArrayBuffer(MemoryManager *mm, void *data);
  PMArrayBuffer(MemoryManager *mm, void *data, uint32_t length);
  PMArrayBuffer &operator=(const PMArrayBuffer &other) = delete;

  std::shared_ptr<const void> getPPtr();
  void *getBuffer();
  uint32_t getLength();
  void persist(uint32_t offset, uint32_t length);
  void snapshot(uint32_t offset, uint32_t length);

  void _deallocate();

 private:
  MemoryManager *_mm;
  PArrayBufferObject *_pab;

  PPtr _pptr;
};
}
#endif