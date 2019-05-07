#include <exception>

#include "internal/pmarraybuffer.h"
#include "persistentarraybuffer.h"
#include "persistentobjectpool.h"
#include "util.h"

Napi::FunctionReference PersistentArrayBuffer::constructor;

PersistentArrayBuffer::PersistentArrayBuffer(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<PersistentArrayBuffer>(info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  _pool = info[0].As<Napi::External<PersistentObjectPool>>().Data();
  // construct by existing PersistentArrayBuffer
  try {
    if (info[1].IsExternal()) {
      void* data = info[1].As<Napi::External<void>>().Data();
      _impl = new internal::PMArrayBuffer(_pool->getMemoryManager(), data);
    }
    // construct by ArrayBuffer
    else if (info[1].IsArrayBuffer()) {
      void* data = info[1].As<Napi::ArrayBuffer>().Data();
      size_t length = info[1].As<Napi::ArrayBuffer>().ByteLength();
      // TODO: internal::PMArrayBuffer should justify whether data is already on
      // pmem
      _impl =
          new internal::PMArrayBuffer(_pool->getMemoryManager(), data, length);
    } else
      throw Napi::Error::New(
          env, "invalid argument to initialize PersistentArrayBuffer");
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to create PersistentArrayBuffer");
  }
};

PersistentArrayBuffer::~PersistentArrayBuffer() { delete _impl; }

void PersistentArrayBuffer::init(Napi::Env env) {
  Napi::HandleScope scope(env);
  Napi::Function func = DefineClass(
      env, "_PersistentArrayBuffer",
      {
          InstanceMethod("_get_buffer", &PersistentArrayBuffer::getBuffer),
          InstanceMethod("_persist", &PersistentArrayBuffer::persist),
          InstanceMethod("_snapshot", &PersistentArrayBuffer::snapshot),
      });
  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
}

Napi::Object PersistentArrayBuffer::newInstance(Napi::Env env,
                                                PersistentObjectPool* pool,
                                                const Napi::Value value) {
  Napi::EscapableHandleScope scope(env);
  ASSERT_TYPE(value.IsArrayBuffer());
  Napi::External<PersistentObjectPool> ext_pool =
      Napi::External<PersistentObjectPool>::New(env, pool);
  Napi::Object obj = constructor.New({ext_pool, value});
  return scope.Escape(napi_value(obj)).ToObject();
}

Napi::Object PersistentArrayBuffer::newInstance(Napi::Env env,
                                                PersistentObjectPool* pool,
                                                const void* data) {
  Napi::EscapableHandleScope scope(env);
  ASSERT_TYPE(value.IsArrayBuffer());
  Napi::External<void> ext_data = Napi::External<void>::New(env, (void*)data);
  Napi::External<PersistentObjectPool> ext_pool =
      Napi::External<PersistentObjectPool>::New(env, pool);
  Napi::Object obj = constructor.New({ext_pool, ext_data});
  return scope.Escape(napi_value(obj)).ToObject();
};

std::shared_ptr<const void> PersistentArrayBuffer::getPPtr(Napi::Env env) {
  try {
    return _impl->getPPtr();
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to retrieve PersistentArrayBuffer");
  }
};

Napi::Value PersistentArrayBuffer::getBuffer(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  try {
    void* buffer = _impl->getBuffer();
    size_t length = _impl->getLength();
    return Napi::ArrayBuffer::New(env, buffer, length);
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to retrieve PersistentArrayBuffer");
  }
}

Napi::Value PersistentArrayBuffer::persist(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  ASSERT_ARGS_LENGTH(info.Length() == 2);
  // TODO: notes in doc: NAPI do not support uint64, so maximum length of buffer
  // should be
  // less than uint32_max
  uint32_t offset = info[0].As<Napi::Number>().Uint32Value();
  uint32_t length = info[1].As<Napi::Number>().Uint32Value();
  try {
    _impl->persist(offset, length);
    return Napi::Value();
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to persist PersistentArrayBuffer");
  }
}

Napi::Value PersistentArrayBuffer::snapshot(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  ASSERT_ARGS_LENGTH(info.Length() == 2);
  // TODO: notes in doc:  NAPI do not support uint64, so maximum length of
  // buffer should be
  // less than uint32_max
  uint32_t offset = info[0].As<Napi::Number>().Uint32Value();
  uint32_t length = info[1].As<Napi::Number>().Uint32Value();
  try {
    _impl->snapshot(offset, length);
    return Napi::Value();
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to snapshot PersistentArrayBuffer");
  }
}