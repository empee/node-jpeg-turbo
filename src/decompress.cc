#include "exports.h"
using namespace Nan;
using namespace v8;
using namespace node;

static char errStr[NJT_MSG_LENGTH_MAX] = "No error";
#define _throw(m) {snprintf(errStr, NJT_MSG_LENGTH_MAX, "%s", m); retval=-1; goto bailout;}

int decompress(unsigned char* srcData, uint32_t srcLength, uint32_t format, uint32_t* width, uint32_t* height, njt_crop* crop, uint32_t* dstLength, unsigned char** dstData, uint32_t dstBufferLength) {
  int retval = 0;
  int err;
  tjhandle handle = NULL;
  int bpp;

  // resize variables
  tjscalingfactor *sf = NULL;
  int n = 0;
  int i;
  int header_width = 0;
  int header_height = 0;
  uint32_t scaled_width = 0;
  uint32_t scaled_height = 0;

  // crop variables
  bool have_crop = false;
  unsigned char *crop_dstBufs[1];
  unsigned char *crop_buffer;
  unsigned char *src_buffer;
  unsigned long crop_dstSizes[1];
  tjtransform transforms[1];
  tjregion rect;
  int jpegSubsamp = 0;
  int rowLength = 0;
  int cropRowLength = 0;
  int offset = 0;

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

  // init crop if we have it
  if (crop->x > 0 || crop->y > 0 || crop->width > 0 || crop->height > 0) {
    handle = tjInitTransform();
    have_crop = true;
    crop->mcu_x = 0;
    crop->mcu_y = 0;
    crop->mcu_w = 0;
    crop->mcu_h = 0;
  }

  err = tjDecompressHeader2(handle, srcData, srcLength, &header_width, &header_height, &jpegSubsamp);

  if (err != 0) {
    _throw(tjGetErrorStr());
  }

  // do cropping
  if (have_crop) {
    if (crop->x > 0) {
      crop->mcu_x = crop->x - crop->x % tjMCUWidth[jpegSubsamp];
    }

    if (crop->width > 0) {
      crop->mcu_w = crop->width + (crop->width + crop->x % tjMCUWidth[jpegSubsamp]) % tjMCUWidth[jpegSubsamp];
    }
    else {
      crop->mcu_w = header_width - crop->mcu_x;
      crop->width = crop->mcu_w - (crop->x - crop->mcu_x);
    }

    if (crop->y > 0) {
      crop->mcu_y = crop->y - crop->y % tjMCUHeight[jpegSubsamp];
    }

    if (crop->height > 0) {
      crop->mcu_h = crop->height + (crop->height + crop->y % tjMCUHeight[jpegSubsamp]) % tjMCUHeight[jpegSubsamp];
    }
    else {
      crop->mcu_h = header_height - crop->mcu_y;
      crop->height = crop->mcu_h - (crop->y - crop->mcu_y);
    }

    rect.x = (int)crop->mcu_x;
    rect.y = (int)crop->mcu_y;
    rect.w = (int)crop->mcu_w;
    rect.h = (int)crop->mcu_h;
    transforms[0].r = rect;
    transforms[0].op = TJXOP_NONE;
    transforms[0].options = TJXOPT_CROP;
    transforms[0].data = NULL;
    transforms[0].customFilter = NULL;
    crop_dstBufs[0] = NULL;
    crop_dstSizes[0] = 0;

    err = tjTransform(handle, srcData, srcLength, 1, crop_dstBufs, crop_dstSizes, transforms, 0);

    if (err != 0) {
      _throw(tjGetErrorStr());
    }

    memcpy(srcData, crop_dstBufs[0], crop_dstSizes[0]);

    tjFree(crop_dstBufs[0]);
    srcLength = crop_dstSizes[0];
    header_width = crop->mcu_w;
    header_height = crop->mcu_h;

    err = tjDestroy(handle);
    if (err != 0) {
      _throw(tjGetErrorStr());
    }
  }

  // decompress
  handle = tjInitDecompress();

  if (handle == NULL) {
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

  // Precise cropping
  if (have_crop && crop->precise) {
    rowLength = *width * bpp;
    cropRowLength = crop->width * bpp;
    offset = (crop->x - crop->mcu_x) * bpp;

    *width = crop->width;
    *height = crop->height;
    *dstLength = *width * *height * bpp;
    crop_dstBufs[0] = (unsigned char*)malloc(*dstLength);
    crop_buffer = crop_dstBufs[0];

    src_buffer = *dstData + (crop->y - crop->mcu_y) * rowLength;
    for (i = 0; i < (int)crop->height; i++) {
      memcpy(crop_buffer, src_buffer + offset, cropRowLength);
      src_buffer += rowLength;
      crop_buffer += cropRowLength;
    }
    if (dstBufferLength <= 0) {
      free(*dstData);
      *dstData = (unsigned char*)malloc(*dstLength);
    }
    memcpy(*dstData, crop_dstBufs[0], *dstLength);
    free(crop_dstBufs[0]);
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
    DecompressWorker(Callback *callback, unsigned char* srcData, uint32_t srcLength, uint32_t format, uint32_t width, uint32_t height, njt_crop crop, Local<Object> &dstObject, unsigned char* dstData, uint32_t dstBufferLength) :
      AsyncWorker(callback),
      srcData(srcData),
      srcLength(srcLength),
      format(format),
      width(width),
      height(height),
      crop(crop),
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
          &this->crop,
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
    njt_crop crop;

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
  njt_crop crop;
  Local<Object> cropObject;
  Local<Value> cropXObject;
  Local<Value> cropWidthObject;
  Local<Value> cropYObject;
  Local<Value> cropHeightObject;
  Local<Value> cropPreciseObject;

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

    // crop
    cropObject = options->Get(New("crop").ToLocalChecked()).As<Object>();
    if (!cropObject->IsUndefined()) {
      if (!cropObject->IsObject()) {
        _throw("Crop must be an object");
      }

      // set default values
      crop.x = 0;
      crop.y = 0;
      crop.width = 0;
      crop.height = 0;
      crop.precise = false;

      cropXObject = cropObject->Get(New("x").ToLocalChecked());
      if (!cropXObject->IsUndefined()) {
        if (!cropXObject->IsUint32()) {
          _throw("Invalid crop.x");
        }
        crop.x = cropXObject->Uint32Value();
      }
      cropYObject = cropObject->Get(New("y").ToLocalChecked());
      if (!cropYObject->IsUndefined()) {
        if (!cropYObject->IsUint32()) {
          _throw("Invalid crop.y");
        }
        crop.y = cropYObject->Uint32Value();
      }
      cropWidthObject = cropObject->Get(New("width").ToLocalChecked());
      if (!cropWidthObject->IsUndefined()) {
        if (!cropWidthObject->IsUint32()) {
          _throw("Invalid crop.width");
        }
        crop.width = cropWidthObject->Uint32Value();
      }
      cropHeightObject = cropObject->Get(New("height").ToLocalChecked());
      if (!cropHeightObject->IsUndefined()) {
        if (!cropHeightObject->IsUint32()) {
          _throw("Invalid crop.height");
        }
        crop.height = cropHeightObject->Uint32Value();
      }
      cropPreciseObject = cropObject->Get(New("precise").ToLocalChecked());
      if (!cropPreciseObject->IsUndefined()) {
        if (!cropPreciseObject->IsBoolean()) {
          _throw("Invalid crop.precise");
        }
        crop.precise = cropPreciseObject->BooleanValue();
      }
    }
  }

  // Do either async or sync decompress
  if (async) {
    AsyncQueueWorker(new DecompressWorker(callback, srcData, srcLength, format, width, height, crop, dstObject, dstData, dstBufferLength));
    return;
  }
  else {
    retval = decompress(
        srcData,
        srcLength,
        format,
        &width,
        &height,
        &crop,
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
