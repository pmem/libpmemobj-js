'use strict'
const fs = require('fs');
const jspmdk = require('bindings')('jspmdk');
const layout_version = 'jspmdk-0.0.1';
const constants = jspmdk.constants;

var sym_pool = Symbol('pool');
var sym_pobj = Symbol('pobj');
var sym_pab = Symbol('pab');

function isValidString(str) {
  var reg =
      /[\x09\x0A\x0D\x20-\x7E]|([\xC2-\xDF][\x80-\xBF])|(\xE0[\xA0-\xBF][\x80-\xBF])|([\xE1-\xEC\xEE\xEF][\x80-\xBF]{2})|(\xED[\x80-\x9F][\x80-\xBF])|(\xF0[\x90-\xBF][\x80-\xBF]{2})|([\xF1-\xF3][\x80-\xBF]{3})|(\xF4[\x80-\x8F][\x80-\xBF]{2})*/;
  return (reg.test(str) && !/\0/.test(str));
  }

class PersistentArrayBuffer {
  snapshot(offset, length) {
    this[sym_pab]._snapshot(offset, length);
  }

  persist(offset, length) {
    this[sym_pab]._persist(offset, length);
  }
  }

class PersistentObject {
  constructor(pobj) {
    this[sym_pobj] = pobj;
  }

  is_array() {
    return this[sym_pobj]._is_array();
  }

  push(item) {
    if (this[sym_pobj]._is_array()) {
      if (item && (item.constructor.name == 'PersistentObject')) {
        item = item[sym_pobj];
      }
      this[sym_pobj]._push(item);
      }
    else {
      throw new Error('push is not a function');
    }
  }

  pop() {
    if (this[sym_pobj]._is_array()) {
      return this[sym_pobj]._pop();
      }
    else {
      throw new Error('push is not a function');
    }
  }
  }

const PersistentObjectProxyHandler = {
  get: function(target, prop) {
    if (target.is_array() && prop == 'length') {
      return target[sym_pobj]._get_length();
      }
    if (prop == 'inspect' || prop == 'valueOf') {
      return target[prop];
      }
    if (typeof(prop) == 'string') {
      if (!target[sym_pobj]) throw new Error('invalid PersistentObject');
      // since NAPI cannot tell wether a number is a uint32, we have to call
      // _get_property({prop: null}) rather than _get_property(prop)
      var arg = {};
      arg[prop] = null;
      var obj;
      try {
        obj = target[sym_pobj]._get_property(arg);
        }
      catch (err) {
        if (err.message == 'key not found') {
          return target[prop];
          }
        else
          throw new Error(err.message);
        }
      if (obj != undefined && obj.constructor.name == '_PersistentObject') {
        obj =
            new Proxy(new PersistentObject(obj), PersistentObjectProxyHandler);
        }
      return obj;
      }
    else {
      return target[prop];
    }
  },

  set: function(target, prop, value) {
    if (target.is_array() && prop == 'length') {
      if (!Number.isInteger(value)) throw 'Invalid array length'
        target[sym_pobj]._set_length(value);
      return true;
      }
    if (typeof(prop) == 'string') {
      if (!isValidString(prop) ||
          (typeof(value) == 'string' && !isValidString(value)))
        throw new Error('invalid characters');

      if (!target[sym_pobj]) throw new Error('invalid PersistentObject');
      // since NAPI cannot tell wether a number is a uint32, we have to call
      // _set_property({prop: value}) rather than _set_property(prop, value)
      var arg = {};
      if (value && value.constructor.name == 'PersistentObject') {
        value = value[sym_pobj];
      }
      arg[prop] = value;
      target[sym_pobj]._set_property(arg);
      }
    else {
      target[prop] = value;
      }
    return true;
  },

  deleteProperty: function(target, prop) {
    if (typeof(prop) == 'string') {
      if (!target[sym_pobj]) throw new Error('invalid PersistentObject');
      // since NAPI cannot tell wether a number is a uint32, we have to call
      // _del_property({prop: null}) rather than _del_property(prop)
      var arg = {};
      arg[prop] = null;
      target[sym_pobj]._del_property(arg);
      }
    else {
      delete target[prop];
      }
    return true;
  },

  // Object.getOwnPropertyNames([]) should be ["length"]
  // Object.getOwnPropertyNames([1,2,3]) should be ["0", "1", "2", "length"]
  ownKeys: function(target) {
    return target[sym_pobj]._get_property_names();
  },
};

class PersistentObjectPool {
  constructor(pool) {
    this[sym_pool] = pool;
    this._closed = true;
  }
  open() {
    if (!this._closed) throw new Error('pool already created or opened');
    this[sym_pool]._open();
    this._closed = false;
  }
  create() {
    if (!this._closed) throw new Error('pool already created or opened');
    this[sym_pool]._create();
    this._closed = false;
  }
  // TODO: document that it does not support create object by persistent
  // object
  create_arraybuffer(buffer) {
    if (this._closed) throw new Error('pool not opened or already closed');
    var _pab = this[sym_pool]._create_arraybuffer(buffer);
    var pab = _pab._get_buffer();  // ArrayBuffer
    pab = new ArrayBuffer(10);
    pab[sym_pab] = _pab;
    pab.snapshot = PersistentArrayBuffer.prototype.snapshot;
    pab.persist = PersistentArrayBuffer.prototype.persist;
    return pab;
  }
  create_object(js_obj) {
    if (this._closed) throw new Error('pool not opened or already closed');
    var _pobj = this[sym_pool]._create_object(js_obj);
    return new Proxy(new PersistentObject(_pobj), PersistentObjectProxyHandler);
  }
  close() {
    if (this._closed) throw new Error('pool not opened or already closed');
    this[sym_pool]._close();
    this._closed = true;
  }
  check() {
    return this[sym_pool]._check();
  }
  gc() {
    if (this._closed) throw new Error('pool not opened or already closed');
    this[sym_pool]._gc();
  }
  // TODO: document this process is sync
  transaction(run) {
    if (this._closed) throw new Error('pool not opened or already closed');
    this[sym_pool]._tx_begin();
    run();
    var stage = this[sym_pool]._tx_stage();
    if (stage == constants.TX_STAGE_WORK) {
      this[sym_pool]._tx_commit();
      this[sym_pool]._tx_end();
      }
    else {
      this[sym_pool]._tx_abort();
      throw new Error('transaction aborted');
    }
  }
  tx_begin() {
    if (this._closed) throw new Error('pool not opened or already closed');
    this[sym_pool]._tx_begin();
  }
  tx_commit() {
    if (this._closed) throw new Error('pool not opened or already closed');
    this[sym_pool]._tx_commit();
  }
  tx_end() {
    if (this._closed) throw new Error('pool not opened or already closed');
    this[sym_pool]._tx_end();
  }
  tx_abort() {
    if (this._closed) throw new Error('pool not opened or already closed');
    this[sym_pool]._tx_abort();
  }
  tx_stage() {
    if (this._closed) throw new Error('pool not opened or already closed');
    return this[sym_pool]._tx_stage();
  }
  }

const PersistentObjectPoolProxyHandler = {
  get: function(target, prop) {
    if (prop == 'root') {
      if (!target[sym_pool]) throw new Error('pool had been close');
      var root = target[sym_pool]._get_root();
      if (root != undefined && root.constructor.name == '_PersistentObject') {
        root =
            new Proxy(new PersistentObject(root), PersistentObjectProxyHandler);
        }
      return root;
      }
    else {
      return target[prop];
    }
  },

  set: function(target, prop, value) {
    if (prop == 'root') {
      if (!target[sym_pool]) throw new Error('pool had been closed');
      if (value && value.constructor.name == 'PersistentObject') {
        value = value[sym_pobj];
      }
      target[sym_pool]._set_root(value);
      }
    else {
      target[prop] = value;
      }
    return true;
  },
};


var new_pool = function(path, size, mode) {
  path = path || '';
  size = size || 0;
  var mode = mode || (fs.constants.S_IRUSR | fs.constants.S_IWUSR);
  var _pool = jspmdk.new_pool(path, layout_version, size, mode);
  return new Proxy(
      new PersistentObjectPool(_pool), PersistentObjectPoolProxyHandler);
};


var bind = function(obj, fn) {
  if (obj != undefined && obj[sym_pobj] != undefined) {
    var new_obj = {};
    new_obj[sym_pobj] = obj[sym_pobj];
    // attach is_array() method to new object
    if (!fn.prototype.is_array) {
      new_obj.is_array = PersistentObject.prototype.is_array;
      }
    // attach push() method to new object
    if (!fn.prototype.push) {
      new_obj.push = PersistentObject.prototype.push;
      }
    // attach pop() method to new object()
    if (!fn.prototype.pop) {
      new_obj.pop = PersistentObject.prototype.pop;
    }
    Object.setPrototypeOf(new_obj, fn.prototype);
    return new Proxy(new_obj, PersistentObjectProxyHandler);
    }
  else {
    throw new Error('can not bind non-PersistentObject');
  }
};

exports.new_pool = new_pool;
exports.constants = constants;
exports.bind = bind;