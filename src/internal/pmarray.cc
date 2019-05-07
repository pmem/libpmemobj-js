#include <assert.h>
#include <list>

#include "memorymanager.h"
#include "pmarray.h"

#define MIN_SIZE_COMBINED 8
#define MIN_SIZE_SPLIT 4
#define PERTURB_SHIFT 5

#define ENTRY_NULL 0
#define ENTRY_DUMMY 1
#define ENTRY_FULL 2

#define ARRAY_MAX_GAP 1024
// TODO: find proper uncheck number
#define ARRAY_MAX_UNCHECK 5000
#define ARRAY_ELEMENTS_SIZE_FACTOR 3

namespace internal {
namespace impl {
PMArray::~PMArray(){};

// SimpleArray
PMSimpleArray::PMSimpleArray(MemoryManager *mm) {
  _mm = mm;
  Logger::Debug("PMSimpleArray::PMSimpleArray: creating empty array\n");
  MM_TX_BEGIN(_mm) {
    _parr = (PArrayObject *)_mm->tx_zalloc(sizeof(PArrayObject), POBJ_TYPE_NUM);
    ((PObject *)_parr)->ob_type = TYPE_CODE_ARRAY;
    _pptr = _mm->pptr(_parr);
  }
  MM_TX_END(_mm)
};

PMSimpleArray::PMSimpleArray(MemoryManager *mm, PPtr pptr) {
  _mm = mm;
  _pptr = pptr;
  Logger::Debug("PMSimpleArray::PMSimpleArray: construct by (%llu, %llu)\n",
                _pptr.pool_uuid_lo, _pptr.off);
  _parr = (PArrayObject *)_mm->direct(_pptr);
}

PPtr PMSimpleArray::getPPtr() { return _pptr; }

void PMSimpleArray::setProperty(uint32_t index, PPtr value_pptr,
                                snapshotFlag flag) {
  uint32_t idx = formatIndex(index);
  // If index exceed the maximum array size, reallocate new space.
  assert(idx < UINT32_MAX);
  uint64_t allocated = getAllocated();
  if ((idx + 1) > allocated) {
    resize(idx + 1);
  }
  PPtr *items = getItems();
  // _mm->snapshotRange(items+idx, sizeof(PPtr));
  if (_mm->inTransaction()) {
    MM_TX_BEGIN(_mm) {
      if (flag) _mm->snapshotRange(items + idx, sizeof(PPtr));
      *(items + idx) = value_pptr;
      if (idx + 1 > getLength()) {
        PVarObject *ob = (PVarObject *)_parr;
        if (flag)
          _mm->snapshotRange(&(ob->ob_size), sizeof(PVarObject::ob_size));
        ob->ob_size = idx + 1;
      }
    }
    MM_TX_END(_mm)
  } else {
    if ((items + idx)->pool_uuid_lo != value_pptr.pool_uuid_lo) {
      MM_TX_BEGIN(_mm) {
        _mm->snapshotRange(items + idx, sizeof(PPtr));
        *(items + idx) = value_pptr;
      }
      MM_TX_END(_mm)

    } else {
      (items + idx)->off = value_pptr.off;
      _mm->persist(items + idx + offsetof(PPtr, off), sizeof(PPtr::off));
    }
    if (idx + 1 > getLength()) {
      PVarObject *ob = (PVarObject *)_parr;
      ob->ob_size = idx + 1;
      _mm->persist(&(ob->ob_size), sizeof(PVarObject::ob_size));
    }
  }
}

PPtr PMSimpleArray::getProperty(uint32_t index) {
  uint32_t idx = formatIndex(index);
  // Return undefined for invalid index
  if (idx >= getLength()) {
    return PPTR_UNDEFINED;
  }

  PPtr *items = getItems();
  return *(items + idx);
}

void PMSimpleArray::delProperty(uint32_t index, snapshotFlag flag) {
  if (index < getLength()) {
    setProperty(index, PPTR_UNDEFINED, flag);
  }
}

std::list<uint32_t> PMSimpleArray::getValidIndex() {
  std::list<uint32_t> indexes;
  uint32_t length = getLength();
  PPtr *items = getItems();
  for (uint32_t i = 0; i < length; ++i) {
    if (!PPTR_EQUALS(*(items + i), PPTR_NULL)) {
      indexes.push_back(i);
    }
  }
  return indexes;
}

void PMSimpleArray::push(PPtr value_pptr, snapshotFlag flag) {
  uint32_t index = getLength();
  setProperty(index, value_pptr, flag);
}

std::shared_ptr<const void> PMSimpleArray::pop(snapshotFlag flag) {
  PPtr *items = getItems();
  uint32_t length = getLength();
  uint32_t new_length = length - 1;

  PPtr pptr = *(items + (length - 1));
  MM_TX_BEGIN(_mm) {
    _mm->snapshotRange(&(((PVarObject *)_parr)->ob_size),
                       sizeof(PVarObject::ob_size));
    ((PVarObject *)_parr)->ob_size = new_length;
    resize(new_length);
  }
  MM_TX_END(_mm)
  return std::make_shared<PPtr>(pptr);
}

uint32_t PMSimpleArray::getLength() { return ((PVarObject *)_parr)->ob_size; }

void PMSimpleArray::setLength(uint32_t new_length) { resize(new_length); }

void PMSimpleArray::_deallocate() {
  MM_TX_BEGIN(_mm) {
    _mm->free(_parr->ob_items);
    _mm->free(_pptr);
  }
  MM_TX_END(_mm)
}

bool PMSimpleArray::shouldConvertToNumDict(uint32_t index) {
  uint32_t allocated = getAllocated();
  if (index < allocated) return false;
  if (index - allocated > ARRAY_MAX_GAP) return true;
  // reuse CPython's overallocation algorithm
  assert(index < UINT32_MAX);
  uint32_t new_size = index + 1;
  uint32_t new_allocated = (new_size >> 3) + (new_size < 9 ? 3 : 6) + new_size;
  if (new_allocated < ARRAY_MAX_UNCHECK) return false;

  uint64_t array_space = new_allocated * sizeof(PPtr);
  uint64_t dict_space = allocated * sizeof(PNumDictKeyEntry);
  return dict_space * ARRAY_ELEMENTS_SIZE_FACTOR < array_space;
}

void *PMSimpleArray::convertToNumDict() {
  PPtr *items = (PPtr *)_mm->direct(_parr->ob_items);
  uint32_t size = _parr->ob_base.ob_size;

  PMNumDict *pnumdict = new PMNumDict(_mm);
  MM_TX_BEGIN(_mm) {
    for (uint32_t i = 0; i < size; ++i) {
      if (!PPTR_EQUALS(*(items + i), PPTR_NULL)) {
        pnumdict->setProperty(i, *(items + i));
      }
    }
    _mm->free(_parr->ob_items);
    _mm->free(_pptr);
  }
  MM_TX_END(_mm)
  return pnumdict;
}

uint32_t PMSimpleArray::formatIndex(uint32_t index) { return index; }

uint64_t PMSimpleArray::getAllocated() {
  return ((PArrayObject *)_parr)->allocated;
}

void PMSimpleArray::resize(uint32_t new_size) {
  uint32_t allocated = getAllocated();
  PPtr *items = getItems();

  if (allocated >= new_size && new_size >= (allocated >> 1)) {
    // no need to allocate new space since there is enough space
    // but initialization is required
    assert(items != nullptr || new_size == 0);
    MM_TX_BEGIN(_mm) {
      PVarObject *ob = (PVarObject *)_parr;
      _mm->snapshotRange(&(ob->ob_size), sizeof(PVarObject::ob_size));
      ob->ob_size = new_size;
      // set to PPTR_NULL
      memset(items + new_size, 0, (allocated - new_size) * sizeof(PPtr));
      _mm->persist(items + new_size, (allocated - new_size) * sizeof(PPtr));
    }
    MM_TX_END(_mm)
    return;
  }

  uint32_t new_allocated = (new_size >> 3) + (new_size < 9 ? 3 : 6) + new_size;
  if (new_size == 0) new_allocated = 0;
  const void *new_addr;
  MM_TX_BEGIN(_mm) {
    if (items == nullptr) {
      new_addr =
          _mm->tx_zalloc(new_allocated * sizeof(PPtr), ARRAY_ITEMS_TYPE_NUM);
    } else {
      new_addr =
          _mm->tz_zrealloc(((PArrayObject *)_parr)->ob_items,
                           new_allocated * sizeof(PPtr), ARRAY_ITEMS_TYPE_NUM);
    }
    _mm->snapshotRange(_parr, sizeof(PArrayObject));
    ((PArrayObject *)_parr)->ob_items = _mm->pptr(new_addr);
    ((PVarObject *)_parr)->ob_size = new_size;
    ((PArrayObject *)_parr)->allocated = new_allocated;
  }
  MM_TX_END(_mm)
}

PPtr *PMSimpleArray::getItems() {
  PPtr items_pptr = ((PArrayObject *)_parr)->ob_items;
  if (PPTR_EQUALS(items_pptr, PPTR_NULL)) {
    return nullptr;
  }
  return (PPtr *)_mm->direct(items_pptr);
}

// NumDict

PMNumDict::PMNumDict(MemoryManager *mm) {
  _mm = mm;
  MM_TX_BEGIN(_mm) {
    _pnumdict = (PNumDictObject *)_mm->tx_zalloc(sizeof(PNumDictObject));
    ((PObject *)_pnumdict)->ob_type = TYPE_CODE_NUMDICT;
    _pnumdict->ma_keys = newKeysObject(MIN_SIZE_COMBINED);
  }
  MM_TX_END(_mm)
  _pptr = _mm->pptr(_pnumdict);
}

PMNumDict::PMNumDict(MemoryManager *mm, PPtr pptr) {
  _mm = mm;
  _pptr = pptr;
  _pnumdict = (PNumDictObject *)_mm->direct(_pptr);
}

PPtr PMNumDict::getPPtr() { return _pptr; }

void PMNumDict::setProperty(uint32_t index, PPtr value_pptr,
                            snapshotFlag flag) {
  assert(index < UINT32_MAX);
  uint64_t khash = fixedHash(index);
  PNumDictKeysObject *keys = getKeys();
  PNumDictKeyEntry *ep = lookup(index, khash);

  MM_TX_BEGIN(_mm) {
    PPtr old_value_pptr = ep->me_value;
    uint32_t me_key = ep->me_key;
    uint32_t me_state = ep->me_state;
    if (!PPTR_EQUALS(old_value_pptr, PPTR_NULL)) {
      assert(me_state != ENTRY_NULL && me_state != ENTRY_DUMMY);
      if (flag) _mm->snapshotRange(&(ep->me_value), sizeof(PPtr));
      ep->me_value = value_pptr;
    } else {
      if (flag) _mm->snapshotRange(ep, sizeof(PNumDictKeyEntry));
      if (me_state == ENTRY_NULL) {
        if (keys->dk_usable <= 0) {
          insertionResize();
          keys = getKeys();
        }
        ep = findEmptySlot(khash);
        if (flag) _mm->snapshotRange(&(keys->dk_usable), sizeof(int64_t));
        keys->dk_usable -= 1;
        assert(keys->dk_usable >= 0);
        ep->me_key = index;
        ep->me_hash = khash;
        ep->me_state = ENTRY_FULL;
        if (flag)
          _mm->snapshotRange(&(_pnumdict->ob_base.ob_size), sizeof(int64_t));
        if (_pnumdict->ob_base.ob_size < index + 1)
          _pnumdict->ob_base.ob_size = index + 1;

      } else {
        if (me_state == ENTRY_DUMMY) {
          ep->me_key = me_key;
          ep->me_hash = khash;
        } else {
          throw "failed to set property";
        }
      }
      if (flag) _mm->snapshotRange(&(_pnumdict->ma_used), sizeof(int64_t));
      _pnumdict->ma_used += 1;
      ep->me_value = value_pptr;
      assert(ep->me_state != ENTRY_NULL && ep->me_state != ENTRY_DUMMY);
    }
  }
  MM_TX_END(_mm)
}

PPtr PMNumDict::getProperty(uint32_t key) {
  if (key >= getLength()) {
    return PPTR_UNDEFINED;
  }
  uint64_t khash = fixedHash(key);
  PNumDictKeyEntry *ep = lookup(key, khash);
  if (ep->me_state == ENTRY_NULL) {
    return PPTR_UNDEFINED;
  }
  return ep->me_value;
}

void PMNumDict::delProperty(uint32_t key, snapshotFlag flag) {
  setProperty(key, PPTR_NULL, flag);
}

std::list<uint32_t> PMNumDict::getValidIndex() {
  std::list<uint32_t> indexes;
  PNumDictKeysObject *keys = getKeys();
  int64_t dk_size = keys->dk_size;
  PNumDictKeyEntry *ep0 = keys->dk_entries;
  PNumDictKeyEntry *ep;
  for (int i = 0; i < dk_size; ++i) {
    ep = ep0 + i;
    if (ep->me_state == ENTRY_FULL) {
      indexes.push_back(ep->me_key);
    }
  }
  return indexes;
}

void PMNumDict::push(PPtr value_pptr, snapshotFlag flag) {
  uint32_t index = getLength();
  setProperty(index, value_pptr, flag);
}

std::shared_ptr<const void> PMNumDict::pop(snapshotFlag flag) {
  uint32_t length = getLength();
  uint32_t key = length - 1;
  uint64_t khash = fixedHash(key);
  PNumDictKeyEntry *ep = lookup(key, khash);
  if (ep->me_state == ENTRY_NULL) {
    return std::make_shared<PPtr>(PPTR_UNDEFINED);
  }
  PPtr old_value_pptr;
  MM_TX_BEGIN(_mm) {
    if (flag) _mm->snapshotRange(ep, sizeof(PNumDictKeyEntry));
    if (flag) _mm->snapshotRange(_pnumdict, sizeof(PNumDictObject));
    old_value_pptr = ep->me_value;
    ep->me_value = PPTR_NULL;
    _pnumdict->ma_used -= 1;
    _pnumdict->ob_base.ob_size -= 1;
    ep->me_state = ENTRY_DUMMY;
  }
  MM_TX_END(_mm)
  return std::make_shared<PPtr>(old_value_pptr);
}

uint32_t PMNumDict::getLength() { return _pnumdict->ob_base.ob_size; }

void PMNumDict::setLength(uint32_t new_length) {
  uint32_t length = _pnumdict->ob_base.ob_size;
  if (length == new_length) return;
  MM_TX_BEGIN(_mm) {
    _mm->snapshotRange(&(_pnumdict->ob_base.ob_size),
                       sizeof(PVarObject::ob_size));
    _pnumdict->ob_base.ob_size = new_length;
    if (new_length < length) {
      for (uint32_t i = new_length; i < length; ++i) {
        delProperty(i, kSnapshot);
      }
    }
  }
  MM_TX_END(_mm)
}

void PMNumDict::_deallocate() {
  MM_TX_BEGIN(_mm) {
    _mm->free(_pnumdict->ma_keys);
    _mm->free(_pptr);
  }
  MM_TX_END(_mm)
}

bool PMNumDict::shouldConvertToSimpleArray(uint32_t key) {
  uint32_t length = getLength();
  uint32_t allocated = getAllocated();
  if (key > SMI_MAX) return false;

  uint32_t new_length = length > key + 1 ? length : (key + 1);
  uint32_t array_allocated =
      (new_length >> 3) + (new_length < 9 ? 3 : 6) + new_length;
  uint64_t array_space = array_allocated * sizeof(PPtr);
  uint64_t dict_space = allocated * sizeof(PNumDictKeyEntry);
  return dict_space >= array_space >> 1;
}

void *PMNumDict::convertToSimpleArray() {
  PNumDictKeysObject *pkeys =
      (PNumDictKeysObject *)_mm->direct(_pnumdict->ma_keys);
  uint32_t dk_size = pkeys->dk_size;
  uint32_t size = _pnumdict->ob_base.ob_size;
  PNumDictKeyEntry *ep0 = pkeys->dk_entries;
  PNumDictKeyEntry *ep = ep0;

  PMSimpleArray *parr = new PMSimpleArray(_mm);
  MM_TX_BEGIN(_mm) {
    // Set the largest index first, so that no resize is required any more
    // resize() is invoked during the process, which tx_zalloc() a region
    // so that no snapshot is required
    parr->setProperty(size - 1, PPTR_UNDEFINED);
    PArrayObject *parr_obj = (PArrayObject *)_mm->direct(parr->getPPtr());
    PPtr *items = (PPtr *)_mm->direct(parr_obj->ob_items);
    uint32_t index;
    for (size_t i = 0; i < dk_size; ++i) {
      ep = ep0 + i;
      if (ep->me_state == ENTRY_FULL) {
        index = ep->me_key;
        assert(index < size);
        *(items + index) = ep->me_value;
      }
    }
    _mm->free(_pnumdict->ma_keys);
    _mm->free(_pptr);
  }
  MM_TX_END(_mm)
  return parr;
}

uint64_t PMNumDict::getAllocated() {
  PNumDictKeysObject *keys = getKeys();
  return keys->dk_size;
}

PNumDictKeysObject *PMNumDict::getKeys() {
  return (PNumDictKeysObject *)_mm->direct(_pnumdict->ma_keys);
}

PPtr PMNumDict::newKeysObject(uint64_t size) {
  assert(size > MIN_SIZE_SPLIT);

  PPtr keys_pptr;
  MM_TX_BEGIN(_mm) {
    PNumDictKeysObject *keys = (PNumDictKeysObject *)_mm->tx_zalloc(
        sizeof(PNumDictKeysObject) + sizeof(PNumDictKeyEntry) * (size - 1),
        PNUMDICTKEYSOBJECT_TYPE_NUM);
    keys_pptr = _mm->pptr(keys);
    keys->dk_size = size;
    uint64_t usable = usableFraction(size);
    assert(usable < INT64_MAX);
    keys->dk_usable = usable;
    PNumDictKeyEntry *ep = keys->dk_entries;
    ep->me_hash = 0;
    for (size_t i = 0; i < size; ++i) {
      (ep + i)->me_state = ENTRY_NULL;
      (ep + i)->me_value = PPTR_NULL;
    }
  }
  MM_TX_END(_mm)

  return keys_pptr;
}

uint64_t PMNumDict::usableFraction(uint64_t size) {
  if (size >= UINT32_MAX) {
    return size / 3 * 2;
  }
  return (2 * size + 1) / 3;
}

uint64_t PMNumDict::fixedHash(uint32_t key) {
  // TODO: find proper hash for 32 bit key to 64 bit hash
  return (uint64_t)key;
}

PNumDictKeyEntry *PMNumDict::lookup(uint32_t key, uint64_t khash) {
  while (true) {
    PPtr ma_keys = _pnumdict->ma_keys;
    PNumDictKeysObject *keys = (PNumDictKeysObject *)_mm->direct(ma_keys);

    ssize_t mask = keys->dk_size - 1;
    PNumDictKeyEntry *ep0 = keys->dk_entries;
    ssize_t idx = khash & mask;
    PNumDictKeyEntry *ep = ep0 + idx;
    uint32_t me_key = ep->me_key;
    uint32_t me_state = ep->me_state;
    PNumDictKeyEntry *freeslot;

    if (me_state == ENTRY_NULL) {
      return ep;
    } else if (me_state == ENTRY_DUMMY) {
      freeslot = ep;
    } else {
      if (ep->me_key == key) {
        return ep;
      }
      freeslot = nullptr;
    }
    uint64_t perturb = khash;

    while (true) {
      idx = (idx << 2) + idx + perturb + 1;
      ep = ep0 + (idx & mask);
      me_key = ep->me_key;
      me_state = ep->me_state;
      if (me_state == ENTRY_NULL) {
        return (freeslot == nullptr) ? ep : freeslot;
      }
      if (me_key == key) {
        return ep;
      } else if (me_state == ENTRY_DUMMY && freeslot == nullptr) {
        freeslot = ep;
      }
      perturb = perturb >> PERTURB_SHIFT;
    }
  }
}

void PMNumDict::insertionResize() {
  size_t minused = growRate();
  size_t newsize = MIN_SIZE_COMBINED;
  while (newsize <= minused && newsize > 0) {
    newsize = newsize << 1;
  }
  PNumDictKeysObject *old_keys = getKeys();
  PPtr old_keys_pptr = _pnumdict->ma_keys;

  MM_TX_BEGIN(_mm) {
    _mm->snapshotRange(&(_pnumdict->ma_keys), sizeof(PPtr));
    _pnumdict->ma_keys = newKeysObject(newsize);
    size_t oldsize = old_keys->dk_size;
    PNumDictKeyEntry *old_ep0 = old_keys->dk_entries;
    for (size_t i = 0; i < oldsize; ++i) {
      PNumDictKeyEntry *old_ep = old_ep0 + i;
      PPtr me_value = old_ep->me_value;
      if (!PPTR_EQUALS(me_value, PPTR_NULL)) {
        uint32_t me_key = old_ep->me_key;
        uint32_t me_state = old_ep->me_state;
        assert(me_state != ENTRY_DUMMY);
        uint64_t me_hash = old_ep->me_hash;
        PNumDictKeyEntry *new_ep = findEmptySlot(me_hash);
        new_ep->me_key = me_key;
        new_ep->me_state = me_state;
        new_ep->me_hash = me_hash;
        new_ep->me_value = me_value;
      }
    }
    PNumDictKeysObject *new_keys = getKeys();
    new_keys->dk_usable -= _pnumdict->ma_used;
    _mm->free(old_keys_pptr);
  }
  MM_TX_END(_mm)
}

PNumDictKeyEntry *PMNumDict::findEmptySlot(uint64_t khash) {
  PNumDictKeysObject *keys = getKeys();
  ssize_t mask = keys->dk_size - 1;
  PNumDictKeyEntry *ep0 = keys->dk_entries;
  ssize_t idx = khash & mask;
  PNumDictKeyEntry *ep = ep0 + idx;
  uint64_t perturb = khash;
  while (ep->me_state != ENTRY_NULL) {
    idx = (idx << 2) + idx + perturb + 1;
    ep = ep0 + (idx & mask);
    perturb = perturb >> PERTURB_SHIFT;
  }
  assert(ep->me_state == ENTRY_NULL);
  return ep;
}

uint64_t PMNumDict::growRate() {
  PNumDictKeysObject *keys = getKeys();
  assert(_pnumdict->ma_used < UINT32_MAX);
  assert(UINT64_MAX - (_pnumdict->ma_used << 2) > (keys->dk_size >> 1));
  return _pnumdict->ma_used * 2 + (keys->dk_size >> 1);
}

}  // namespace impl
}  // namespace internal