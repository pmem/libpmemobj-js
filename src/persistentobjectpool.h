#ifndef PERSISTENTOBJECTPOOL_H
#define PERSISTENTOBJECTPOOL_H

#include <napi.h>
#include <map>
#include <memory>

#include "internal/pmobjectpool.h"

class PersistentObjectPool : public Napi::ObjectWrap<PersistentObjectPool> {
 public:
  static void init(Napi::Env env);
  static Napi::Object newInstance(Napi::Env env,
                                  const Napi::CallbackInfo& info);

 public:
  PersistentObjectPool(const Napi::CallbackInfo& info);
  internal::MemoryManager *getMemoryManager();
  Napi::Value resurrect(Napi::Env env, std::shared_ptr<const void>);
  std::shared_ptr<const void> persist(Napi::Env env, const Napi::Value value);
  void tx_enter_context(Napi::Env env);
  void tx_exit_context(Napi::Env env);
  void tx_abort_context(Napi::Env env);

  std::map<Napi::Value, std::shared_ptr<const void>> _cache;

 private:
  static Napi::FunctionReference constructor;

 private:
  Napi::Value check(const Napi::CallbackInfo& info);
  Napi::Value open(const Napi::CallbackInfo& info);
  Napi::Value create(const Napi::CallbackInfo& info);
  Napi::Value getRoot(const Napi::CallbackInfo& info);
  Napi::Value setRoot(const Napi::CallbackInfo& info);
  Napi::Value createObject(const Napi::CallbackInfo& info);
  Napi::Value createArrayBuffer(const Napi::CallbackInfo& info);
  Napi::Value close(const Napi::CallbackInfo& info);
  Napi::Value gc(const Napi::CallbackInfo& info);

  Napi::Value tx_begin(const Napi::CallbackInfo& info);
  Napi::Value tx_commit(const Napi::CallbackInfo& info);
  Napi::Value tx_abort(const Napi::CallbackInfo& info);
  Napi::Value tx_end(const Napi::CallbackInfo& info);
  Napi::Value tx_stage(const Napi::CallbackInfo& info);

  std::string _path;
  std::string _layout;
  // TODO: note in doc: the maximum poolsize should be uint32_max
  uint32_t _poolsize;
  mode_t _mode;

  internal::PMObjectPool *_impl;
};

#endif