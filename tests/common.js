const execSync = require('child_process').execSync;
const process = require('process');
const assert = require('assert');
const fs = require('fs');

const config = {
  'valid_path': __dirname + '/tmp-fs',
  'invalid_path': __dirname + '/invalid-path'
}

// typical remove command is "rm -rf", I replace it with a safe command
// "safe-rm"
var cm_remove = 'rm -rf'

var resetFolder = function() {
  return new Promise(function(resolve, reject) {
    path = config['valid_path'];
    var cmd_str = cm_remove + ' ' + path + ' && mkdir ' + path;
    if (!fs.existsSync(path)) cmd_str = 'mkdir ' + path;
    execSync(cmd_str);
    resolve();
  });
}

var assertEqual =
    function(obj1, obj2) {
    var keys = Object.getOwnPropertyNames(obj1);
    for (var key in keys) {
      assert(obj1[key] == obj2[key]);
    }
}

    exports.resetFolder = resetFolder;
exports.config = config;
exports.assertEqual = assertEqual;