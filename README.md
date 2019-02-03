# **libpmemobj-js: Persistent Memory Development Kit for JavaScript**

The **Persistent Memory Development Kit for JavaScript\* (libpmemobj-js)** is a project to provide a Node.js module to store JavaScript objects in persistent memory. One of the goal of the project is to make programming with persistent JavaScript objects feels natural to developer. We have implemented persistent JavaScript classes including PersistentObject, PersistentArray and PersistentArrayBuffer, however they are not fully performance-optimized. Please see our [examples](#Example) and [API document](https://github.com/pmem/libpmemobj-js/blob/master/API-document.md) for details.

This module uses the libpmemobj library from the Persistent Memory Development Kit (PMDK). For more information on PMDK, please visit http://pmem.io and https://github.com/pmem/pmdk.

## Dependencies

+ Node.js 8.x or higher
+ [PMDK](https://github.com/pmem/pmdk) - native persistent memory libraries
+ [node-addon-api](https://github.com/nodejs/node-addon-api) - header-only C++ wrapper classes which simplify the use of the C based [N-API](https://nodejs.org/dist/latest/docs/api/n-api.html) provided by Node.js 
+ [bindings](https://github.com/TooTallNate/node-bindings) - Helper module for loading native module's .node file
+ Use only for testing
  + [mocha](https://github.com/mochajs/mocha) - test framework

## BUILD & TEST

### Get the Codes

```
$ git clone https://github.com/pmem/libpmemobj-js.git
$ cd libpmemobj-js
```

### Build libpmemobj-js

You can build the dependency to PMDK by using our script, then install libpmemobj-js by npm

```
$ cd deps
$ ./buildall.sh
$ cd ../src
$ npm install
```
### TEST

After build, you can run the tests by

```
$ mocha ../tests
```

## Example

We are using memory to [emulate a persistent memory](http://pmem.io/2016/02/22/pm-emulation.html).

```javascript

const jspmdk = require('jspmdk');
const constants = jspmdk.constants;

// you should specify your own path to persistent memory here
var path = '/path/to/pmem/file';
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

var root = pool.root;
pool.root = undefined;

// persistent Object
var pobj = pool.create_object({a: 1});
var a = pobj.a;
pobj.b = 2;
delete pobj.b;

// persistent Array
var parr = pool.create_object([1, 2]);
if (parr.is_array()) {
  parr.push(3);
  parr.pop();
}

// persistent ArrayBuffer
var pab = pool.create_arraybuffer(new ArrayBuffer(10));
var pab_uint8 = new Uint8Array(pab);
pab_uint8[0] = 1;
pab.persist(0, 1)

// close object pool
pool.close();
```


<!-- ## Install clang-format hook
cd scripts
./git-pre-commit-format install -->
