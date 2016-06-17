#include "exports.h"

NAN_METHOD(BufferSize) {
  int retval = 0;
  char errStr[NJT_MSG_LENGTH_MAX] = "No error";

  // Input
  Callback *callback = NULL;
  Local<Object> options;
  Local<Value> subsampling_obj, width_obj, height_obj;
  int subsampling = NJT_DEFAULT_SUBSAMPLING, width = 0, height = 0;
  unsigned long length = 0;

  // Try to find callback here, so if we want to throw something we can use callback's err
  if (info[info.Length() - 1]->IsFunction()) {
    callback = new Callback(info[info.Length() - 1].As<Function>());
  }

  if ((NULL != callback && info.Length() < 2) || (NULL == callback && info.Length() < 1)) {
    _throw("Too few arguments");
  }

  // Options
  options = info[0].As<Object>();
  if (!options->IsObject()) {
    _throw("Options must be an Object");
  }

  // Subsampling
  subsampling_obj = options->Get(New("subsampling").ToLocalChecked());
  if (!subsampling_obj->IsUndefined()) {
    if (!subsampling_obj->IsUint32()) {
      _throw("Invalid subsampling method");
    }
    subsampling = check_subsampling_mode(subsampling_obj->Uint32Value());
    if (subsampling == -1) {
      _throw("Invalid subsampling method");
    }
  }

  // Width
  width_obj = options->Get(New("width").ToLocalChecked());
  if (width_obj->IsUndefined()) {
    _throw("Missing width");
  }
  if (!width_obj->IsUint32()) {
    _throw("Invalid width value");
  }
  width = width_obj->Uint32Value();

  // Height
  height_obj = options->Get(New("height").ToLocalChecked());
  if (height_obj->IsUndefined()) {
    _throw("Missing height");
  }
  if (!height_obj->IsUint32()) {
    _throw("Invalid height value");
  }
  height = height_obj->Uint32Value();

  // Finally, calculate the buffer size
  length = tjBufSize(width, height, subsampling);

  // How to return length
  if (NULL != callback) {
    Local<Value> argv[] = {
      Null(),
      New((uint32_t)length)
    };
    callback->Call(2, argv);
  }
  else {
    info.GetReturnValue().Set(New((uint32_t)length));
  }

  return;

  bailout:
  if (retval != 0) {
    if (NULL == callback) {
      ThrowError(TypeError(errStr));
    }
    else {
      Local<Value> argv[] = {
        Error(errStr)
      };
      callback->Call(1, argv);
    }
    return;
  }
}
