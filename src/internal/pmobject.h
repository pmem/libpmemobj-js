#ifndef INTERNAL_PMOBJECT_H
#define INTERNAL_PMOBJECT_H

#include <stddef.h>
#include <sys/stat.h>
#include <list>
#include <memory>

#include "memorymanager.h"
#include "pmdict.h"
#include "pmarray.h"

namespace internal {
class PMObject {
 public:
  PMObject(MemoryManager* mm, void* data);
  PMObject(MemoryManager* mm, bool is_array = false);
  PMObject(const PMObject& other) = delete;
  PMObject& operator=(const PMObject& other) = delete;
  ~PMObject();
  std::shared_ptr<const void> getPPtr();
  void setProperty(std::string key, std::shared_ptr<const void> value_pptr_ptr,
                   snapshotFlag flag = kSnapshot);
  void setProperty(uint32_t index, std::shared_ptr<const void> value_pptr_ptr,
                   snapshotFlag flag = kSnapshot);
  std::shared_ptr<const void> getProperty(std::string key);
  std::shared_ptr<const void> getProperty(uint32_t index);
  void delProperty(std::string key, snapshotFlag flag = kSnapshot);
  void delProperty(uint32_t index, snapshotFlag flag = kSnapshot);
  std::list<std::shared_ptr<const void>> getPropertyNames();
  std::list<uint32_t> getValidIndex();
  void push(std::shared_ptr<const void> data);
  std::shared_ptr<const void> pop();
  bool isArray();
  uint32_t getLength();
  void setLength(uint32_t new_length);

  void _deallocate();

 private:
  MemoryManager* _mm;
  PObjectObject* _pobj;
  PPtr _pptr;

  impl::PMArray* _elements;
  impl::PMDict* _extra_props;
};
}
#endif