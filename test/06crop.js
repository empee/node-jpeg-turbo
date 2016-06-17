var assert = require("chai").assert;
var jpeg = require('../');

var generator = require('./img-generator.js');
var rawCrop = generator.crop();

var config = require(__dirname + '/config.json');
var tests = require(__dirname + '/' + config.tests.crop);

describe("Crop", function() {
  this.timeout(10000);
  this.slow(500);
  tests.forEach(function(opts) {
    var inputName = [];
    var outputName = [];
    var options = {crop: {}};
    var result = {};
    var shouldFail = false;

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
      result = opts.successReturn.precise?opts.successReturn.precise:opts.successReturn['*'];
      shouldFail = false;
      Object.keys(result).forEach(function(key) {
        outputName.push(key + ': ' + JSON.stringify(result[key]));
      });
    }

    var name = ' {crop: {' + inputName.join(', ') + '}} ' +
      (shouldFail?'should fail with':'should return') + ' {' + outputName.join(', ') + '}';

    it(name, function(done) {
      var optionsCopy = JSON.parse(JSON.stringify(options));
      runTest({options: optionsCopy, result: result, shouldFail: shouldFail}, done);
    });
  });
});

function runTest(test, callback) {
  var file = rawCrop;
  test.options.width = 1024;
  test.options.height = 1024;
  jpeg.crop(file, test.options, function(err, output) {
    if(test.shouldFail) {
      console.log('FAIL');
      assert.isNotNull(err, 'error was expected');
      assert.typeOf(err, test.result.type, 'error type was not expected');
      assert.include(err[test.result.key], test.result.value, 'error value was not expected');
      callback();
    } else {
      assert.isNull(err, 'error was not expected');
      assert.equal(output.width, test.result.width, 'output width was not expected');
      assert.equal(output.height, test.result.height, 'output height was not expected');
      assert.equal(output.length, output.width*output.height*output.bpp, 'output data length does not match');
      assert.instanceOf(output.data, Buffer, 'output data should be a buffer');
      assert.equal(output.data.length, output.length, 'output data does not match output length');
      if(test.result.coordinates) {
        var coordinates = generator.findCoordinates(output.data, output.width, output.height, output.bpp);
        assert.deepEqual(coordinates, test.result.coordinates, 'output blocks/edges are not what was expected, misaligned crop');
      }
      callback();
    }
  });
}
