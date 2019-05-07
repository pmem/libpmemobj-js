#include "pmobject.h"
#include "common.h"
#include "pmarray.h"
#include "pmdict.h"

namespace internal {
PMObject::PMObject(MemoryManager* mm, void* data) {
  _mm = mm;
  _pptr = *((PPtr*)data);
  Logger::Debug("PMObject::PMObject: construct by (%llu, %llu)\n",
                _pptr.pool_uuid_lo, _pptr.off);
  _pobj = (PObjectObject*)_mm->direct(_pptr);
  PObject* pobj = (PObject*)_mm->direct(_pobj->elements);

  if (pobj->ob_type == TYPE_CODE_ARRAY) {
    _elements = new impl::PMSimpleArray(_mm, _pobj->elements);
  } else if (pobj->ob_type == TYPE_CODE_NUMDICT) {
    _elements = new impl::PMNumDict(_mm, _pobj->elements);
  } else {
    // TODO: throw an error
    _elements = nullptr;
  }
  _extra_props = new impl::PMDict(_mm, _pobj->extra_props);
}

PMObject::PMObject(MemoryManager* mm, bool is_array) {
  _mm = mm;
  MM_TX_BEGIN(_mm) {
    _pobj = (PObjectObject*)_mm->tx_zalloc(sizeof(PObjectObject));
    ((PObject*)_pobj)->ob_type = TYPE_CODE_OBJECT;
    _extra_props = new impl::PMDict(_mm);
    _pobj->extra_props = _extra_props->getPPtr();
    _elements = new impl::PMSimpleArray(_mm);
    _pobj->elements = _elements->getPPtr();
    _pobj->is_array = is_array;
    _pptr = _mm->pptr(_pobj);
  }
  MM_TX_END(_mm)
  Logger::Debug("PMObject::PMObject: creating empty object at (%llu, %llu)\n",
                _pptr.pool_uuid_lo, _pptr.off);
}

PMObject::~PMObject() {
  delete _elements;
  delete _extra_props;
}

std::shared_ptr<const void> PMObject::getPPtr() {
  return std::make_shared<PPtr>(_pptr);
}

void PMObject::setProperty(std::string key,
                           std::shared_ptr<const void> value_pptr_ptr,
                           snapshotFlag flag) {
  _extra_props->setProperty(key, *((PPtr*)value_pptr_ptr.get()), flag);
}

void PMObject::setProperty(uint32_t index,
                           std::shared_ptr<const void> value_pptr_ptr,
                           snapshotFlag flag) {
  if (_elements->shouldConvertToNumDict(index)) {
    MM_TX_BEGIN(_mm) {
      impl::PMNumDict* new_elements =
          (impl::PMNumDict*)_elements->convertToNumDict();
      _mm->snapshotRange(&(_pobj->elements), sizeof(PPtr));
      _pobj->elements = new_elements->getPPtr();
      delete ((impl::PMSimpleArray*)_elements);
      _elements = new_elements;
    }
    MM_TX_END(_mm)
  } else if (_elements->shouldConvertToSimpleArray(index)) {
    MM_TX_BEGIN(_mm) {
      impl::PMSimpleArray* new_elements =
          (impl::PMSimpleArray*)_elements->convertToSimpleArray();
      _mm->snapshotRange(&(_pobj->elements), sizeof(PPtr));
      _pobj->elements = new_elements->getPPtr();
      delete ((impl::PMNumDict*)_elements);
      _elements = new_elements;
    }
    MM_TX_END(_mm)
  }
  _elements->setProperty(index, *((PPtr*)value_pptr_ptr.get()), flag);
}

std::shared_ptr<const void> PMObject::getProperty(std::string key) {
  PPtr pptr = _extra_props->getProperty(key);
  return std::make_shared<PPtr>(pptr);
}

std::shared_ptr<const void> PMObject::getProperty(uint32_t index) {
  PPtr pptr = _elements->getProperty(index);
  return std::make_shared<PPtr>(pptr);
}

void PMObject::delProperty(std::string key, snapshotFlag flag) {
  _extra_props->delProperty(key, flag);
}

void PMObject::delProperty(uint32_t index, snapshotFlag flag) {
  _elements->delProperty(index, flag);
}

std::list<std::shared_ptr<const void>> PMObject::getPropertyNames() {
  return _extra_props->getPropertyNames();
}

std::list<uint32_t> PMObject::getValidIndex() {
  return _elements->getValidIndex();
}

void PMObject::push(std::shared_ptr<const void> data) {
  _elements->push(*((PPtr*)data.get()));
}

std::shared_ptr<const void> PMObject::pop() { return _elements->pop(); }

bool PMObject::isArray() { return _pobj->is_array; }

uint32_t PMObject::getLength() { return _elements->getLength(); }

void PMObject::setLength(uint32_t new_length) {
  _elements->setLength(new_length);
}

void PMObject::_deallocate() {
  MM_TX_BEGIN(_mm) {
    _mm->free(_pptr); 
  }
  MM_TX_END(_mm)
}

}  // namespace internal
