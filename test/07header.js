var assert = require("chai").assert;
var jpeg = require('../');

var generator = require('./img-generator.js');
var rawJahne = generator.jahne();

var files = {
  'DEFAULT_SUBSAMPLING': jpeg.compress(rawJahne, {width: 1024}).data,
  'SAMP_444': jpeg.compress(rawJahne, {width: 1024, subsampling: jpeg.SAMP_444}).data,
  'SAMP_422': jpeg.compress(rawJahne, {width: 1024, subsampling: jpeg.SAMP_422}).data,
  'SAMP_420': jpeg.compress(rawJahne, {width: 1024, subsampling: jpeg.SAMP_420}).data,
  'SAMP_GRAY': jpeg.compress(rawJahne, {width: 1024, subsampling: jpeg.SAMP_GRAY}).data,
  'SAMP_440': jpeg.compress(rawJahne, {width: 1024, subsampling: jpeg.SAMP_440}).data
};

describe("Decompress - init", function() {
  it("No parameters", function(done) {
    assert.throws(function() {
      jpeg.header();
    }, TypeError, /Too few arguments/);
    done();
  });
  it("Invalid source buffer", function(done) {
    assert.throws(function() {
      jpeg.header("string");
    }, TypeError, /Invalid source buffer/);
    done();
  });
  it("Empty input buffer", function(done) {
    jpeg.header(Buffer.alloc(100), function(err) {
      assert.isNotNull(err, 'error was expected');
      assert.typeOf(err, 'Error', 'error type was not expected');
      assert.include(err.message, 'Not a JPEG file: starts with 0x00 0x00', 'error value was not expected');
      done();
    });
  });
  Object.keys(files).forEach(function(subsampling) {
    it('Subsampling ' + subsampling, function(done) {
      jpeg.header(files[subsampling], function(err,output) {
        assert.isNull(err, 'error was not expected');
        assert.equal(output.width, 1024, 'output width was not expected');
        assert.equal(output.height, 1024, 'output height was not expected');
        assert.equal(output.subsampling, jpeg[subsampling]);
        done();
      });
    });
  });
});
