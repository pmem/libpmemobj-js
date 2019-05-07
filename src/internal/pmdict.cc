#include <assert.h>
#include <openssl/md5.h>
#include <list>
#include <string>

#include "common.h"
#include "pmdict.h"

#define MIN_SIZE_COMBINED 8
#define MIN_SIZE_SPLIT 4
#define PERTURB_SHIFT 5

namespace internal {
namespace impl {
PMDict::PMDict(MemoryManager *mm, PPtr pptr) {
  _mm = mm;
  _pptr = pptr;
  Logger::Debug("PMDict::PMDict: construct by (%llu, %llu)\n",
                _pptr.pool_uuid_lo, _pptr.off);
  _pdict = (PDictObject *)_mm->direct(_pptr);
}

PMDict::PMDict(MemoryManager *mm) {
  _mm = mm;
  Logger::Debug("PMDict::PMDict: creating empty dict\n");
  MM_TX_BEGIN(_mm) {
    _pdict = (PDictObject *)_mm->tx_zalloc(sizeof(PDictObject));
    ((PObject *)_pdict)->ob_type = TYPE_CODE_DICT;
    _pdict->ma_keys = newKeysObject(MIN_SIZE_COMBINED);
  }
  MM_TX_END(_mm)
  _pptr = _mm->pptr(_pdict);
}

PPtr PMDict::getPPtr() { return _mm->pptr(_pdict); }

void PMDict::setProperty(std::string key, PPtr value_pptr, snapshotFlag flag) {
  const char *kstr = key.c_str();
  Logger::Debug("PMDict::setProperty: trying to set property %s\n", kstr);
  uint64_t khash = fixedHash(kstr);
  PDictKeysObject *keys = getKeys();
  PDictKeyEntry *ep = lookup(kstr, khash);
  MM_TX_BEGIN(_mm) {
    PPtr old_value_pptr = ep->me_value;
    PPtr me_key = ep->me_key;
    if (!PPTR_EQUALS(old_value_pptr, PPTR_NULL)) {
      assert(!PPTR_EQUALS(me_key, PPTR_NULL) &&
             !PPTR_EQUALS(me_key, PPTR_DUMMY));
      if (flag) _mm->snapshotRange(&(ep->me_value), sizeof(PPtr));
      ep->me_value = value_pptr;
    } else {
      PPtr key_pptr = _mm->persistString(key);
      if (flag) _mm->snapshotRange(ep, sizeof(PDictKeyEntry));
      if (PPTR_EQUALS(me_key, PPTR_NULL)) {
        if (keys->dk_usable <= 0) {
          insertionResize();
          keys = getKeys();
        }
        if (flag) _mm->snapshotRange(&(keys->dk_usable), sizeof(int64_t));
        keys->dk_usable -= 1;
        assert(keys->dk_usable >= 0);
        ep->me_key = key_pptr;
        ep->me_hash = khash;
      } else {
        assert(PPTR_EQUALS(me_key, PPTR_DUMMY));
        ep->me_key = key_pptr;
        ep->me_hash = khash;
      }
      if (flag) _mm->snapshotRange(&(_pdict->ma_used), sizeof(uint64_t));
      _pdict->ma_used += 1;
      ep->me_value = value_pptr;
      assert(!PPTR_EQUALS(ep->me_key, PPTR_NULL) &&
             !PPTR_EQUALS(ep->me_key, PPTR_DUMMY));
    }
  }
  MM_TX_END(_mm)
}

PPtr PMDict::getProperty(std::string key) {
  const char *kstr = key.c_str();
  Logger::Debug("PMDict::setProperty: trying to get property %s\n", kstr);
  uint64_t khash = fixedHash(kstr);
  PDictKeyEntry *ep = lookup(kstr, khash);
  if (PPTR_EQUALS(ep->me_key, PPTR_NULL)) {
    return PPTR_EMPTY;
  }
  return ep->me_value;
}

void PMDict::delProperty(std::string key, snapshotFlag flag) {
  const char *kstr = key.c_str();
  uint64_t khash = fixedHash(kstr);
  PDictKeyEntry *ep = lookup(kstr, khash);
  if (ep == nullptr || PPTR_EQUALS(ep->me_value, PPTR_NULL)) {
    return;
  }
  Logger::Debug("PMDict::delProperty: trying to delete property %s\n", kstr);
  MM_TX_BEGIN(_mm) {
    if (flag) _mm->snapshotRange(ep, sizeof(PDictKeyEntry));
    PPtr old_value_pptr = ep->me_value;
    ep->me_value = PPTR_NULL;
    if (flag) _mm->snapshotRange(&(_pdict->ma_used), sizeof(uint64_t));
    _pdict->ma_used -= 1;
    PPtr old_key_pptr = ep->me_key;
    ep->me_key = PPTR_DUMMY;
    _mm->free(old_key_pptr);
    _mm->free(old_value_pptr);
  }
  MM_TX_END(_mm)
}

std::list<std::shared_ptr<const void>> PMDict::getPropertyNames() {
  std::list<std::shared_ptr<const void>> names;
  PPtr ma_keys = _pdict->ma_keys;
  PDictKeysObject *keys = (PDictKeysObject *)_mm->direct(ma_keys);
  int64_t dk_size = keys->dk_size;
  PDictKeyEntry *ep0 = keys->dk_entries;
  PDictKeyEntry *ep;
  for (int i = 0; i < dk_size; ++i) {
    ep = ep0 + i;
    if (ep->me_key.pool_uuid_lo != 0) {
      names.push_back(std::make_shared<PPtr>(ep->me_key));
    }
  }
  return names;
}

void PMDict::_deallocate(){MM_TX_BEGIN(_mm){_mm->free(_pdict->ma_keys);
_mm->free(_pptr);
}
MM_TX_END(_mm)
}

PPtr PMDict::newKeysObject(uint64_t size) {
  assert(size > MIN_SIZE_SPLIT);
  PDictKeysObject *keys;
  MM_TX_BEGIN(_mm) {
    keys = (PDictKeysObject *)_mm->tx_zalloc(
        sizeof(PDictKeysObject) + sizeof(PDictKeyEntry) * (size - 1),
        PDICTKEYSOBJECT_TYPE_NUM);
    keys->dk_size = size;
    uint64_t usable = usableFraction(size);
    assert(usable < INT64_MAX);
    keys->dk_usable = usable;
    PDictKeyEntry *ep = keys->dk_entries;
    // Hash value of slot 0 is used by popitem, so it must be initialized
    ep->me_hash = 0;
    for (size_t i = 0; i < size; ++i) {
      (ep + i)->me_key = PPTR_NULL;
      (ep + i)->me_value = PPTR_NULL;
    }
  }
  MM_TX_END(_mm)

  return _mm->pptr(keys);
}

uint64_t PMDict::fixedHash(const char *key) {
  unsigned char digest[16];
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, key, strlen(key));
  MD5_Final(digest, &ctx);
  return ((uint64_t *)digest)[0] ^ ((uint64_t *)(digest + 8))[0];
}

PDictKeysObject *PMDict::getKeys() {
  return (PDictKeysObject *)_mm->direct(_pdict->ma_keys);
}

PDictKeyEntry *PMDict::lookup(const char *key, uint64_t khash) {
  while (true) {
    PPtr ma_keys = _pdict->ma_keys;
    PDictKeysObject *keys = (PDictKeysObject *)_mm->direct(ma_keys);

    ssize_t mask = keys->dk_size - 1;
    PDictKeyEntry *ep0 = keys->dk_entries;
    // find the index based on hash value
    ssize_t idx = khash & mask;
    PDictKeyEntry *ep = ep0 + idx;
    PPtr me_key = ep->me_key;

    PDictKeyEntry *freeslot;
    if (PPTR_EQUALS(me_key, PPTR_NULL)) {
      return ep;
    } else if (PPTR_EQUALS(me_key, PPTR_DUMMY)) {
      freeslot = ep;
    } else {
      if (ep->me_hash == khash) {
        char *str = (char *)_mm->direct(me_key) + sizeof(PStringObject);
        if (strcmp(str, key) == 0) {
          return ep;
        }
      }
      freeslot = nullptr;
    }
    uint64_t perturb = khash;

    while (true) {
      idx = (idx << 2) + idx + perturb + 1;
      ep = ep0 + (idx & mask);
      me_key = ep->me_key;
      if (PPTR_EQUALS(me_key, PPTR_NULL)) {
        return (freeslot == nullptr) ? ep : freeslot;
      }
      if (ep->me_hash == khash && !PPTR_EQUALS(me_key, PPTR_DUMMY)) {
        char *str = (char *)_mm->direct(me_key) + sizeof(PStringObject);
        ;
        if (strcmp(str, key) == 0) return ep;
      } else if (PPTR_EQUALS(me_key, PPTR_DUMMY) && freeslot == nullptr) {
        freeslot = ep;
      }
      perturb = perturb >> PERTURB_SHIFT;
    }
  }
}

void PMDict::insertionResize() {
  size_t minused = growRate();
  size_t newsize = MIN_SIZE_COMBINED;
  while (newsize <= minused && newsize > 0) {
    newsize = newsize << 1;
  }
  PDictKeysObject *old_keys = getKeys();
  PPtr old_keys_pptr = _pdict->ma_keys;

  MM_TX_BEGIN(_mm) {
    _mm->snapshotRange(&(_pdict->ma_keys), sizeof(PPtr));
    _pdict->ma_keys = newKeysObject(newsize);
    size_t oldsize = old_keys->dk_size;
    PDictKeyEntry *old_ep0 = old_keys->dk_entries;
    for (size_t i = 0; i < oldsize; ++i) {
      PDictKeyEntry *old_ep = old_ep0 + i;
      PPtr me_value = old_ep->me_value;
      if (!PPTR_EQUALS(me_value, PPTR_NULL)) {
        PPtr me_key = old_ep->me_key;
        assert(!PPTR_EQUALS(me_key, PPTR_DUMMY));
        uint64_t me_hash = old_ep->me_hash;
        PDictKeyEntry *new_ep = findEmptySlot(me_hash);
        new_ep->me_key = me_key;
        new_ep->me_hash = me_hash;
        new_ep->me_value = me_value;
      }
    }
    PDictKeysObject *new_keys = getKeys();
    new_keys->dk_usable -= _pdict->ma_used;
    _mm->free(old_keys_pptr);
  }
  MM_TX_END(_mm)
}

uint64_t PMDict::usableFraction(uint64_t size) {
  if (size >= UINT32_MAX) {
    return size / 3 * 2;
  }
  return (2 * size + 1) / 3;
}

uint64_t PMDict::growRate() {
  PDictKeysObject *keys = getKeys();
  assert(_pdict->ma_used < UINT32_MAX);
  assert(UINT64_MAX - (_pdict->ma_used << 2) > (keys->dk_size >> 1));
  return _pdict->ma_used * 2 + (getKeys()->dk_size >> 1);
}

PDictKeyEntry *PMDict::findEmptySlot(uint64_t khash) {
  PDictKeysObject *keys = getKeys();
  int64_t mask = keys->dk_size - 1;
  PDictKeyEntry *ep0 = keys->dk_entries;
  int64_t idx = khash & mask;
  PDictKeyEntry *ep = ep0 + idx;
  uint64_t perturb = khash;
  while (!PPTR_EQUALS(ep->me_key, PPTR_NULL)) {
    idx = (idx << 2) + idx + perturb + 1;
    ep = ep0 + (idx & mask);
    perturb = perturb >> PERTURB_SHIFT;
  }
  assert(PPTR_EQUALS(ep->me_key, PPTR_NULL));
  return ep;
}

}  // namespace impl
}  // namespace internal