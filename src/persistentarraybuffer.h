#ifndef PERSISTENTARRAYBUFFER_H
#define PERSISTENTARRAYBUFFER_H

#include <napi.h>
#include <memory>

#include "internal/pmarraybuffer.h"
#include "persistentobjectpool.h"

class PersistentArrayBuffer : public Napi::ObjectWrap<PersistentArrayBuffer> {
 public:
  static void init(Napi::Env env);
  static Napi::Object newInstance(Napi::Env env, PersistentObjectPool* pool,
                                  const Napi::Value value);
  static Napi::Object newInstance(Napi::Env env, PersistentObjectPool* pool,
                                  const void* data);

 public:
  PersistentArrayBuffer(const Napi::CallbackInfo& info);
  PersistentArrayBuffer(const PersistentArrayBuffer& other) = delete;
  PersistentArrayBuffer& operator=(const PersistentArrayBuffer& other) = delete;
  ~PersistentArrayBuffer();
  std::shared_ptr<const void> getPPtr(Napi::Env env);

 private:
  static Napi::FunctionReference constructor;

 private:
  Napi::Value getBuffer(const Napi::CallbackInfo& info);
  Napi::Value persist(const Napi::CallbackInfo& info);
  Napi::Value snapshot(const Napi::CallbackInfo& info);

  internal::PMArrayBuffer* _impl;
  PersistentObjectPool* _pool;
};

#endif