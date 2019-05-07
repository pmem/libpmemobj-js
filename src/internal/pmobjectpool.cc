#include <exception>
#include <memory>
#include <string>

#include "common.h"
#include "pmobjectpool.h"

namespace internal {

int PMObjectPool::check(std::string path, std::string layout) {
  return MemoryManager::check(path, layout);
}

PMObjectPool::PMObjectPool(std::string path, std::string layout) {
  _mm = new MemoryManager(path, layout);
}

PMObjectPool::PMObjectPool(std::string path, std::string layout,
                           uint32_t poolsize, mode_t mode) {
  _mm = new MemoryManager(path, layout, poolsize, mode);
  setRoot(std::make_shared<PPtr>(PPTR_UNDEFINED));
}

PMObjectPool::~PMObjectPool() { delete _mm; }

MemoryManager* PMObjectPool::getMemoryManager() { return _mm; }

std::shared_ptr<const void> PMObjectPool::getRoot() {
  PPtr root_pptr = _mm->root(sizeof(PRoot));
  PRoot* root = (PRoot*)_mm->direct(root_pptr);
  Logger::Debug("PMObjectPool::setRoot: get root_object as (%llu, %llu)\n",
                root->root_object.pool_uuid_lo, root->root_object.off);
  return std::make_shared<PPtr>(root->root_object);
}

void PMObjectPool::setRoot(std::shared_ptr<const void> data) {
  PPtr pptr = *((PPtr*)data.get());
  uint64_t type_code = *((uint64_t*)data.get());
  if (!(type_code == TYPE_CODE_NUMBER || type_code == TYPE_CODE_SINGLETON) &&
      _mm->direct(pptr) == NULL) {
    throw "invalid argument";
  }
  PPtr root_pptr = _mm->root(sizeof(PRoot));
  PRoot* root = (PRoot*)_mm->direct(root_pptr);
  MM_TX_BEGIN(_mm) {
    _mm->snapshotRange(root, sizeof(PRoot));
    root->root_object = pptr;
  }
  MM_TX_END(_mm)
  Logger::Debug("PMObjectPool::setRoot: set root_object as (%llu, %llu)\n",
                pptr.pool_uuid_lo, pptr.off);
}

// data -> {uint64_t, uint64_t}
PERSISTENT_VALUE PMObjectPool::getValue(std::shared_ptr<const void> data) {
  PERSISTENT_VALUE value;
  uint64_t type_code = *((uint64_t*)data.get());
  if (type_code == TYPE_CODE_NUMBER) {
    // number
    value.type = PERSISTENT_TYPE_NUMBER;
    value.data = (uint64_t*)data.get() + 1;
  } else if (type_code == TYPE_CODE_SINGLETON) {
    // singleton
    uint64_t off = *((uint64_t*)data.get() + 1);
    value.data = nullptr;
    if (off == SINGLETON_OFFSET_TRUE)
      value.type = PERSISTENT_TYPE_TRUE;
    else if (off == SINGLETON_OFFSET_FALSE)
      value.type = PERSISTENT_TYPE_FALSE;
    else if (off == SINGLETON_OFFSET_JS_NULL)
      value.type = PERSISTENT_TYPE_NULL;
    else if (off == SINGLETON_OFFSET_EMPTY_STRING)
      value.type = PERSISTENT_TYPE_EMPTY_STRING;
    else if (off == SINGLETON_OFFSET_UNDEFINED)
      value.type = PERSISTENT_TYPE_UNDEFINED;
    else if (off == SINGLETON_OFFSET_EMPTY)
      throw "key not found";
    else
      throw "invalid argument";
  } else {
    // string/object/arraybuffer
    PObject* pobj = (PObject*)_mm->direct(*((PPtr*)data.get()));
    if (pobj->ob_type == TYPE_CODE_STRING) {
      value.type = PERSISTENT_TYPE_STRING;
      value.data = (char*)pobj + sizeof(PStringObject);
    } else if (pobj->ob_type == TYPE_CODE_ARRAYBUFFER) {
      value.type = PERSISTENT_TYPE_ARRAYBUFFER;
      value.data = data.get();
    } else if (pobj->ob_type == TYPE_CODE_OBJECT) {
      value.type = PERSISTENT_TYPE_OBJECT;
      value.data = data.get();
    } else
      throw "invalid argument";
  }
  return value;
}

std::shared_ptr<const void> PMObjectPool::persistDouble(double value) {
  PPtr pptr = PPTR_ZERO;
  pptr.off = reinterpret_cast<uint64_t&>(value);
  return std::make_shared<PPtr>(pptr);
}

std::shared_ptr<const void> PMObjectPool::persistBoolean(bool value) {
  if (value)
    return std::make_shared<PPtr>(PPTR_TRUE);
  else
    return std::make_shared<PPtr>(PPTR_FALSE);
}

std::shared_ptr<const void> PMObjectPool::persistJSNull() {
  return std::make_shared<PPtr>(PPTR_JS_NULL);
}

std::shared_ptr<const void> PMObjectPool::persistUndefined() {
  return std::make_shared<PPtr>(PPTR_UNDEFINED);
}

std::shared_ptr<const void> PMObjectPool::persistString(std::string value) {
  if (value.length() == 0) {
    return std::make_shared<PPtr>(PPTR_EMPTY_STRING);
  } else {
    PPtr result = _mm->persistString(value);
    return std::make_shared<PPtr>(result);
    ;
  }
}

void PMObjectPool::close() { _mm->close(); }

void PMObjectPool::gc() { _mm->gc(); }

int PMObjectPool::tx_begin() { return _mm->tx_begin(); }

void PMObjectPool::tx_commit() { _mm->tx_commit(); }

void PMObjectPool::tx_abort() { _mm->tx_abort(); }

int PMObjectPool::tx_end() { return _mm->tx_end(); }

int PMObjectPool::tx_stage() { return _mm->tx_stage(); }
}  // namespace internal