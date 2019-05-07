#include <exception>
#include <list>

#include "internal/pmobjectpool.h"
#include "persistentobject.h"
#include "util.h"

Napi::FunctionReference PersistentObject::constructor;

PersistentObject::PersistentObject(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<PersistentObject>(info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  _pool = info[0].As<Napi::External<PersistentObjectPool>>().Data();
  // construct by existing PersistentObject
  if (info[1].IsExternal()) {
    void* data = info[1].As<Napi::External<void>>().Data();
    try {
      _impl = new internal::PMObject(_pool->getMemoryManager(), data);
    } catch (const char* errmsg) {
      _pool->tx_abort_context(env);
      throw Napi::Error::New(env, "failed to create PersistentObject");
    }
  }
  // construct by Object or Array
  else if (info[1].IsObject()) {
    // using transaction here can help to improve the performance
    Logger::Debug(
        "PersistentObject::PersistentObject: constructed by JS object\n");
    try {
      _pool->tx_enter_context(env);
      internal::MemoryManager* mm = _pool->getMemoryManager();
      _impl = new internal::PMObject(mm, info[1].IsArray());
      _pool->_cache.insert(
          std::map<Napi::Value, std::shared_ptr<const void>>::value_type(
              info[1], _impl->getPPtr()));
      Napi::Object obj = info[1].As<Napi::Object>();
      Napi::Array props = obj.GetPropertyNames();
      for (uint32_t i = 0; i < props.Length(); ++i) {
        Napi::Value key = props.Get(i);
        Napi::Value value = obj.Get(key);
        if (key.IsString()) {
          Logger::Debug(
              "PersistentObject::PersistentObject: setting property key = %s\n",
              key.As<Napi::String>().Utf8Value().c_str());
          _impl->setProperty(key.As<Napi::String>().Utf8Value(),
                             _pool->persist(env, value), kNotSnapshot);
        } else if (key.IsNumber()) {
          Logger::Debug(
              "PersistentObject::PersistentObject: setting property key = %d\n",
              key.As<Napi::Number>().Uint32Value());
          _impl->setProperty(key.As<Napi::Number>().Uint32Value(),
                             _pool->persist(env, value), kNotSnapshot);
        }
      }
      _pool->tx_exit_context(env);
    } catch (const char* errmsg) {
      _pool->tx_abort_context(env);
      throw Napi::Error::New(env, "failed to create PersistentObject");
    }
  } else {
    throw Napi::Error::New(env,
                           "invalid argument to initialize PersistentObject");
  }
};

PersistentObject::~PersistentObject() { delete _impl; }

void PersistentObject::init(Napi::Env env) {
  Napi::HandleScope scope(env);
  Napi::Function func = DefineClass(
      env, "_PersistentObject",
      {
          InstanceMethod("_get_property", &PersistentObject::getProperty),
          InstanceMethod("_set_property", &PersistentObject::setProperty),
          InstanceMethod("_del_property", &PersistentObject::delProperty),
          InstanceMethod("_get_property_names",
                         &PersistentObject::getPropertyNames),
          InstanceMethod("_set_length", &PersistentObject::setLength),
          InstanceMethod("_get_length", &PersistentObject::getLength),
          InstanceMethod("_push", &PersistentObject::push),
          InstanceMethod("_pop", &PersistentObject::pop),
          InstanceMethod("_is_array", &PersistentObject::isArray),
      });
  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
}

Napi::Object PersistentObject::newInstance(Napi::Env env,
                                           PersistentObjectPool* pool,
                                           const Napi::Value value) {
  Napi::EscapableHandleScope scope(env);
  // new PersistentObject(js_obj);
  Napi::External<PersistentObjectPool> ext_pool =
      Napi::External<PersistentObjectPool>::New(env, pool);
  Napi::Object obj = constructor.New({ext_pool, value});
  return scope.Escape(napi_value(obj)).ToObject();
}

Napi::Object PersistentObject::newInstance(Napi::Env env,
                                           PersistentObjectPool* pool,
                                           const void* data) {
  Napi::EscapableHandleScope scope(env);
  // new PersistentObject(void *data);
  Napi::External<PersistentObjectPool> ext_pool =
      Napi::External<PersistentObjectPool>::New(env, pool);
  Napi::External<void> ext_data = Napi::External<void>::New(env, (void*)data);
  Napi::Object obj = constructor.New({ext_pool, ext_data});
  return scope.Escape(napi_value(obj)).ToObject();
}

std::shared_ptr<const void> PersistentObject::getPPtr(Napi::Env env) {
  return _impl->getPPtr();
}

Napi::Value PersistentObject::getProperty(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object arg = info[0].As<Napi::Object>();
  Napi::Value key = arg.GetPropertyNames().Get((uint32_t)0);
  ASSERT_TYPE(key.IsNumber() || key.IsString());
  try {
    if (key.IsString()) {
      Logger::Debug(
          "PersistentObject::PersistentObject: getting property key = %s\n",
          key.As<Napi::String>().Utf8Value().c_str());
      return _pool->resurrect(
          env, _impl->getProperty(key.As<Napi::String>().Utf8Value()));
    } else {
      return _pool->resurrect(
          env, _impl->getProperty(key.As<Napi::Number>().Uint32Value()));
    }
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to get property");
  }
}

Napi::Value PersistentObject::setProperty(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object arg = info[0].As<Napi::Object>();
  Napi::Value key = arg.GetPropertyNames().Get((uint32_t)0);
  Napi::Value value = arg.Get(key);
  ASSERT_TYPE(key.IsNumber() || key.IsString());
  // snapshot here is not necessary
  // using transaction here can help to improve the performance
  try {
    if (key.IsString()) {
      _impl->setProperty(key.As<Napi::String>().Utf8Value(),
                         _pool->persist(env, value), kSnapshot);
    } else {
      _impl->setProperty(key.As<Napi::Number>().Uint32Value(),
                         _pool->persist(env, value), kSnapshot);
    }
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to set property");
  }
  return Napi::Value();
}

Napi::Value PersistentObject::delProperty(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object arg = info[0].As<Napi::Object>();
  Napi::Value key = arg.GetPropertyNames().Get((uint32_t)0);
  ASSERT_TYPE(key.IsNumber() || key.IsString());
  // snapshot here is not necessary
  // using transaction here can help to improve the performance
  try {
    if (key.IsString()) {
      _impl->delProperty(key.As<Napi::String>().Utf8Value(), kSnapshot);
    } else {
      _impl->delProperty(key.As<Napi::Number>().Uint32Value(), kSnapshot);
    }
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to delete property");
  }
  return Napi::Value();
}

Napi::Value PersistentObject::getPropertyNames(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Array result = Napi::Array::New(env);
  try {
    std::list<std::shared_ptr<const void>> names = _impl->getPropertyNames();
    uint32_t index = 0;
    for (auto it = names.begin(); it != names.end(); ++it) {
      std::shared_ptr<const void> data = *it;
      result.Set(index, _pool->resurrect(env, data));
      index += 1;
    }
    if (_impl->isArray()) {
      result.Set(index, Napi::String::New(env, "length"));
      index += 1;
      std::list<uint32_t> indexes = _impl->getValidIndex();
      for (auto it = indexes.begin(); it != indexes.end(); ++it) {
        uint32_t data = *it;
        result.Set(index, Napi::Number::New(env, data).ToString());
        index += 1;
      }
    }
    return result;
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to get properties names");
  }
}

Napi::Value PersistentObject::push(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  try {
    _impl->push(_pool->persist(env, info[0]));
    return Napi::Value();
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to push");
  }
}

Napi::Value PersistentObject::pop(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  try {
    Napi::Value result = _pool->resurrect(env, _impl->pop());
    return result;
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to pop");
  }
}

Napi::Value PersistentObject::isArray(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  try {
    if (_impl->isArray()) {
      return Napi::Boolean::New(env, true);
    } else {
      return Napi::Boolean::New(env, false);
    }
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to tell wether it is PersistentArray");
  }
}

Napi::Value PersistentObject::getLength(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  try {
    Napi::Value result = Napi::Number::New(env, _impl->getLength());
    return result;
  } catch (const char* errmsg) {
    throw Napi::Error::New(env, "failed to get length");
  }
}

Napi::Value PersistentObject::setLength(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  try {
    _impl->setLength(info[0].As<Napi::Number>().Uint32Value());
    return Napi::Value();
  } catch (const char* errmsg) {
    _pool->tx_abort_context(env);
    throw Napi::Error::New(env, "failed to set length");
  }
}