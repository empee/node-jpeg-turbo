#include "exports.h"

int get_bpp(int format) {
  if (format > TJ_NUMPF || format < 0) {
    return -1;
  }
  return tjPixelSize[format];
}

int check_subsampling_mode(int mode) {
  switch (mode) {
    case SAMP_444:
    case SAMP_422:
    case SAMP_420:
    case SAMP_GRAY:
    case SAMP_440:
      return mode;
    default:
      return -1;
  }
}

njt_object *new_njt_object() {
  njt_object *obj = (njt_object*)malloc(sizeof(njt_object));
  if (obj == NULL) return NULL;

  obj->data = (njt_data*)malloc(sizeof(njt_data));
  if (obj->data == NULL) {
    free(obj);
    return NULL;
  }

  // Set default values
  obj->pitch = 0;
  obj->width = 0;
  obj->height = 0;
  obj->format = NJT_DEFAULT_FORMAT;
  obj->bpp = get_bpp(NJT_DEFAULT_FORMAT);
  obj->subsampling = NJT_DEFAULT_SUBSAMPLING;
  obj->quality = NJT_DEFAULT_QUALITY;

  obj->data->buffer = NULL;
  obj->data->length = 0;
  obj->data->used = 0;
  obj->data->created = false;
  obj->data->read_only = false;

  return obj;
}

void del_njt_object(njt_object *obj) {
  if (obj != NULL) {
    free(obj->data);
  }
  free(obj);
}

void copy_njt_object(njt_object *to, njt_object *from) {
  to->width = from->width;
  to->height = from->height;
  to->format = from->format;
  to->bpp = from->bpp;
  to->subsampling = from->subsampling;
  to->quality = from->quality;
}

njt_crop *new_njt_crop() {
  njt_crop *obj = (njt_crop*)malloc(sizeof(njt_crop));
  if (obj == NULL) return NULL;

  // Set default values
  obj->x = 0;
  obj->w = 0;
  obj->y = 0;
  obj->h = 0;
  obj->mcu_x = 0;
  obj->mcu_w = 0;
  obj->mcu_y = 0;
  obj->mcu_h = 0;
  obj->precise = false;

  return obj;
}

void del_njt_crop(njt_crop *obj) {
  free(obj);
}

njt_scale *new_njt_scale() {
  njt_scale *obj = (njt_scale*)malloc(sizeof(njt_scale));
  if (obj == NULL) return NULL;

  // Set default values
  obj->w = 0;
  obj->h = 0;
  obj->mode = NJT_DEFAULT_SCALE;
  obj->keep_aspect = true;

  return obj;
}

void del_njt_scale(njt_scale *obj) {
  free(obj);
}

int parse_input(char* errStr, const FunctionCallbackInfo<Value>& info, njt_object *input, njt_object *output, Local<Object>* options, Local<Object>* output_obj) {
  int retval = 0;
  int cursor = 0;

  Local<Object> input_obj, options_obj;

  // Input buffer
  input_obj = info[cursor++].As<Object>();
  if (!Buffer::HasInstance(input_obj)) {
    _throw("Invalid source buffer");
  }

  input->data->buffer = (unsigned char*) Buffer::Data(input_obj);
  input->data->length = Buffer::Length(input_obj);

  // Options
  options_obj = info[cursor++].As<Object>();

  // Check if options we just got is actually the destination buffer
  // If it is, pull new object from info and set that as options
  if (Buffer::HasInstance(options_obj) && info.Length() > cursor) {
    *output_obj = options_obj;
    options_obj = info[cursor++].As<Object>();
    output->data->buffer = (unsigned char*) Buffer::Data(*output_obj);
    output->data->length = Buffer::Length(*output_obj);
    output->data->used = output->data->length;
  }
  else {
    *output_obj = New<Object>();
  }

  *options = options_obj;

  bailout:
  return retval;
}

int parse_format(char* errStr, Local<Object>options, njt_object* input, njt_object* output) {
  int retval = 0;
  Local<Value> format_obj;

  format_obj = options->Get(New("format").ToLocalChecked());
  if (!format_obj->IsUndefined()) {
    if (!format_obj->IsUint32()) {
      _throw("Invalid format");
    }
    output->bpp = input->bpp = get_bpp(format_obj->Uint32Value());
    if (input->bpp == -1) {
      _throw("Invalid format");
    }
    output->format = input->format = format_obj->Uint32Value();
  }

  bailout:
  return retval;
}

int parse_dimensions(char* errStr, Local<Object>options, njt_object* input) {
  int retval = 0;
  Local<Value> width_obj;
  Local<Value> height_obj;

  // Figure out width & height
  width_obj = options->Get(New("width").ToLocalChecked());
  height_obj = options->Get(New("height").ToLocalChecked());
  if (width_obj->IsUndefined() && height_obj->IsUndefined()) {
    _throw("You must specify either width or height");
  }

  if (!width_obj->IsUndefined()) {
    if (!width_obj->IsInt32()) {
      _throw("Invalid width");
    }
    input->width = width_obj->Int32Value();

    if (input->width == 0 || (uint32_t)input->width * input->bpp > input->data->length) {
      _throw("Invalid width");
    }

    // If height is not defined, calculate it assuming we are consuming entire incoming buffer
    if (height_obj->IsUndefined()) {
      input->height = (input->data->length / (input->width * input->bpp));
    }
  }

  if (!height_obj->IsUndefined()) {
    if (!height_obj->IsInt32()) {
      _throw("Invalid height");
    }
    input->height = height_obj->Int32Value();

    if (input->height == 0 || (uint32_t)input->height * input->bpp > input->data->length) {
      _throw("Invalid height");
    }

    // If width is not defined, calculate it assuming we are consuming entire incoming buffer
    if (width_obj->IsUndefined()) {
      input->width = (input->data->length / (input->height * input->bpp));
    }
  }

  bailout:
  return retval;
}

int parse_scale(char* errStr, Local<Object>options, njt_scale *scale_opt) {
  int retval = 0;
  Local<Object> scale_obj;
  Local<Value> scale_width_obj, scale_height_obj, scale_mode_obj, scale_aspect_obj;

  scale_obj = options->Get(New("scale").ToLocalChecked()).As<Object>();
  if (!scale_obj->IsUndefined()) {
    if (!scale_obj->IsObject()) {
      _throw("Scale must be an object");
    }

    scale_width_obj = scale_obj->Get(New("width").ToLocalChecked());
    if (!scale_width_obj->IsUndefined()) {
      if (!scale_width_obj->IsInt32()) {
        _throw("Invalid scale.width");
      }
      scale_opt->w = scale_width_obj->Int32Value();
    }
    scale_height_obj = scale_obj->Get(New("height").ToLocalChecked());
    if (!scale_height_obj->IsUndefined()) {
      if (!scale_height_obj->IsInt32()) {
        _throw("Invalid scale.height");
      }
      scale_opt->h = scale_height_obj->Int32Value();
    }
    scale_mode_obj = scale_obj->Get(New("mode").ToLocalChecked());
    if (!scale_mode_obj->IsUndefined()) {
      if (!scale_mode_obj->IsUint32()) {
        _throw("Invalid scaling mode");
      }
      scale_opt->mode = check_scale_mode(scale_mode_obj->Uint32Value());
      if (scale_opt->mode == -1) {
        _throw("Invalid scaling mode");
      }
    }
    scale_aspect_obj = scale_obj->Get(New("keepAspect").ToLocalChecked());
    if (!scale_aspect_obj->IsUndefined()) {
      if (!scale_aspect_obj->IsBoolean()) {
        _throw("Invalid keepAspect");
      }
      scale_opt->keep_aspect = scale_aspect_obj->BooleanValue();
    }
  }

  bailout:
  return retval;
}

int parse_crop(char* errStr, Local<Object> options, njt_crop *crop_opt) {
  int retval = 0;
  Local<Object> crop_obj;
  Local<Value> crop_x_obj, crop_width_obj, crop_y_obj, crop_height_obj, crop_precise_obj;

  crop_obj = options->Get(New("crop").ToLocalChecked()).As<Object>();
  if (!crop_obj->IsUndefined()) {
    if (!crop_obj->IsObject()) {
      _throw("Crop must be an object");
    }

    crop_x_obj = crop_obj->Get(New("x").ToLocalChecked());
    if (!crop_x_obj->IsUndefined()) {
      if (!crop_x_obj->IsUint32()) {
        _throw("Invalid crop.x");
      }
      crop_opt->x = crop_x_obj->Uint32Value();
    }
    crop_y_obj = crop_obj->Get(New("y").ToLocalChecked());
    if (!crop_y_obj->IsUndefined()) {
      if (!crop_y_obj->IsUint32()) {
        _throw("Invalid crop.y");
      }
      crop_opt->y = crop_y_obj->Uint32Value();
    }
    crop_width_obj = crop_obj->Get(New("width").ToLocalChecked());
    if (!crop_width_obj->IsUndefined()) {
      if (!crop_width_obj->IsInt32()) {
        _throw("Invalid crop.width");
      }
      crop_opt->w = crop_width_obj->Int32Value();
    }
    crop_height_obj = crop_obj->Get(New("height").ToLocalChecked());
    if (!crop_height_obj->IsUndefined()) {
      if (!crop_height_obj->IsInt32()) {
        _throw("Invalid crop.height");
      }
      crop_opt->h = crop_height_obj->Int32Value();
    }
    crop_precise_obj = crop_obj->Get(New("precise").ToLocalChecked());
    if (!crop_precise_obj->IsUndefined()) {
      if (!crop_precise_obj->IsBoolean()) {
        _throw("Invalid crop.precise");
      }
      crop_opt->precise = crop_precise_obj->BooleanValue();
    }
  }

  bailout:
  return retval;
}

NAN_METHOD(GetBpp) {
  int retval = 0;
  char errStr[NJT_MSG_LENGTH_MAX] = "No error";

  // input
  Callback *callback = NULL;
  Local<Value> bpp_val;

  // Try to find callback here, so if we want to throw something we can use callback's err
  if (info[info.Length() - 1]->IsFunction()) {
    callback = new Callback(info[info.Length() - 1].As<Function>());
  }

  if ((NULL != callback && info.Length() < 2) || (NULL == callback && info.Length() < 1)) {
    _throw("Too few arguments");
  }

  bpp_val = info[0].As<Value>();
  if (!bpp_val->IsUint32()) {
    _throw("Invalid bpp");
  }

  retval = get_bpp(bpp_val->Uint32Value());

  if (retval == -1) {
    _throw("Invalid bpp");
  }

  if (NULL != callback) {
    Local<Value> argv[] = {
      Null(),
      New((uint32_t)retval)
    };
    callback->Call(2, argv);
  }
  else {
    info.GetReturnValue().Set(New((uint32_t)retval));
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

NAN_METHOD(Header) {
  int retval = 0;
  int err;
  char errStr[NJT_MSG_LENGTH_MAX] = "No error";
  tjhandle handle = NULL;

  // input
  Callback *callback = NULL;
  Local<Object> input_obj;
  unsigned char* buffer;
  uint32_t length = 0;

  // Output
  Local<Object> obj = New<Object>();
  int width = 0;
  int height = 0;
  int subsampling = -1;

  // Try to find callback here, so if we want to throw something we can use callback's err
  if (info[info.Length() - 1]->IsFunction()) {
    callback = new Callback(info[info.Length() - 1].As<Function>());
  }

  if ((NULL != callback && info.Length() < 2) || (NULL == callback && info.Length() < 1)) {
    _throw("Too few arguments");
  }

  // Input buffer
  input_obj = info[0].As<Object>();
  if (!Buffer::HasInstance(input_obj)) {
    _throw("Invalid source buffer");
  }

  buffer = (unsigned char*) Buffer::Data(input_obj);
  length = Buffer::Length(input_obj);

  // Init libjpeg-turbo decompress
  handle = tjInitDecompress();
  if (handle == NULL) {
    _throw(tjGetErrorStr());
  }

  retval = tjDecompressHeader2(handle, buffer, length, &width, &height, &subsampling);
  if (retval != 0) {
    _throw(tjGetErrorStr());
  }

  obj->Set(New("width").ToLocalChecked(), New(width));
  obj->Set(New("height").ToLocalChecked(), New(height));
  obj->Set(New("subsampling").ToLocalChecked(), New(subsampling));

  if (NULL != callback) {
    Local<Value> argv[] = {
      Null(),
      obj
    };
    callback->Call(2, argv);
  }
  else {
    info.GetReturnValue().Set(obj);
  }

  bailout:
  if (handle != NULL) {
    err = tjDestroy(handle);
    // If we already have an error retval wont be 0 so in that case we don't want to overwrite error message
    // Also cant use _throw here because infinite-loop
    if (err != 0 && retval == 0) {
      snprintf(errStr, NJT_MSG_LENGTH_MAX, "%s", tjGetErrorStr());
    }
  }

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
  }
}
