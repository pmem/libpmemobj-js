#include "pmarraybuffer.h"
#include "common.h"

namespace internal {
PMArrayBuffer::PMArrayBuffer(MemoryManager *mm, void *data) {
  _mm = mm;
  _pptr = *((PPtr *)data);
  _pab = (PArrayBufferObject *)_mm->direct(_pptr);

}

PMArrayBuffer::PMArrayBuffer(MemoryManager *mm, void *data, uint32_t length) {
  _mm = mm;
  if (_mm->inTransaction()) {
    _pab = (PArrayBufferObject *)_mm->tx_zalloc(
        sizeof(PArrayBufferObject) + length, POBJ_TYPE_NUM);
    ((PObject *)_pab)->ob_type = TYPE_CODE_ARRAYBUFFER;
    _pab->ob_length = length;
    memcpy(((char *)_pab) + sizeof(PArrayBufferObject), data, length);
  } else {
    _pab = (PArrayBufferObject *)_mm->zalloc(
        sizeof(PArrayBufferObject) + length, POBJ_TYPE_NUM);
    ((PObject *)_pab)->ob_type = TYPE_CODE_ARRAYBUFFER;
    _pab->ob_length = length;
    memcpy(((char *)_pab) + sizeof(PArrayBufferObject), data, length);
    _mm->persist(_pab, sizeof(PArrayBufferObject) + length);
  }
  _pptr = _mm->pptr(_pab);
}

std::shared_ptr<const void> PMArrayBuffer::getPPtr() {
  return std::make_shared<PPtr>(_pptr);
}

void *PMArrayBuffer::getBuffer() {
  return ((char *)_pab) + sizeof(PArrayBufferObject);
}

uint32_t PMArrayBuffer::getLength() { return _pab->ob_length; }

void PMArrayBuffer::persist(uint32_t offset, uint32_t length) {
  _mm->persist(((char *)_pab) + sizeof(PArrayBufferObject) + offset, length);
}

void PMArrayBuffer::snapshot(uint32_t offset, uint32_t length) {
  _mm->snapshotRange(((char *)_pab) + sizeof(PArrayBufferObject) + offset,
                     length);
}
}