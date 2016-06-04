#include "exports.h"
using namespace Nan;
using namespace v8;
using namespace node;

static char errStr[NJT_MSG_LENGTH_MAX] = "No error";
#define _throw(m) {snprintf(errStr, NJT_MSG_LENGTH_MAX, "%s", m); retval=-1; goto bailout;}

int decompress(unsigned char* srcData, uint32_t srcLength, uint32_t format, uint32_t* width, uint32_t* height, uint32_t* dstLength, unsigned char** dstData, uint32_t dstBufferLength) {
  int retval = 0;
  int err;
  tjhandle handle = NULL;
  tjscalingfactor *sf = NULL;
  int n = 0;
  int i;
  int bpp;
  int header_width = 0;
  int header_height = 0;
  uint32_t scaled_width = 0;
  uint32_t scaled_height = 0;

  // Figure out bpp from format (needed to calculate output buffer size)
  switch (format) {
    case FORMAT_GRAY:
      bpp = 1;
      break;
    case FORMAT_RGB:
    case FORMAT_BGR:
      bpp = 3;
      break;
    case FORMAT_RGBX:
    case FORMAT_BGRX:
    case FORMAT_XRGB:
    case FORMAT_XBGR:
    case FORMAT_RGBA:
    case FORMAT_BGRA:
    case FORMAT_ABGR:
    case FORMAT_ARGB:
      bpp = 4;
      break;
    default:
      _throw("Invalid output format");
  }

  handle = tjInitDecompress();
  if (handle == NULL) {
    _throw(tjGetErrorStr());
  }

  err = tjDecompressHeader(handle, srcData, srcLength, &header_width, &header_height);

  if (err != 0) {
    _throw(tjGetErrorStr());
  }

  // scaling requested
  if(*width != 0 || *height != 0) {
    sf = tjGetScalingFactors(&n);
    if (*width != 0 && *width < (uint32_t)TJSCALED(header_width, sf[n-1])) {
      _throw("Scaling width too small");
    }
    if (*height != 0 && *height < (uint32_t)TJSCALED(header_height, sf[n-1])) {
      _throw("Scaling height too small");
    }
    for(i=0; i<n; i++) {
      scaled_width = (uint32_t)TJSCALED(header_width, sf[i]);
      scaled_height = (uint32_t)TJSCALED(header_height, sf[i]);
      if ((*width == 0 || scaled_width <= *width) && (*height == 0 || scaled_height <= *height)) {
        break;
      }
    }
    *width = scaled_width;
    *height = scaled_height;
  }
  else {
    *width = header_width;
    *height = header_height;
  }

  *dstLength = *width * *height * bpp;

  if (dstBufferLength > 0) {
    if (dstBufferLength < *dstLength) {
      _throw("Insufficient output buffer");
    }
  }
  else {
    *dstData = (unsigned char*)malloc(*dstLength);
  }

  err = tjDecompress2(handle, srcData, srcLength, *dstData, *width, 0, *height, format, TJFLAG_FASTDCT);

  if(err != 0) {
    _throw(tjGetErrorStr());
  }


  bailout:
  if (handle != NULL) {
    err = 0;
    err = tjDestroy(handle);
    // If we already have an error retval wont be 0 so in that case we don't want to overwrite error message
    // Also cant use _throw here because infinite-loop
    if (err != 0 && retval == 0) {
      snprintf(errStr, NJT_MSG_LENGTH_MAX, "%s", tjGetErrorStr());
    }
  }

  return retval;
}

class DecompressWorker : public AsyncWorker {
  public:
    DecompressWorker(Callback *callback, unsigned char* srcData, uint32_t srcLength, uint32_t format, uint32_t width, uint32_t height, Local<Object> &dstObject, unsigned char* dstData, uint32_t dstBufferLength) :
      AsyncWorker(callback),
      srcData(srcData),
      srcLength(srcLength),
      format(format),
      width(width),
      height(height),
      dstData(dstData),
      dstBufferLength(dstBufferLength),
      dstLength(0) {
        if (dstBufferLength > 0) {
          SaveToPersistent("dstObject", dstObject);
        }
      }

    ~DecompressWorker() {}

    void Execute () {
      int err;

      err = decompress(
          this->srcData,
          this->srcLength,
          this->format,
          &this->width,
          &this->height,
          &this->dstLength,
          &this->dstData,
          this->dstBufferLength);

      if(err != 0) {
        SetErrorMessage(errStr);
      }
    }

    void HandleOKCallback () {
      Local<Object> obj = New<Object>();
      Local<Object> dstObject;

      if (this->dstBufferLength > 0) {
        dstObject = GetFromPersistent("dstObject").As<Object>();
      }
      else {
        dstObject = NewBuffer((char*)this->dstData, this->dstLength).ToLocalChecked();
      }

      obj->Set(New("data").ToLocalChecked(), dstObject);
      obj->Set(New("width").ToLocalChecked(), New(this->width));
      obj->Set(New("height").ToLocalChecked(), New(this->height));
      obj->Set(New("size").ToLocalChecked(), New(this->dstLength));
      obj->Set(New("format").ToLocalChecked(), New(this->format));

      Local<Value> argv[] = {
        Null(),
        obj
      };

      callback->Call(2, argv);
    }

  private:
    unsigned char* srcData;
    uint32_t srcLength;
    uint32_t format;
    uint32_t width;
    uint32_t height;

    unsigned char* dstData;
    uint32_t dstBufferLength;
    uint32_t dstLength;
};

void decompressParse(const Nan::FunctionCallbackInfo<Value>& info, bool async) {
  int retval = 0;
  int cursor = 0;

  // Input
  Callback *callback = NULL;
  Local<Object> srcObject;
  unsigned char* srcData = NULL;
  uint32_t srcLength = 0;
  Local<Object> options;
  Local<Value> formatObject;
  uint32_t format = NJT_DEFAULT_FORMAT;
  Local<Value> widthObject;
  Local<Value> heightObject;

  // Output
  Local<Object> dstObject;
  uint32_t dstBufferLength = 0;
  unsigned char* dstData = NULL;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t dstLength;

  // Try to find callback here, so if we want to throw something we can use callback's err
  if (async) {
    if (info[info.Length() - 1]->IsFunction()) {
      callback = new Callback(info[info.Length() - 1].As<Function>());
    }
    else {
      _throw("Missing callback");
    }
  }

  if ((async && info.Length() < 2) || (!async && info.Length() < 1)) {
    _throw("Too few arguments");
  }

  // Input buffer
  srcObject = info[cursor++].As<Object>();
  if (!Buffer::HasInstance(srcObject)) {
    _throw("Invalid source buffer");
  }

  srcData = (unsigned char*) Buffer::Data(srcObject);
  srcLength = Buffer::Length(srcObject);

  // Options
  options = info[cursor++].As<Object>();

  // Check if options we just got is actually the destination buffer
  // If it is, pull new object from info and set that as options
  if (Buffer::HasInstance(options) && info.Length() > cursor) {
    dstObject = options;
    options = info[cursor++].As<Object>();
    dstBufferLength = Buffer::Length(dstObject);
    dstData = (unsigned char*) Buffer::Data(dstObject);
  }

  // Options are optional
  if (options->IsObject()) {
    // Format of output buffer
    formatObject = options->Get(New("format").ToLocalChecked());
    if (!formatObject->IsUndefined()) {
      if (!formatObject->IsUint32()) {
        _throw("Invalid format");
      }
      format = formatObject->Uint32Value();
    }

    // (scaling) width
    widthObject = options->Get(New("width").ToLocalChecked());
    if (!widthObject->IsUndefined()) {
      if (!widthObject->IsUint32()) {
        _throw("Invalid scaling width");
      }
      width = widthObject->Uint32Value();
    }

    // (scaling) height
    heightObject = options->Get(New("height").ToLocalChecked());
    if (!heightObject->IsUndefined()) {
      if (!heightObject->IsUint32()) {
        _throw("Invalid scaling height");
      }
      height = heightObject->Uint32Value();
    }
  }

  // Do either async or sync decompress
  if (async) {
    AsyncQueueWorker(new DecompressWorker(callback, srcData, srcLength, format, width, height, dstObject, dstData, dstBufferLength));
    return;
  }
  else {
    retval = decompress(
        srcData,
        srcLength,
        format,
        &width,
        &height,
        &dstLength,
        &dstData,
        dstBufferLength);


    if(retval != 0) {
      // decompress will set the errStr
      goto bailout;
    }
    Local<Object> obj = New<Object>();

    if (dstBufferLength == 0) {
      dstObject = NewBuffer((char*)dstData, dstLength).ToLocalChecked();
    }

    obj->Set(New("data").ToLocalChecked(), dstObject);
    obj->Set(New("width").ToLocalChecked(), New(width));
    obj->Set(New("height").ToLocalChecked(), New(height));
    obj->Set(New("size").ToLocalChecked(), New(dstLength));
    obj->Set(New("format").ToLocalChecked(), New(format));

    info.GetReturnValue().Set(obj);
    return;
  }

  // If we have error throw error or call callback with error
  bailout:
  if (retval != 0) {
    if (NULL == callback) {
      ThrowError(TypeError(errStr));
    }
    else {
      Local<Value> argv[] = {
        New(errStr).ToLocalChecked()
      };
      callback->Call(1, argv);
    }
    return;
  }

}

NAN_METHOD(DecompressSync) {
  decompressParse(info, false);
}

NAN_METHOD(Decompress) {
  decompressParse(info, true);
}
