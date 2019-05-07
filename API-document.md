# **API Document for JSPMDK**
[Array]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array
[String]: https://developer.mozilla.org/zh-CN/docs/Web/JavaScript/Reference/Global_Objects/String


## Creating and Accessing a PersistentObjectPool

+ Module **jspmdk**

  + Description

    The Node.js module that can be loaded by Node.js programs.

  + Usage

    ```javascript
      const jspmdk = require('/path/to/module');
    ```

+ jspmdk.**constants**
  + Description

    Return a JavaScript object containing JSPMDK's constants. Including:

    + **MIN_POOL_SIZE** - the minimum pool size
  
    and transaction stage constants which is consistent to [libpmemobj's transaction stage constants](http://pmem.io/2015/06/15/transactions.html)

    + **TX_STAGE_NONE**
    + **TX_STAGE_WORK**
    + **TX_STAGE_ONCOMMIT**
    + **TX_STAGE_ONABORT**
    + **TX_STAGE_FINALLY**
  
  + Usage

    ```javascript
      var constants = jspmdk.constants;
    ```
+ jspmdk.**new_pool**(path, pool_size=MIN_POOL_SIZE, mode=fs.constants.S_IRUSR | fs.constants.S_IWUSR);

  + Description
  
    Return a **PersistentObjectPool** object which is not been backed by actual pool file. *path* specifies the path to the file to be backed by the pool object, and *pool_size* is the file size in bytes, which is set to be *MIN_POOL_SIZE* by default. *mode* specify the permission of the file to be created.

  + Usage

    ```javascript
      var fs = require('fs');
      var pool = jspmdk.new_pool('/path/to/file', constants.MIN_POOL_SIZE, fs.constants.S_IRUSR | fs.constants.S_IWUSR);
    ```

## PersistentObjectPool

+ PersistentObjectPool.prototype.**check**()
  
  + Description
  
    Check wether the pool file in the *path* already exists. Return **-1** if it not exists, return **0** if it exists but is not a valid pool file, and return **1** if it exists and is valid.

  + Usage
  
    ```javascript
      pool.check();
    ```

+ PersistentObjectPool.prototype.**create**()
  + Description

    Create a backing pool file at *path* with *pool_size* for the **PersistentObjectPool** instance. Raise an error if the file already exists or the creation fails.

  + Usage

    ```javascript
      pool.create();
    ```

+ PersistentObjectPool.prototype.**open**()
  + Description

    Open a backing pool file at *path* for the **PersistentObjectPool** instance. Raise an error if the file does not exist or the process fails.

  + Usage

    ```javascript
      pool.open();
    ```

+ PersistentObjectPool.prototype.**root**

  + Description

    The “root” object of the pool. Only objects that are reachable by traversing the object graph starting from the root object will be preserved once the object pool is closed.

    The getter return a either a JavaScript value (like number, string), or an instance of persistent classes (**PersistentObject**, **PersistentArray**, **PersistentArrayBuffer**).

    The setter check if the value is an instance of persistent classes. If it is, set it directly to the root. Otherwise, check if the value can be convert to persistent classes. If it can, do the conversion and store it to PersistentObjectPool, otherwise raise an error.

  + Usage
    ```javascript
      pool.root = 1;
      pool.root = {a:1};
      var pobj = pool.root;
    ```

+ PersistentObjectPool.prototype.**create_object**(js_object)

  + Description

    Create an instance of **PersistentObject** or **PersistentArray** by an instance of JavaScript Object or Array. The JavaScript object will be saved in the pool but it would survive the garbage collection only if users manually reference the it from the root object. 

  + Usage
   
    ```javascript
      var pobj = pool.create_object({a:1});
      var parr = pool.create_object([1, 2, 3]);
      pool.root.pobj = pobj;
      pool.root.parr = parr;
    ```

+ PersistentObjectPool.prototype.**create_arraybuffer**(js_arraybuffer)

  + Description

    Create an instance of **PersistentArrayBuffer** by an instance of JavaScript ArrayBuffer. Data will be copied and saved in the pool but it would survive the garbage collection only if users manually reference the it from the root object. 

  + Usage
   
    ```javascript
      var pab = pool.create_arraybuffer(new ArrayBuffer(10));
      pool.root.pab = pab;
    ```

+ PersistentObjectPool.prototype.**transaction**(fn)

  + Description

    Execute *fn* in transaction. All changes to objects managed by the pool should be committed; if the transaction end abnormally or the program stops running for any reason in the middle, then none of the changes to the persistent objects inside the transaction should be visible. Note that the transaction does not affect changes to normal JS objects; only changes to Persistent objects will be rolled back on abnormal exit.

  + Usage:
    ```javascript
      pool.transaction(function(){
        pobj.a -= 50;
        pobj.b += 50;
      });
    ```

+ PersistentObjectPool.prototype.**tx_XXX**()
  
  + Description
  
    Enable user the manage the life cycle of transaction themselves. The function here is consistent to [libpmemobj's transaction API](http://pmem.io/pmdk/manpages/linux/master/libpmemobj/pmemobj_tx_begin.3), including **tx_begin**(), **tx_commit**(), **tx_end**(), **tx_abort**() and **tx_stage**().

  + Usage
  
    ```javascript
      pool.tx_begin();
      if (pool.tx_stage == constants.TX_STAGE_WORK){
        try{
          // do something
          pool.tx_commit();
        }
        catch(e){
          pool.tx_abort();
        }
      }
      pool.tx_end();
    ```

+ PersistentObjectPool.prototype.**gc**()

  + Description

    Free all unreferenced objects - objects that are not accessible by tracing the objects graph starting at the root object. This function is also called automatically when the pool is opened or closed.

  + Usage
    ```javascript
      pool.gc();
    ```


+ PersistentObjectPool.prototype.**close**()

  + Description

    Call **gc**() and close the pool. The object pool can be reopened and all the objects reachable from the root object will be in the same state they were in when the pool was closed.

  + Usage:
    ```javascript
      pool.close();
    ```

## PersistentObject

+ **PersistentObject** is designed to be used as if primitive JavaScript object. We can create a new PersistentObject instance by **create_object**() from a pool or just setting a JavaScript object to existing persistent structure. Any object that is got from the properties of a persistent object will also be a persistent object. 

+ PersistentObject.prototype.[[setter]] (key, value);

  + Description

    The setter allow users to atomically set an key-value pair to the property of a PersistentObject instance. The key must be a number or a string, otherwise an error would be raised. The value can be either a primitive JavaScript object (number, bool, string, undefined, null, ArrayBuffer, Array, Object) or a persistent object. If it is a JavaScript object, it will be firstly persisted in the pool.
  
  + Usage

    ```javascript
      var pobj = pool.create_object({a:1});
      pobj.b = 2;
      pobj.c = {c:1};
      pobj.d = pobj.c;
      pool.root.pobj = pobj;
    ```
+ PersistentObject.prototype.[[getter]] (key);

  + Description

    The getter allow users to atomically get a property of a persistent object specified by *key*. The key must be a number or a string, otherwise an error would be raised. If the value of the property is non-container type (number, bool, string, undefined, null, ArrayBuffer), the return value would be a primitive JS object.  Otherwise if the value of the property is container-type (Array, Object), the return value would be a persistent object. 
  
  + Usage

    ```javascript
      var pobj = pool.create_object({a:1, b:{b:2}});
      pobj.a; // 1
      pobj.b; // PersistentObject
    ```
+ PersistentObject.prototype.[[deleter]] (key);

  + Description

    The deleter allow users to delete a property of a persistent object specified by *key*. If the key exists in the persistent object, it would be deleted, otherwise nothing happens.
  
  + Usage

    ```javascript
      var pobj = pool.create_object({a:1, b:{b:2}});
      delete pobj.a;
    ```
+ PersistentObject.prototype.**is_array**()

  + Description

    Tell wether a persistent object is a persistent array. Return true if it is, otherwise return false.
  
  + Usage

    ```javascript
      var pobj = pool.create_object({a:1, b:2});
      var parr = pool.create_object([1,2,3]);
      pobj.is_array(); // false
      parr.is_array(); // true
    ```

+ Object.getPropertyNames(PersistentObject)

  + Description
    PersistentObject reuse the reflection in JavaScript Object. Users can get all the properties' names by this method. The return value is a JavaScript Array. However, due to our implementation, the Array is unordered.
  
  + Usage

    ```javascript
      var pobj = pool.create_object({a:1, b:2, c:3});
      Object.getPropertyNames(pobj); // ["b", "a", "c"], could be in arbitrary order
    ```

## PersistentArray

  + **PersistentArray** is designed to be used as if primitive JavaScript array. It inherits all the member functions from PersistentObject while holding additional Array's features.

  + PersistentArray.prototype.[[get_length]], 
    PersistentArray.prototype.[[set_length]] (length);

    + Description: 
      
      the setter and getter of the property "length" of PersistentArray. The length must be an positive integer that less than Uint32_max, otherwise an error would be raised. Setting the length of a PersistentArray instance would expand or shrink the original array. 

    + Usage:

      ```javascript
        var parr = pool.create_object([1,2,3]);
        parr.length; // 3
        parr.length = 2; // now the array would be [1, 2]
      ```
  + PersistentArray.prototype.**push**(item)

    + Description:
  
      Add a item to the last position of persistent array, and the item can be persistent objects or primitive JS objects. The semantics is same as Array's *push*.
    
    + Usage:

      ```javascript
        var parr = pool.create_object([1,2,3]);
        parr.push(4)
        parr.push({a:1})
      ```
  + PersistentArray.prototype.**pop**()

    + Description:
  
      Return the last item in the persistent array and delete it. The return value can be be persistent objects (Array, Object) or primitive JS objects(number, string, undefined, bool, null, ArrayBuffer). The semantics is same as Array's *pop*.
    
    + Usage:

      ```javascript
        var parr = pool.create_object([1,2,3,{a:1}]);
        parr.pop(); // PersistentObject
        parr.pop(); // 3
      ```


## PersistentArrayBuffer
  + **PersistentArrayBuffer** is designed to be used as if primitive JavaScript arraybuffer. We can create a new PersistentArrayBuffer instance by **create_arraybuffer**() from a pool or just setting a JavaScript arraybuffer to existing persistent structure. However, users should flush the cache to ensure the data is persisted to the persistent arraybuffer. In addition, changes to persistent arraybuffer could be inconsistent when the process abort abnormally, so users can use transaction and snapshot to guarantee data consistency.
 
  + PersistentArrayBuffer.prototype.**flush**(offset, length)

    + Description

      For PersistentArrayBuffer objects, users should flush the cache to ensure the data is persisted.

    + Usage:
      ```javascript
        var pab = pool.create_arraybuffer(new ArrayBuffer(64));
        pab_uint8 = new Uint8Array(pab);
        pab_uint8[0] = 1;
        pab.flush(0, 1);
      ```

  + PersistentArrayBuffer.prototype.**snapshot**(offset, length)

    + Description

      To safely write PersistentArrayBuffer objects in transaction, users should manually snapshot the region to be written, so that in case of failure, the snapshot region can be rolled back. Besides, there is no need to flush the region that been taken a snapshot. For PersistentObject object, no snapshot is required by users since we automatically take one. Note that the snapshot must be in a transaction, otherwise an error would be raised.

    + Usage:
      ```javascript
        pool.transaction(function(){
          pab.snapshot(0,2);
          var pab_uint8 = new Uint8Array(pab);
          // would be rolled back upon failure since it is taken a snapshot before
          pab_uint8[0] = 1;
          pab_uint8[1] = 2;
        });
      ```