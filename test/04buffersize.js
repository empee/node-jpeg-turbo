var assert = require("chai").assert;
var jpeg = require('../');

describe("Buffersize", function() {
  it("No parameters", function(done) {
    assert.throws(function() {
      jpeg.bufferSize();
    }, TypeError, /Too few arguments/);
    done();
  });
  it("Invalid options", function(done) {
    assert.throws(function() {
      jpeg.bufferSize("string");
    }, TypeError, /Options must be an Object/);
    done();
  });
  it("Missing width", function(done) {
    assert.throws(function() {
      jpeg.bufferSize({height: 100});
    }, TypeError, /Missing width/);
    done();
  });
  it("Missing height", function(done) {
    jpeg.bufferSize({width: 100}, function(err) {
      assert.isNotNull(err);
      assert.typeOf(err, 'Error');
      assert.include(err.message, 'Missing height');
      done();
    });
  });
  it("Missing subsampling", function(done) {
    jpeg.bufferSize({width: 100, height: 100}, function(err, data) {
      assert.isNull(err);
      assert.isAtLeast(data, 100*100);
      done();
    });
  });
  it("Subsampling", function(done) {
    jpeg.bufferSize({width: 100, height: 100, subsampling: jpeg.SAMP_444}, function(err, data) {
      assert.isNull(err);
      assert.isAtLeast(data, 100*100);
      done();
    });
  });
});

