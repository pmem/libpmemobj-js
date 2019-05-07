#include <napi.h>
#include <stdio.h>
#include <exception>

#include "persistentarraybuffer.h"
#include "persistentobject.h"
#include "persistentobjectpool.h"
#include "util.h"

#define CHECK_POOL_IS_AVAILABLE()                                     \
  {                                                                   \
    if (_impl == nullptr) {                                           \
      throw Napi::Error::New(env, "pool not been opened or created"); \
    }                                                                 \
  }

Napi::FunctionReference PersistentObjectPool::constructor;

PersistentObjectPool::PersistentObjectPool(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<PersistentObjectPool>(info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  _path = info[0].As<Napi::String>().Utf8Value();
  _layout = info[1].As<Napi::String>().Utf8Value();
  _poolsize = info[2].As<Napi::Number>().Uint32Value();
  _mode = info[3].As<Napi::Number>().Uint32Value();
  _impl = nullptr;
};

void PersistentObjectPool::init(Napi::Env env) {
  Napi::HandleScope scope(env);
  Napi::Function func = DefineClass(
      env, "_PersistentObjectPool",
      {
          InstanceMethod("_check", &PersistentObjectPool::check),
          InstanceMethod("_open", &PersistentObjectPool::open),
          InstanceMethod("_create", &PersistentObjectPool::create),
          InstanceMethod("_get_root", &PersistentObjectPool::getRoot),
          InstanceMethod("_set_root", &PersistentObjectPool::setRoot),
          InstanceMethod("_create_object", &PersistentObjectPool::createObject),
          InstanceMethod("_create_arraybuffer",
                         &PersistentObjectPool::createArrayBuffer),
          InstanceMethod("_close", &PersistentObjectPool::close),
          InstanceMethod("_gc", &PersistentObjectPool::gc),
          InstanceMethod("_tx_begin", &PersistentObjectPool::tx_begin),
          InstanceMethod("_tx_commit", &PersistentObjectPool::tx_commit),
          InstanceMethod("_tx_abort", &PersistentObjectPool::tx_abort),
          InstanceMethod("_tx_end", &PersistentObjectPool::tx_end),
          InstanceMethod("_tx_stage", &PersistentObjectPool::tx_stage),
      });
  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
}

Napi::Object PersistentObjectPool::newInstance(Napi::Env env,
                                               const Napi::CallbackInfo& info) {
  Napi::EscapableHandleScope scope(env);
  // new PersistentObjectPool(path, layout, poolsize, mode);
  ASSERT_ARGS_LENGTH(info.Length() == 4);
  Napi::Object obj =
      constructor.New({info[0], info[1], info[2], info[3], info[4]});
  return scope.Escape(napi_value(obj)).ToObject();
}

Napi::Value PersistentObjectPool::check(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  int result = internal::PMObjectPool::check(_path, _layout);
  return Napi::Number::New(env, result);
}

internal::MemoryManager* PersistentObjectPool::getMemoryManager() {
  return _impl->getMemoryManager();
};

Napi::Value PersistentObjectPool::resurrect(
    Napi::Env env, std::shared_ptr<const void> pptr_ptr) {
  try {
    PERSISTENT_VALUE pvalue = _impl->getValue(pptr_ptr);
    Napi::Value result;
    if (pvalue.type == PERSISTENT_TYPE_NUMBER) {
      return Napi::Number::New(env, *((double*)(pvalue.data)));
    } else if (pvalue.type == PERSISTENT_TYPE_STRING) {
      return Napi::String::New(env, (char*)(pvalue.data));
    } else if (pvalue.type == PERSISTENT_TYPE_TRUE) {
      return Napi::Boolean::New(env, true);
    } else if (pvalue.type == PERSISTENT_TYPE_FALSE) {
      return Napi::Boolean::New(env, false);
    } else if (pvalue.type == PERSISTENT_TYPE_NULL) {
      return env.Null();
    } else if (pvalue.type == PERSISTENT_TYPE_UNDEFINED) {
      return env.Undefined();
    } else if (pvalue.type == PERSISTENT_TYPE_OBJECT) {
      return PersistentObject::newInstance(env, this, pvalue.data);
    } else if (pvalue.type == PERSISTENT_TYPE_ARRAYBUFFER) {
      return PersistentArrayBuffer::newInstance(env, this, pvalue.data);
    } else {
      throw Napi::Error::New(env, "unknown persistent type");
    }
  } catch (const char* errmsg) {
    tx_abort_context(env);
    throw Napi::Error::New(env, errmsg);
  };
}

std::shared_ptr<const void> PersistentObjectPool::persist(
    Napi::Env env, const Napi::Value value) {
  try {
    if (value.IsBuffer() || value.IsDataView() || value.IsFunction() ||
        value.IsPromise()) {
      throw Napi::Error::New(env, "unsupported type");
    } else if (value.IsNumber()) {
      return _impl->persistDouble(value.As<Napi::Number>().DoubleValue());
    } else if (value.IsString()) {
      return _impl->persistString(value.As<Napi::String>().Utf8Value());
    } else if (value.IsNull()) {
      return _impl->persistJSNull();
    } else if (value.IsUndefined()) {
      return _impl->persistUndefined();
    } else if (value.IsBoolean()) {
      return _impl->persistBoolean(value.As<Napi::Boolean>().Value());
    } else if (value.IsArrayBuffer()) {
      Napi::Object n_pab = PersistentArrayBuffer::newInstance(env, this, value);
      PersistentArrayBuffer* pab =
          Napi::ObjectWrap<PersistentArrayBuffer>::Unwrap(
              n_pab.As<Napi::Object>());
      return pab->getPPtr(env);
    } else if (value.IsObject()) {
      PersistentObject* pobj = nullptr;
      // if value is PersistentObject
      bool clear_cache = 0;
      if (_cache.empty()) clear_cache = 1;
      try {
        pobj = Napi::ObjectWrap<PersistentObject>::Unwrap(
            value.As<Napi::Object>());
      } catch (std::exception& e) {
        // value is JS Object
        Logger::Debug(
            "PersistentObjectPool::persist: try to persist JS object\n");
        for (auto it = _cache.begin(); it != _cache.end(); ++it){
          if (it->first == value){
            return it->second;
          }
        }
        Napi::Object n_obj = PersistentObject::newInstance(env, this, value);
        pobj = Napi::ObjectWrap<PersistentObject>::Unwrap(n_obj);
      }
      if (clear_cache) _cache.clear();
      return pobj->getPPtr(env);
    } else {
      throw Napi::Error::New(env, "unsupported type");
    }
  } catch (const char* errmsg) {
    tx_abort_context(env);
    throw Napi::Error::New(env, "failed to persist data as persistent ");
  }
};

void PersistentObjectPool::tx_enter_context(Napi::Env env) {
  CHECK_POOL_IS_AVAILABLE();
  try {
    int errnum = _impl->tx_begin();
    if (errnum) {
      throw Napi::Error::New(env, "failed to begin transaction");
    }
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to switch transaction state");
  }
}

void PersistentObjectPool::tx_exit_context(Napi::Env env) {
  CHECK_POOL_IS_AVAILABLE();
  try {
    if (_impl->tx_stage() == TX_STAGE_WORK) {
      _impl->tx_commit();
    }
    int errnum = _impl->tx_end();
    if (errnum) {
      throw Napi::Error::New(env, "failed to end transaction");
    }
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to switch transaction state");
  }
}

void PersistentObjectPool::tx_abort_context(Napi::Env env) {
  CHECK_POOL_IS_AVAILABLE();
  try {
    if (_impl->tx_stage() == TX_STAGE_WORK) {
      _impl->tx_abort();
      _impl->tx_end();
    }
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to switch transaction state");
  }
}

Napi::Value PersistentObjectPool::open(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (_impl != nullptr) {
    throw Napi::Error::New(env, "pool already opened or created");
  }
  try {
    _impl = new internal::PMObjectPool(_path, _layout);
    return Napi::Value();
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to open pool");
  }
}

Napi::Value PersistentObjectPool::create(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (_impl != nullptr) {
    throw Napi::Error::New(env, "pool already opened or created");
  }
  try {
    _impl = new internal::PMObjectPool(_path, _layout, _poolsize, _mode);
    return Napi::Value();
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to create pool");
  }
}

Napi::Value PersistentObjectPool::getRoot(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  try {
    Napi::Value result = resurrect(env, _impl->getRoot());
    return result;
  } catch (const char* errmsg) {
    tx_abort_context(env);
    throw Napi::Error::New(env, "failed to get root");
  }
}

Napi::Value PersistentObjectPool::setRoot(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  ASSERT_ARGS_LENGTH(info.Length() == 1);
  try {
    _impl->setRoot(persist(env, info[0]));
  } catch (const char* errmsg) {
    tx_abort_context(env);
    throw Napi::Error::New(env, "failed to set root");
  }

  return Napi::Value();
}

Napi::Value PersistentObjectPool::createObject(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  ASSERT_ARGS_LENGTH(info.Length() <= 1);
  Napi::Value value = (info.Length() == 1) ? info[0] : Napi::Object::New(env);
  return PersistentObject::newInstance(env, this, value);
}

Napi::Value PersistentObjectPool::createArrayBuffer(
    const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  ASSERT_ARGS_LENGTH(info.Length() == 1);
  return PersistentArrayBuffer::newInstance(env, this, info[0]);
}

Napi::Value PersistentObjectPool::close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  try {
    _impl->close();
    delete _impl;
    _impl = nullptr;
    return Napi::Value();
  } catch (const char* errmsg) {
    tx_abort_context(env);
    throw Napi::Error::New(env, "failed to close pool");
  }
}

Napi::Value PersistentObjectPool::gc(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  try {
    _impl->gc();
    return Napi::Value();
  } catch (const char* errmsg) {
    tx_abort_context(env);
    throw Napi::Error::New(env, "failed to gc");
  }
}

Napi::Value PersistentObjectPool::tx_begin(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  return Napi::Number::New(env, _impl->tx_begin());
}

Napi::Value PersistentObjectPool::tx_commit(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  _impl->tx_commit();
  return Napi::Value();
}

Napi::Value PersistentObjectPool::tx_abort(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  _impl->tx_abort();
  return Napi::Value();
}

Napi::Value PersistentObjectPool::tx_end(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  return Napi::Number::New(env, _impl->tx_end());
}

Napi::Value PersistentObjectPool::tx_stage(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  CHECK_POOL_IS_AVAILABLE();
  return Napi::Number::New(env, _impl->tx_stage());
}