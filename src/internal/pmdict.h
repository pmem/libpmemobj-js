#ifndef INTERNAL_PMDICT_H
#define INTERNAL_PMDICT_H

#include <stddef.h>
#include <sys/stat.h>
#include <list>
#include <memory>

#include "memorymanager.h"

namespace internal {
namespace impl {
class PMDict {
 public:
  PMDict(MemoryManager* mm, PPtr data);
  PMDict(MemoryManager* mm);

  PPtr getPPtr();
  void setProperty(std::string key, PPtr value, snapshotFlag flag = kSnapshot);
  PPtr getProperty(std::string key);
  void delProperty(std::string key, snapshotFlag flag = kSnapshot);
  std::list<std::shared_ptr<const void>> getPropertyNames();
  void _deallocate();

 private:
  PPtr newKeysObject(uint64_t size);
  uint64_t fixedHash(const char* key);
  PDictKeysObject* getKeys();
  PDictKeyEntry* lookup(const char* key, uint64_t khash);
  void insertionResize();
  uint64_t usableFraction(uint64_t size);
  uint64_t growRate();
  PDictKeyEntry* findEmptySlot(uint64_t khash);

  MemoryManager* _mm;
  PDictObject* _pdict;
  PPtr _pptr;
};
}
}

#endif