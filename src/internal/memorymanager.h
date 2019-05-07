#ifndef INTERNAL_MM_H
#define INTERNAL_MM_H

#include <libpmemobj.h>
#include <sys/stat.h>
#include <string>

#include "common.h"

typedef PMEMoid PPtr;

typedef pobj_tx_stage POOL_TX_STAGE;

#define MM_TX_BEGIN(mm) mm->tx_enter_context();
#define MM_TX_END(mm) mm->tx_exit_context();

enum snapshotFlag { kNotSnapshot, kSnapshot };

namespace internal {
class MemoryManager {
 public:
  static int check(std::string path, std::string layout);

 public:
  MemoryManager(std::string path, std::string layout);
  MemoryManager(std::string path, std::string layout, uint32_t poolsize,
                mode_t mode);

  PPtr root(size_t size);
  void *direct(PPtr pptr);
  PPtr pptr(const void *addr);
  int snapshotRange(const void* ptr, size_t size);
  bool inTransaction();
  void checkAndFree(const void* data);

  void *tx_zalloc(size_t size, int type_num = POBJ_TYPE_NUM);
  void *tz_zrealloc(PPtr pptr, size_t size, int type_num = POBJ_TYPE_NUM);
  void *zalloc(size_t size, int type_num = POBJ_TYPE_NUM);
  void persist(const void* addr, size_t length);
  PPtr persistString(std::string str);

  void tx_enter_context();
  void tx_exit_context();
  int tx_begin();
  void tx_commit();
  void tx_abort();
  int tx_end();
  int tx_stage();

  void free(PPtr pptr);
  void close();
  void gc();

 private:
  PMEMobjpool *_pool;
};
};
#endif