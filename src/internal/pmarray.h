#ifndef INTERNAL_PMELEMENTS_H
#define INTERNAL_PMELEMENTS_H

#include <stddef.h>
#include <sys/stat.h>
#include <list>
#include <memory>

#include "memorymanager.h"

namespace internal {
namespace impl {

class PMArray {
 public:
  virtual ~PMArray() = 0;
  virtual PPtr getPPtr() = 0;
  virtual void setProperty(uint32_t index, PPtr value_pptr,
                           snapshotFlag flag = kSnapshot) = 0;
  virtual PPtr getProperty(uint32_t index) = 0;
  virtual void delProperty(uint32_t index, snapshotFlag flag = kSnapshot) = 0;
  virtual std::list<uint32_t> getValidIndex() = 0;
  virtual void push(PPtr value_pptr, snapshotFlag flag = kSnapshot) = 0;
  virtual std::shared_ptr<const void> pop(snapshotFlag flag = kSnapshot) = 0;
  virtual uint32_t getLength() = 0;
  virtual void setLength(uint32_t new_length) = 0;
  virtual void _deallocate() = 0;

  virtual bool shouldConvertToNumDict(uint32_t index) { return false; };
  virtual bool shouldConvertToSimpleArray(uint32_t index) { return false; };
  virtual void* convertToSimpleArray() { return nullptr; };
  virtual void* convertToNumDict() { return nullptr; };

 private:
  MemoryManager* _mm;
  PPtr _pptr;
};

class PMSimpleArray : public PMArray {
 public:
  PMSimpleArray(MemoryManager* mm);
  PMSimpleArray(MemoryManager* mm, PPtr pptr);
  ~PMSimpleArray(){};
  PPtr getPPtr();
  void setProperty(uint32_t index, PPtr value_pptr,
                   snapshotFlag flag = kSnapshot);
  PPtr getProperty(uint32_t index);
  void delProperty(uint32_t index, snapshotFlag flag = kSnapshot);
  std::list<uint32_t> getValidIndex();
  void push(PPtr value_pptr, snapshotFlag flag = kSnapshot);
  std::shared_ptr<const void> pop(snapshotFlag flag = kSnapshot);
  uint32_t getLength();
  void setLength(uint32_t new_length);
  void _deallocate();

  bool shouldConvertToNumDict(uint32_t index);
  void* convertToNumDict();

 private:
  uint32_t formatIndex(uint32_t index);
  uint64_t getAllocated();
  void resize(uint32_t new_size);
  PPtr* getItems();

  MemoryManager* _mm;
  PPtr _pptr;
  PArrayObject* _parr;
};

class PMNumDict : public PMArray {
 public:
  PMNumDict(MemoryManager* mm);
  PMNumDict(MemoryManager* mm, PPtr pptr);
  ~PMNumDict(){};
  PPtr getPPtr();
  void setProperty(uint32_t key, PPtr value_pptr,
                   snapshotFlag flag = kSnapshot);
  PPtr getProperty(uint32_t key);
  void delProperty(uint32_t key, snapshotFlag flag = kSnapshot);
  std::list<uint32_t> getValidIndex();
  void push(PPtr value_pptr, snapshotFlag flag = kSnapshot);
  std::shared_ptr<const void> pop(snapshotFlag flag = kSnapshot);
  uint32_t getLength();
  void setLength(uint32_t new_length);
  void _deallocate();

  bool shouldConvertToSimpleArray(uint32_t key);
  void* convertToSimpleArray();

 private:
  uint64_t getAllocated();
  PNumDictKeysObject* getKeys();
  PPtr newKeysObject(uint64_t size);
  uint64_t usableFraction(uint64_t size);
  uint64_t fixedHash(uint32_t key);
  PNumDictKeyEntry* lookup(uint32_t key, uint64_t khash);
  void insertionResize();
  PNumDictKeyEntry* findEmptySlot(uint64_t khash);
  uint64_t growRate();

  MemoryManager* _mm;
  PPtr _pptr;
  PNumDictObject* _pnumdict;
};

}  // namespace impl
}  // namespace internal

#endif