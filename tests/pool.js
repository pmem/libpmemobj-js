const assert = require('assert');
const common = require('./common');
const jspmdk = require('../src/jspmdk');
const constants = jspmdk.constants;

var valid_path = common.config.valid_path + '/file0';
var invalid_path = common.config.invalid_path + '/file0';

describe('pool object', () => {
  beforeEach(async function() {
    await common.resetFolder();
  });

  it('should throw an error due to invalid path on create', function() {
    try {
      var pool = jspmdk.new_pool(invalid_path, constants.MIN_POOL_SIZE);
      pool.create();
      assert.fail('failed to throw an error due to invalid path on create')
      }
    catch (err) {
      assert(err.message == 'failed to create pool');
    }
  });

  it('should throw an error due to invalid poolsize on create', function() {
    try {
      var pool = jspmdk.new_pool(valid_path, 0);
      pool.create();
      assert.fail('failed to throw an error due to invalid poolsize on create');
      }
    catch (err) {
      assert(err.message == 'failed to create pool');
    }
  });

  it('should throw an error due to pool already exist on create', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    try {
      var new_pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
      new_pool.create();
      assert.fail(
          'failed to throw an error due to pool already exist on create');
      }
    catch (err) {
      assert(err.message == 'failed to create pool');
    }
    pool.close();
  });

  it('should return a pool object on created', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    assert(pool.constructor.name = 'PersistentObjectPool');
    pool.close();
  });

  it('should pass check() after create', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    pool.close();
    assert(pool.check() == 1);
  });

  it('should throw an error due to invalid path on open', function() {
    try {
      var pool = jspmdk.new_pool(invalid_path, 0);
      pool.open();
      assert.fail('failed to throw an error due to invalid path on open');
      }
    catch (err) {
      assert(err.message == 'failed to open pool');
    }
  });

  it('should return a pool object when reopening an existing pool', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    pool.close();
    var new_pool = jspmdk.new_pool(valid_path, 0);
    new_pool.open();
    assert(new_pool.constructor.name = 'PersistentObjectPool');
  });

  it('should throw an error on duplicated close', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    pool.close();
    try {
      pool.close();
      assert.fail('failed to throw an error on duplicated close')
      }
    catch (err) {
      assert(err.message == 'pool not opened or already closed');
    }
  });

  it('should set root as value', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    // root is initialized to undefined
    assert(pool.root == undefined);
    // number
    var number = 1;
    pool.root = number;
    assert(pool.root == number);
    // string
    var string = 'abc';
    pool.root = string;
    assert(pool.root == string);
    // bool
    var bool = true;
    pool.root = bool;
    assert(pool.root = bool);
    pool.close();
  });

  it('should set root as an Array', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    var arr = ['a', 'b', 'c', 'd'];
    pool.root = arr;
    common.assertEqual(pool.root, arr);
    pool.close();
  });

  it('should set root as an Object', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    var obj = {'a': 'a', 'b': 'b', 'c': 'c', 'd': 'd'};
    pool.root = obj;
    common.assertEqual(pool.root, obj);
  });

  it('should throw an error when set root as unsupported js type', function() {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    var test_func = function() {};
    try {
      pool.root = test_func;
      assert.fail(
          'failed to throw an error when set root as unsupported js type')
      }
    catch (err) {
      assert(err.message == 'unsupported type')
    }
  });

});

describe('transaction', () => {
  beforeEach(async function() {
    await common.resetFolder();
  });

  it('should set root in transaction context', () => {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    pool.transaction(function() {
      pool.root = 10;
    });
    assert(pool.root == 10);
    // reopen pool
    pool.close();
    pool.open();
    assert(pool.root == 10);
  });

  it('should set root in transaction API', () => {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    pool.tx_begin();
    assert(
        pool.tx_stage() == constants.TX_STAGE_WORK,
        'failed to start transaction');
    pool.root = 10;
    pool.tx_commit();
    pool.tx_end();
    assert(
        pool.tx_stage() == constants.TX_STAGE_NONE,
        'failed to end transaction');
    assert(pool.root == 10);
    pool.close();
  });

  it('should unset change to root on transaction abort', () => {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    pool.tx_begin();
    assert(
        pool.tx_stage() == constants.TX_STAGE_WORK,
        'failed to start transaction');
    pool.root = 10;
    pool.tx_abort();
    assert(pool.root == undefined);
  });

});

// TODO: test GC