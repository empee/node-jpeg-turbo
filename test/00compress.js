var assert = require("chai").assert;
var jpeg = require('../');
var generator = require('./img-generator.js');
var rawJahne = generator.jahne();
var rawCrop = generator.crop();

var config = require(__dirname + '/config.json');
var formats = config.formats;
var compress = require(__dirname + '/' + config.tests.compress);

describe("Compress", function() {
  it("No parameters", function(done) {
    assert.throws(function() {
      jpeg.compress();
    }, TypeError, /Too few arguments/);
    done();
  });
  it("Invalid callback", function(done) {
    assert.throws(function() {
      jpeg.compress("string");
    }, TypeError, /Too few arguments/);
    done();
  });

  compress.forEach(function(opts) {
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
    if(options && typeof options === 'object' && typeof options.format === 'undefined') {
      name += formats.join(',') + ' ';
    }
    name += opts.name + ' ';
    name += (shouldFail?'should fail with':'should return') + ' {' + outputName.join(', ') + '}';

    it(name, function(done) {
      var tests = [];
      if(options && typeof options === 'object' && typeof options.format === 'undefined') {
        formats.forEach(function(format) {
          var test = {};
          test.result = result;
          test.shouldFail = shouldFail;

          test.options = JSON.parse(JSON.stringify(options));
          test.options.format = jpeg[format];

          if(test.options && test.options.subsampling) {
            test.options.subsampling = jpeg[test.options.subsampling];
          }

          if(opts.params.input) {
            switch(opts.params.input) {
              case 'string': test.input = 'string'; break;
              case 'emptyBuffer': test.input = new Buffer.alloc(1024*4); break;
              case 'raw-jahne': test.input = rawJahne; break;
              case 'raw-crop': test.input = rawCrop; break;
            }
          }

          if(opts.params.output) {
            var length = 0;
            switch(opts.params.output) {
              case 'small': length = 100; break;
              case 'exact':
                  if(format === 'FORMAT_GRAY') {
                    length = 2099200;
                  }
                  else {
                    length = 3147776;
                  }
                  break;
              case 'large': length = 1024 * 1024 * 10; break;
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

        if(test.options && test.options.format) {
          test.options.format = jpeg[test.options.format];
        }

        if(test.options && test.options.subsampling) {
          test.options.subsampling = jpeg[test.options.subsampling];
        }

        if(opts.params.input) {
          switch(opts.params.input) {
            case 'string': test.input = 'string'; break;
            case 'emptyBuffer': test.input = new Buffer.alloc(1024*4); break;
            case 'raw-jahne': test.input = rawJahne; break;
            case 'raw-crop': test.input = rawCrop; break;
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
      assert.equal(err[test.result.key], test.result.value, 'error value was not expected');
      callback();
    }
    else {
      assert.isNull(err, 'error was not expected');
      assert.instanceOf(output.data, Buffer, 'output data should be a buffer');
      if(test.result.subsampling) {
        if(test.result.subsampling == "DEFAULT_SUBSAMPLING" && test.options.format === jpeg.FORMAT_GRAY) {
          assert.equal(output.subsampling, jpeg.SAMP_GRAY);
        } else {
          assert.equal(output.subsampling, jpeg[test.result.subsampling]);
        }
      }
      if(test.output) {
        assert.equal(output.data.length, test.outputLength, 'output buffer length is not the same as before');
        // output.data.length will not match output.length in this case since compress returns "bytes used" as output.length
      }
      else {
        assert.equal(output.data.length, output.length, 'output data length not match output length');
      }
      callback();
    }
  });
  jpeg.compress.apply(this, args);
}

