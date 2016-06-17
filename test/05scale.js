var assert = require("chai").assert;
var jpeg = require('../');

var generator = require('./img-generator.js');
var files = {};
files.jahne = {data: generator.jahne(), width: 1024, height: 1024};

var config = require(__dirname + '/config.json');
var formats = config.formats;
var scalers = config.scalers;
var tests = require(__dirname + '/' + config.tests.scale);

describe("Scale", function() {
  scalers.splice(scalers.indexOf('SCALE_FAST'), 1);
  scalers.forEach(function(mode) {
    describe(mode, function() {
      this.timeout(10000);
      this.slow(3000);
      Object.keys(tests).forEach(function(filename) {
        tests[filename].forEach(function(opts) {
          var inputName = [];
          var outputName = [];
          var options = {};
          var result = {};
          var shouldFail = false;

          Object.keys(opts.params).forEach(function(key) {
            options[key] = JSON.parse(JSON.stringify(opts.params[key]));
            inputName.push(key + ': ' + (typeof opts.params[key] === 'object'?JSON.stringify(opts.params[key]):opts.params[key]) );
          });

          options.scale.mode = jpeg[mode];

          if(opts.success.indexOf(mode) !== -1) {
            result = opts.successReturn[mode]?opts.successReturn[mode]:opts.successReturn['*'];
            shouldFail = false;
            Object.keys(result).forEach(function(key) {
              outputName.push(key + ': ' + result[key]);
            });
          }
          else if(opts.fail.indexOf(mode) !== -1) {
            result = opts.failReason[mode]?opts.failReason[mode]:opts.failReason['*'];
            shouldFail = true;
            outputName.push('Type: ' + result.type);
            outputName.push(result.key + ': ' + result.value);
          }

          var name = formats.join(',') + ' {' + inputName.join(', ') + '} ' +
            (shouldFail?'should fail with':'should return') + ' {' + outputName.join(', ') + '}';

          it(name, function(done) {
            var tests = [];
            // Altough our input is rgba, we can still scale it as we want, output just would look a bit funky..
            formats.forEach(function(format) {
              options.format = jpeg[format];
              var optionsCopy = JSON.parse(JSON.stringify(options));
              tests.push({options: optionsCopy, result: result, shouldFail: shouldFail, file: filename});
            });
            runTests(tests, done);
          });
        });
      });
    });
  });
});

function runTests(tests, done) {
  var test = tests.shift();
  if (!test) {
    done();
    return;
  }
  runTest(test, function() {
    runTests(tests,done);
  });
}

function runTest(test, callback) {
  var file = files[test.file];
  test.options.width = file.width;
  test.options.height = file.height;
  jpeg.scale(file.data, test.options, function(err, data) {
    if(test.shouldFail) {
      assert.isNotNull(err);
      assert.typeOf(err, test.result.type);
      assert.include(err[test.result.key], test.result.value);
      callback();
    } else {
      assert.isNull(err);
      assert.equal(data.width, test.result.width);
      assert.equal(data.height, test.result.height);
      assert.equal(data.bpp, jpeg.getBpp(test.options.format));
      assert.equal(data.length, data.width*data.height*data.bpp);
      assert.equal(data.format, test.options.format);
      assert.instanceOf(data.data, Buffer);
      assert.equal(data.data.length, data.length);
      callback();
    }
  });
}
