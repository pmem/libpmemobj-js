#ifndef PERSISTENTOBJECT_H
#define PERSISTENTOBJECT_H

#include <napi.h>

#include "internal/pmobject.h"
#include "persistentobjectpool.h"

class PersistentObject : public Napi::ObjectWrap<PersistentObject> {
 public:
  static void init(Napi::Env env);
  static Napi::Object newInstance(Napi::Env env, PersistentObjectPool* pool,
                                  const Napi::Value value);
  static Napi::Object newInstance(Napi::Env env, PersistentObjectPool* pool,
                                  const void* data);

 public:
  PersistentObject(const Napi::CallbackInfo& info);
  PersistentObject(const PersistentObject& other) = delete;
  PersistentObject& operator=(const PersistentObject& other) = delete;
  ~PersistentObject();
  std::shared_ptr<const void> getPPtr(Napi::Env env);

 private:
  static Napi::FunctionReference constructor;

 private:
  // NAPI cannot tell wether a Napi::Value is a uint32, so we
  // have to wrap it in a Napi::Object like {index: null}
  Napi::Value getProperty(const Napi::CallbackInfo& info);
  Napi::Value setProperty(const Napi::CallbackInfo& info);
  Napi::Value delProperty(const Napi::CallbackInfo& info);
  Napi::Value getPropertyNames(const Napi::CallbackInfo& info);
  // Array methods
  Napi::Value push(const Napi::CallbackInfo& info);
  Napi::Value pop(const Napi::CallbackInfo& info);
  Napi::Value isArray(const Napi::CallbackInfo& info);
  Napi::Value getLength(const Napi::CallbackInfo& info);
  Napi::Value setLength(const Napi::CallbackInfo& info);

  internal::PMObject* _impl;
  PersistentObjectPool* _pool;
};

#endif