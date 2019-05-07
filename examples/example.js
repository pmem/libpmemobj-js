
// TODO: require(module)
const jspmdk = require('../src/jspmdk');
const constants = jspmdk.constants;

var args = process.argv.splice(2);
var path = args[0] || '/home/ssg-test/tmp/jspmdk-pool';

var pool = jspmdk.new_pool(path, constants.MIN_POOL_SIZE);

var check = pool.check();
if (check == -1) {
  // not exists
  pool.create();
  }
else if (check == 0) {
  // not consistent
  }
else if (check == 1) {
  // exists and consistent
  pool.open();
  }

// root
var root = pool.root;
pool.root = undefined;

// PersistentObject
var pobj = pool.create_object({a: 1});
var a = pobj.a;
pobj.b = 2;
delete pobj.b;
Object.getOwnPropertyNames(pobj);

// PersistentArray
var parr = pool.create_object([1, 2]);
parr.length = 3;
if (parr.is_array()) {
  parr.push(3);
  parr.pop();
  }

// PersistentArrayBuffer
var pab = pool.create_arraybuffer(new ArrayBuffer(10));
var pab_uint8 = new Uint8Array(pab);
pab_uint8[0] = 1;
pab.persist(0, 1)

// transaction
pool.transaction(function() {
  pobj.a = 2;
  pab.snapshot(0, 1);
  pab_uint8[0] = 2;
});

pool.close();

console.log('done!');