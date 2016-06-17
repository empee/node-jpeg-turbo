var assert = require("chai").assert;
var jpeg = require('../');

var generator = require('./img-generator.js');
var rawCrop = generator.crop();
var files = {};
files.crop = jpeg.compress(rawCrop, {width: 1024, height: 1024, subsampling: jpeg.SAMP_444}).data;

var config = require(__dirname + '/config.json');
var formats = config.formats;
var tests = require(__dirname + '/' + config.tests.crop);

describe("Decompress - crop", function() {
  this.timeout(10000);
  this.slow(500);
  tests.forEach(function(opts) {
    [true, false].forEach(function(precise) {
      var inputName = [];
      var outputName = [];
      var options = {crop: {}};
      var result = {};
      var shouldFail = false;

      inputName.push('precise: ' + precise);

      Object.keys(opts.params).forEach(function(key) {
        options.crop[key] = opts.params[key];
        inputName.push(key + ': ' + opts.params[key]);
      });

      if(opts.failReason !== null) {
        result = opts.failReason;
        shouldFail = true;
        outputName.push('Type: ' + result.type);
        outputName.push(result.key + ': ' + result.value);
      }
      else {
        options.crop.precise = precise;
        result = opts.successReturn[precise?'precise':'*']?opts.successReturn[precise?'precise':'*']:opts.successReturn['*'];
        shouldFail = false;
        Object.keys(result).forEach(function(key) {
          outputName.push(key + ': ' + JSON.stringify(result[key]));
        });
      }

      var name = formats.join(',') + ' {crop: {' + inputName.join(', ') + '}} ' +
        (shouldFail?'should fail with':'should return') + ' {' + outputName.join(', ') + '}';
      it(name, function(done) {
        var tests = [];
        formats.forEach(function(format) {
          options.format = jpeg[format];
          var optionsCopy = JSON.parse(JSON.stringify(options));
          tests.push({options: optionsCopy, result: result, shouldFail: shouldFail});
        });
        runTests(tests, done);
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
  jpeg.decompress(files.crop, test.options, function(err, output) {
    if(test.shouldFail) {
      assert.isNotNull(err, 'error was expected');
      assert.typeOf(err, test.result.type, 'error type was not expected');
      assert.include(err[test.result.key], test.result.value, 'error value was not expected');
      callback();
    } else {
      assert.isNull(err, 'error was not expected');
      assert.equal(output.width, test.result.width, 'output width was not expected');
      assert.equal(output.height, test.result.height, 'output height was not expected');
      assert.equal(output.bpp, jpeg.getBpp(test.options.format), 'output bpp should be what was requested');
      assert.equal(output.length, output.width*output.height*output.bpp, 'output data length does not match');
      assert.equal(output.format, test.options.format, 'output format should be what was requested');
      assert.instanceOf(output.data, Buffer, 'output data should be a buffer');
      assert.equal(output.data.length, output.length, 'output data does not match output length');
      if(test.options.format === jpeg.FORMAT_RGBA && test.result.coordinates) {
        var coordinates = generator.findCoordinates(output.data, output.width, output.height, output.bpp);
        assert.deepEqual(coordinates, test.result.coordinates, 'output blocks/edges are not what was expected, misaligned crop');
      }
      callback();
    }
  });
}

