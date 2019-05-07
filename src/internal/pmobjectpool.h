#ifndef INTERNAL_PMOBJECTPOOL_H
#define INTERNAL_PMOBJECTPOOL_H

#include <stddef.h>
#include <sys/stat.h>
#include <memory>

#include "memorymanager.h"
#include "../util.h"

namespace internal {
class PMObjectPool {
 public:
  static int check(std::string path, std::string layout);

 public:
  PMObjectPool(std::string path, std::string layout);
  PMObjectPool(std::string path, std::string layout, uint32_t poolsize,
               mode_t mode);
  PMObjectPool(const PMObjectPool& other) = delete;
  PMObjectPool& operator=(const PMObjectPool& other) = delete;

  ~PMObjectPool();

  MemoryManager* getMemoryManager();

  std::shared_ptr<const void> getRoot();
  void setRoot(std::shared_ptr<const void> data);

  PERSISTENT_VALUE getValue(std::shared_ptr<const void> data);
  std::shared_ptr<const void> persistDouble(double value);
  std::shared_ptr<const void> persistBoolean(bool value);
  std::shared_ptr<const void> persistJSNull();
  std::shared_ptr<const void> persistUndefined();
  std::shared_ptr<const void> persistString(std::string value);

  void close();
  void gc();

  int tx_begin();
  void tx_commit();
  void tx_abort();
  int tx_end();
  int tx_stage();

 private:
  MemoryManager* _mm = nullptr;
};
}
#endif