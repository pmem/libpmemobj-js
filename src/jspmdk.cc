#include <napi.h>

#include "persistentarraybuffer.h"
#include "persistentobject.h"
#include "persistentobjectpool.h"

Napi::Value newPool(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return PersistentObjectPool::newInstance(env, info);
}

Napi::Object initModule(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "new_pool"),
              Napi::Function::New(env, newPool));
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("MIN_POOL_SIZE", Napi::Number::New(env, PMEMOBJ_MIN_POOL));
  obj.Set("TX_STAGE_NONE", Napi::Number::New(env, TX_STAGE_NONE));
  obj.Set("TX_STAGE_WORK", Napi::Number::New(env, TX_STAGE_WORK));
  obj.Set("TX_STAGE_ONCOMMIT", Napi::Number::New(env, TX_STAGE_ONCOMMIT));
  obj.Set("TX_STAGE_ONABORT", Napi::Number::New(env, TX_STAGE_ONABORT));
  obj.Set("TX_STAGE_FINALLY", Napi::Number::New(env, TX_STAGE_FINALLY));
  exports.Set(Napi::String::New(env, "constants"), obj);
  return exports;
}

Napi::Object initAll(Napi::Env env, Napi::Object exports) {
  PersistentObjectPool::init(env);
  PersistentObject::init(env);
  PersistentArrayBuffer::init(env);
  return initModule(env, exports);
}

NODE_API_MODULE(jspmdk, initAll)