var assert = require("chai").assert;
var jpeg = require('../');

var generator = require('./img-generator.js');
var rawJahne = generator.jahne();
var compressedJahne = jpeg.compress(rawJahne, {width: 1024, height: 1024, format: jpeg.FORMAT_RGBA}).data;

var config = require(__dirname + '/config.json');
var formats = config.formats;
var init = require(__dirname + '/' + config.tests.decompress.init);

describe("Decompress - init", function() {
  it("No parameters", function(done) {
    assert.throws(function() {
      jpeg.decompress();
    }, TypeError, /Too few arguments/);
    done();
  });
  it("Invalid source buffer", function(done) {
    assert.throws(function() {
      jpeg.decompress("string");
    }, TypeError, /Invalid source buffer/);
    done();
  });

  init.forEach(function(opts) {
    var outputName = [];
    var options;
    var result = {};
    var shouldFail = false;

    if(opts.params.options) {
      options = opts.params.options;
    }

    if(opts.failReason !== null) {
      result = opts.failReason;
      shouldFail = true;
      outputName.push('Type: ' + result.type);
      outputName.push(result.key + ': ' + result.value);
    }
    else {
      result = opts.successReturn;
      shouldFail = false;
      Object.keys(result).forEach(function(key) {
        outputName.push(key + ': ' + result[key]);
      });
    }

    var name = '';
    if(options && typeof options === 'object') {
      name += formats.join(',') + ' ';
    }
    name += opts.name + ' ';
    name += (shouldFail?'should fail with':'should return') + ' {' + outputName.join(', ') + '}';

    it(name, function(done) {
      var tests = [];
      if(options && typeof options === 'object') {
        formats.forEach(function(format) {
          var test = {};
          test.result = result;
          test.shouldFail = shouldFail;

          options.format = jpeg[format];
          test.options = JSON.parse(JSON.stringify(options));

          if(opts.params.input) {
            switch(opts.params.input) {
              case 'string': test.input = 'string'; break;
              case 'emptyBuffer': test.input = new Buffer.alloc(100); break;
              case 'compressed-jahne': test.input = compressedJahne; break;
            }
          }

          if(opts.params.output) {
            var length = 0;
            switch(opts.params.output) {
              case 'small': length = 100; break;
              case 'exact': length = result.width * result.height * jpeg.getBpp(jpeg.DEFAULT_FORMAT); break;
              case 'large': length = result.width * result.height * 5; break;
              default:
                  test.output = opts.params.output;
            }
            if(length > 0) {
              test.output = Buffer.alloc(length);
              test.outputLength = length;
            }
          }

          tests.push(test);
        });
      }
      else {
        var test = {};
        test.result = result;
        test.shouldFail = shouldFail;

        if(options) {
          test.options = JSON.parse(JSON.stringify(options));
        }

        if(opts.params.input) {
          switch(opts.params.input) {
            case 'string': test.input = 'string'; break;
            case 'emptyBuffer': test.input = new Buffer.alloc(100); break;
            case 'compressed-jahne': test.input = compressedJahne; break;
          }
        }

        if(opts.params.output) {
          var length = 0;
          switch(opts.params.output) {
            case 'small': length = 100; break;
            case 'exact': length = result.width * result.height * jpeg.getBpp(jpeg.DEFAULT_FORMAT); break;
            case 'large': length = result.width * result.height * 5; break;
            default:
                test.output = opts.params.output;
          }
          if(length > 0) {
            test.output = Buffer.alloc(length);
            test.outputLength = length;
          }
        }

        tests.push(test);
      }

      runTests(tests, done);
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
  var args = [];
  if(test.input) {
    args.push(test.input);
  }
  if(test.output) {
    args.push(test.output);
  }
  if(test.options) {
    args.push(test.options);
  }
  args.push(function(err, output) {
    if(test.shouldFail) {
      assert.isNotNull(err, 'error was expected');
      assert.typeOf(err, test.result.type, 'error type was not expected');
      assert.include(err[test.result.key], test.result.value, 'error value was not expected');
      callback();
    }
    else {
      var format = test.options?(typeof test.options.format !== 'undefined'?test.options.format:jpeg.DEFAULT_FORMAT):jpeg.DEFAULT_FORMAT;
      if(test.result.format) {
        format = jpeg[test.result.format];
      }
      assert.isNull(err, 'error was not expected');
      assert.equal(output.width, test.result.width, 'output width was not expected');
      assert.equal(output.height, test.result.height, 'output height was not expected');
      assert.equal(output.bpp, jpeg.getBpp(format), 'output bpp should be what was requested');
      assert.equal(output.format, format, 'output format should be what was requested');
      assert.instanceOf(output.data, Buffer, 'output data should be a buffer');
      if(test.output) {
        assert.equal(output.data.length, test.outputLength, 'output data length not match output length');
        // output.data.length will not match (test.)output.length in this case since compress returns "bytes used" as output.length
      }
      else {
        assert.equal(output.length, output.width*output.height*output.bpp, 'output data length does not match');
        assert.equal(output.data.length, output.length, 'output data length not match output length');
      }
      callback();
    }
  });
  jpeg.decompress.apply(this, args);
}
