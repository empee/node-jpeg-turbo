var path = require('path');

var binary = require('node-pre-gyp');

var binding = require(binary.find(
  path.resolve(path.join(__dirname, './package.json'))));

// Copy exports so that we can customize them on the JS side without
// overwriting the binding itself.
Object.keys(binding).forEach(function(key) {
  module.exports[key] = binding[key];
});

module.exports.formatName = function(format) {
  if (format === binding.FORMAT_RGB) {
    return 'FORMAT_RGB';
  }
  if (format === binding.FORMAT_BGR) {
    return 'FORMAT_BGR';
  }
  if (format === binding.FORMAT_RGBX) {
    return 'FORMAT_RGBX';
  }
  if (format === binding.FORMAT_BGRX) {
    return 'FORMAT_BGRX';
  }
  if (format === binding.FORMAT_XRGB) {
    return 'FORMAT_XRGB';
  }
  if (format === binding.FORMAT_XBGR) {
    return 'FORMAT_XBGR';
  }
  if (format === binding.FORMAT_GRAY) {
    return 'FORMAT_GRAY';
  }
  if (format === binding.FORMAT_RGBA) {
    return 'FORMAT_RGBA';
  }
  if (format === binding.FORMAT_BGRA) {
    return 'FORMAT_BGRA';
  }
  if (format === binding.FORMAT_ABGR) {
    return 'FORMAT_ABGR';
  }
  if (format === binding.FORMAT_ARGB) {
    return 'FORMAT_ARGB';
  }
  return false;
};

module.exports.subsamplingName = function(subsampling) {
  if (subsampling === binding.SAMP_444) {
    return 'SAMP_444';
  }
  if (subsampling === binding.SAMP_422) {
    return 'SAMP_422';
  }
  if (subsampling === binding.SAMP_420) {
    return 'SAMP_420';
  }
  if (subsampling === binding.SAMP_GRAY) {
    return 'SAMP_GRAY';
  }
  if (subsampling === binding.SAMP_440) {
    return 'SAMP_440';
  }
  return false;
};

