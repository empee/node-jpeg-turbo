#include "exports.h"

NAN_MODULE_INIT(InitAll) {
  Set(target, New("bufferSize").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(BufferSize)).ToLocalChecked());
  Set(target, New("compress").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(Compress)).ToLocalChecked());
  Set(target, New("decompress").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(Decompress)).ToLocalChecked());
  Set(target, New("getBpp").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(GetBpp)).ToLocalChecked());
  Set(target, New("crop").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(Crop)).ToLocalChecked());
  Set(target, New("scale").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(Scale)).ToLocalChecked());
  Set(target, New("header").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(Header)).ToLocalChecked());
  Set(target, New("FORMAT_RGB").ToLocalChecked(), New(FORMAT_RGB));
  Set(target, New("FORMAT_BGR").ToLocalChecked(), New(FORMAT_BGR));
  Set(target, New("FORMAT_RGBX").ToLocalChecked(), New(FORMAT_RGBX));
  Set(target, New("FORMAT_BGRX").ToLocalChecked(), New(FORMAT_BGRX));
  Set(target, New("FORMAT_XRGB").ToLocalChecked(), New(FORMAT_XRGB));
  Set(target, New("FORMAT_XBGR").ToLocalChecked(), New(FORMAT_XBGR));
  Set(target, New("FORMAT_GRAY").ToLocalChecked(), New(FORMAT_GRAY));
  Set(target, New("FORMAT_RGBA").ToLocalChecked(), New(FORMAT_RGBA));
  Set(target, New("FORMAT_BGRA").ToLocalChecked(), New(FORMAT_BGRA));
  Set(target, New("FORMAT_ABGR").ToLocalChecked(), New(FORMAT_ABGR));
  Set(target, New("FORMAT_ARGB").ToLocalChecked(), New(FORMAT_ARGB));
  Set(target, New("SAMP_444").ToLocalChecked(), New(SAMP_444));
  Set(target, New("SAMP_422").ToLocalChecked(), New(SAMP_422));
  Set(target, New("SAMP_420").ToLocalChecked(), New(SAMP_420));
  Set(target, New("SAMP_GRAY").ToLocalChecked(), New(SAMP_GRAY));
  Set(target, New("SAMP_440").ToLocalChecked(), New(SAMP_440));
  Set(target, New("SCALE_FAST").ToLocalChecked(), New(SCALE_FAST));
  Set(target, New("SCALE_NEAREST").ToLocalChecked(), New(SCALE_NEAREST));
  Set(target, New("SCALE_BILINEAR").ToLocalChecked(), New(SCALE_BILINEAR));
  Set(target, New("SCALE_BICUBIC").ToLocalChecked(), New(SCALE_BICUBIC));
  Set(target, New("DEFAULT_FORMAT").ToLocalChecked(), New(NJT_DEFAULT_FORMAT));
  Set(target, New("DEFAULT_SUBSAMPLING").ToLocalChecked(), New(NJT_DEFAULT_SUBSAMPLING));
  Set(target, New("DEFAULT_QUALITY").ToLocalChecked(), New(NJT_DEFAULT_QUALITY));
  Set(target, New("DEFAULT_SCALE").ToLocalChecked(), New(NJT_DEFAULT_SCALE));
}

// There is no semi-colon after NODE_MODULE as it's not a function (see node.h).
// see http://nodejs.org/api/addons.html
NODE_MODULE(jpegturbo, InitAll)
