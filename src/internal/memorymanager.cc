#include <assert.h>
#include <libpmemobj.h>
#include <stdio.h>
#include <list>
#include <map>
#include <set>
#include <string>

using namespace std;

#include "memorymanager.h"
#include "pmarray.h"
#include "pmdict.h"
#include "pmobject.h"

bool operator<(const PPtr& a, const PPtr& b) {
  return (a.pool_uuid_lo < b.pool_uuid_lo ||
          (a.pool_uuid_lo == b.pool_uuid_lo && a.off < b.off));
}

namespace internal {
int MemoryManager::check(std::string path, std::string layout) {
  return pmemobj_check(path.c_str(), layout.c_str());
}

MemoryManager::MemoryManager(std::string path, std::string layout) {
  _pool = pmemobj_open(path.c_str(), layout.c_str());
  if (_pool == NULL) throw "failed to open pool";
}

MemoryManager::MemoryManager(std::string path, std::string layout,
                             uint32_t poolsize, mode_t mode) {
  _pool = pmemobj_create(path.c_str(), layout.c_str(), poolsize, mode);

  if (_pool == NULL) throw "failed to create pool";
}

PPtr MemoryManager::root(size_t size) {
  PPtr root_pptr = pmemobj_root(_pool, size);
  if (PPTR_EQUALS(root_pptr, PPTR_NULL)) throw "failed to get root";
  return root_pptr;
}

void* MemoryManager::direct(PPtr pptr) { return pmemobj_direct(pptr); }

PPtr MemoryManager::pptr(const void* addr) { return pmemobj_oid(addr); }

int MemoryManager::snapshotRange(const void* ptr, size_t size) {
  Logger::Debug(
      "MemoryManager::snapshotRange: taking snapshot at (%llu, %llu) with size "
      "= %llu\n",
      pptr(ptr).pool_uuid_lo, pptr(ptr).off, size);
  return pmemobj_tx_add_range_direct(ptr, size);
}

bool MemoryManager::inTransaction() {
  pobj_tx_stage tx_stage = pmemobj_tx_stage();
  return (tx_stage == TX_STAGE_WORK);
}

void* MemoryManager::tx_zalloc(size_t size, int type_num) {
  if (size == 0) return nullptr;
  PPtr pptr = pmemobj_tx_zalloc(size, type_num);
  if (PPTR_EQUALS(pptr, PPTR_NULL)) {
    throw "failed allocate memory";
  }
  return direct(pptr);
}

void* MemoryManager::tz_zrealloc(PPtr pptr, size_t size, int type_num) {
  if (size == 0) {
    free(pptr);
    return nullptr;
  };
  if (type_num == NONE_TYPE_NUM) {
    type_num = pmemobj_type_num(pptr);
  }
  PPtr pptr_new = pmemobj_tx_zrealloc(pptr, size, type_num);
  if (PPTR_EQUALS(pptr_new, PPTR_NULL)) {
    throw "failed to allocate memory";
  }
  return direct(pptr_new);
}

void* MemoryManager::zalloc(size_t size, int type_num) {
  if (size == 0) return nullptr;
  PPtr pptr;
  pmemobj_zalloc(_pool, &pptr, size, type_num);
  if (PPTR_EQUALS(pptr, PPTR_NULL)) {
    throw "failed allocate memory";
  }
  return direct(pptr);
}

void MemoryManager::persist(const void* addr, size_t length) {
  pmemobj_persist(_pool, addr, length);
}

PPtr MemoryManager::persistString(std::string str) {
  PStringObject* psobj = nullptr;
  size_t length = sizeof(PStringObject) + str.length() + 1;
  if (inTransaction()) {
    psobj = (PStringObject*)tx_zalloc(length, POBJ_TYPE_NUM);
    ((PObject*)psobj)->ob_type = TYPE_CODE_STRING;
    strncpy((char*)psobj + sizeof(PStringObject), str.c_str(), str.length());
  } else {
    psobj = (PStringObject*)zalloc(length, POBJ_TYPE_NUM);
    ((PObject*)psobj)->ob_type = TYPE_CODE_STRING;
    strncpy((char*)psobj + sizeof(PObject), str.c_str(), str.length());
    persist(psobj, length);
  }
  return pptr(psobj);
}

void MemoryManager::tx_enter_context() {
  int errnum = pmemobj_tx_begin(_pool, NULL, NULL);
  if (errnum) {
    throw "failed to switch transaction state";
  }
}

void MemoryManager::tx_exit_context() {
  if (pmemobj_tx_stage() == TX_STAGE_WORK) {
    pmemobj_tx_commit();
  }
  int errnum = pmemobj_tx_end();
  if (errnum) {
    throw "ailed to switch transaction state";
  }
}

int MemoryManager::tx_begin() { return pmemobj_tx_begin(_pool, NULL, NULL); }

void MemoryManager::tx_commit() { pmemobj_tx_commit(); }

void MemoryManager::tx_abort() { pmemobj_tx_abort(INTERNAL_ABORT_ERRNO); }

int MemoryManager::tx_end() { return pmemobj_tx_end(); }

int MemoryManager::tx_stage() { return pmemobj_tx_stage(); }

void MemoryManager::free(PPtr pptr) {
  Logger::Debug("MemoryManager::free: trying to free (%llu, %llu)\n",
                pptr.pool_uuid_lo, pptr.off);
  if (direct(pptr) != NULL) {
    int errnum = pmemobj_tx_free(pptr);
    if (errnum) {
      pmemobj_tx_end();
      throw "failed to free memory";
    };
  }
}

void MemoryManager::close() { pmemobj_close(_pool); }

void MemoryManager::gc() {
  set<PPtr> containers, other;
  size_t type_counts[TYPE_CODE_INTERNAL_MAX] = {0};
  map<uint32_t, map<PPtr, set<PPtr>>> substructures;

  map<string, int> gc_count;
  gc_count[string("container-total")] = 0;
  gc_count[string("other-total")] = 0;
  gc_count[string("container-live")] = 0;

  // if root is not a container, it will not be calculated in other-live
  gc_count[string("other-live")] = 0;
  gc_count[string("non-pobject-total")] = 0;

  PPtr pptr = pmemobj_first(_pool);
  while (!PPTR_EQUALS(pptr, PPTR_NULL)) {
    uint64_t type_num = pmemobj_type_num(pptr);
    if (type_num == POBJ_TYPE_NUM) {
      PObject* pobj = (PObject*)direct(pptr);
      size_t type_code = pobj->ob_type;
      assert(type_code < TYPE_CODE_INTERNAL_MAX);
      type_counts[type_code] += 1;

      // If is containter type
      if (TYPE_CODE_IS_CONTAINER(type_code)) {
        containers.insert(pptr);
      } else {
        other.insert(pptr);
      }
    } else {
      // Non PObject type (those do not have a PObject in the head), such as
      // PDictKeysObject
      gc_count[string("non-pobject-total")] += 1;
    }
    pptr = pmemobj_next(pptr);
  }
  gc_count[string("container-total")] = containers.size();
  gc_count[string("other-total")] = other.size();

  // Trace the object tree, removing objects that are referenced.
  PPtr root_pptr = pmemobj_root(_pool, 0);
  PPtr root_obj_pptr = ((PRoot*)direct(root_pptr))->root_object;
  list<PPtr> live;
  if (root_obj_pptr.pool_uuid_lo == 0 ||
      root_obj_pptr.pool_uuid_lo == TYPE_CODE_SINGLETON ||
      root_obj_pptr.pool_uuid_lo == TYPE_CODE_NUMBER) {
    // singleton / number
  } else {
    PObject* root_obj = (PObject*)direct(root_obj_pptr);
    assert(root_obj->ob_type < TYPE_CODE_INTERNAL_MAX);
    // root object must be Object or non-container
    if (root_obj->ob_type == TYPE_CODE_OBJECT) {
      containers.erase(root_obj_pptr);
      live.push_back(root_obj_pptr);
    } else {
      assert(!TYPE_CODE_IS_CONTAINER(root_obj->ob_type));
      other.erase(root_obj_pptr);
    }
  }
  
  list<PPtr>::iterator it_live;
  // TODO: find way to encapsulate the process
  for (it_live = live.begin(); it_live != live.end(); ++it_live) {
    PPtr to_trace = *it_live;
    PObject* pobj = (PObject*)direct(to_trace);

    if (pobj->ob_type == TYPE_CODE_OBJECT) {
      PPtr elements_pptr = ((PObjectObject*)pobj)->elements;
      PPtr props_pptr = ((PObjectObject*)pobj)->extra_props;
      // append elements to live
      assert(containers.find(elements_pptr) != containers.end());
      live.push_back(elements_pptr);
      containers.erase(elements_pptr);
      // append props to live
      assert(containers.find(props_pptr) != containers.end());
      live.push_back(props_pptr);
      containers.erase(props_pptr);
    } else if (pobj->ob_type == TYPE_CODE_ARRAY) {
      PArrayObject* parr = (PArrayObject*)pobj;
      PPtr items_pptr = parr->ob_items;
      PPtr* items = (PPtr*)direct(items_pptr);
      size_t items_size = parr->allocated;

      for (size_t i = 0; i < items_size; ++i) {
        PPtr item_pptr = *(items + i);
        if (containers.find(item_pptr) != containers.end()) {
          live.push_back(item_pptr);
          containers.erase(item_pptr);
        } else if (other.find(item_pptr) != other.end()) {
          other.erase(item_pptr);
          gc_count[string("other-live")] += 1;
        }
      }
    } else if (pobj->ob_type == TYPE_CODE_DICT) {
      PDictObject* pdict = (PDictObject*)pobj;
      PDictKeysObject* pkeys = (PDictKeysObject*)direct(pdict->ma_keys);
      size_t pkeys_size = pkeys->dk_size;
      PDictKeyEntry* ep0 = pkeys->dk_entries;

      for (size_t i = 0; i < pkeys_size; ++i) {
        PPtr key_pptr = (ep0 + i)->me_key;
        PPtr value_pptr = (ep0 + i)->me_value;
        // key must be a string (that is, in the set "other")
        if (other.find(key_pptr) != other.end()) {
          gc_count[string("other-live")] += 1;
        }
        // value could be container or non-container
        if (containers.find(value_pptr) != containers.end()) {
          live.push_back(value_pptr);
          containers.erase(value_pptr);
        } else if (other.find(value_pptr) != other.end()) {
          other.erase(value_pptr);
          gc_count[string("other-live")] += 1;
        }
      }
    } else if (pobj->ob_type == TYPE_CODE_NUMDICT) {
      PNumDictObject* pdict = (PNumDictObject*)pobj;
      PNumDictKeysObject* pkeys = (PNumDictKeysObject*)direct(pdict->ma_keys);
      size_t pkeys_size = pkeys->dk_size;
      PNumDictKeyEntry* ep0 = pkeys->dk_entries;

      for (size_t i = 0; i < pkeys_size; ++i) {
        PPtr value_pptr = (ep0 + i)->me_value;
        // value could be container or non-container
        if (containers.find(value_pptr) != containers.end()) {
          live.push_back(value_pptr);
          containers.erase(value_pptr);
        } else if (other.find(value_pptr) != other.end()) {
          ;
          other.erase(value_pptr);
          gc_count[string("other-live")] += 1;
        }
      }
    }
  }
  gc_count[string("containers-live")] = live.size();
  // Everything left is unreferenced via the root, deallocate it.
  set<PPtr>::iterator it_container;
  for (it_container = containers.begin(); it_container != containers.end();
       ++it_container) {
    PPtr container_pptr = *it_container;

    PObject* pobj = (PObject*)direct(container_pptr);
    Logger::Debug(
        "MemoryManager::gc: trying to free container at (%llu, %llu)\n",
        container_pptr.pool_uuid_lo, container_pptr.off);
    if (pobj->ob_type == TYPE_CODE_OBJECT) {
      PMObject* obj = new PMObject(this, &container_pptr);
      obj->_deallocate();
      delete obj;
    } else if (pobj->ob_type == TYPE_CODE_ARRAY) {
      impl::PMSimpleArray* arr = new impl::PMSimpleArray(this, container_pptr);
      arr->_deallocate();
      delete arr;

    } else if (pobj->ob_type == TYPE_CODE_DICT) {
      impl::PMDict* dict = new impl::PMDict(this, container_pptr);
      dict->_deallocate();
      delete dict;

    } else if (pobj->ob_type == TYPE_CODE_NUMDICT) {
      impl::PMNumDict* numdict = new impl::PMNumDict(this, container_pptr);
      numdict->_deallocate();
      delete numdict;
    }
  }

  set<PPtr>::iterator it_other;
  gc_count[string("other-live")] =
      gc_count[string("other-total")] - other.size();
  MM_TX_BEGIN(this) {
    for (it_other = other.begin(); it_other != other.end(); ++it_other) {
      PPtr other_pptr = *it_other;
      free(other_pptr);
    }
  }
  MM_TX_END(this)
}

}  // namespace internal