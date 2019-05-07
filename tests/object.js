const assert = require('assert');
const common = require('./common');
const jspmdk = require('../src/jspmdk');
const constants = jspmdk.constants;

var valid_path = common.config.valid_path + '/file0';
var invalid_path = common.config.invalid_path + '/file0';

describe('persistent object', () => {
  beforeEach(async function() {
    await common.resetFolder();
  });

  it('should create a persistent object through pool.create_object()',
     function() {
       var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
       pool.create();
       var obj = {a: 1, b: 2, c: 3};
       var pobj = pool.create_object(obj);
       assert(pobj.constructor.name == 'PersistentObject');
       common.assertEqual(obj, pobj);
     });

  it('should set new property as (key, value) and (index, value)', () => {
    var pool = jspmdk.new_pool(valid_path, constants.MIN_POOL_SIZE);
    pool.create();
    var pobj = pool.create_object({a: 1, b: 2, c: 3});
    pobj.d = 4;
    pobj[0] = 'abc';
    assert(pobj.d == 4);
    assert(pobj[0] == 'abc');
  });



});
