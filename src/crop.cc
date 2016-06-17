#include "exports.h"

/*
 * Crops buffer of any format to wanted size
 */
int crop(char* errStr, njt_object* input, njt_object* output, njt_crop* crop_opt) {
  int retval = 0;
  int input_row_length, output_row_length, offset, i;
  unsigned char* input_pos = input->data->buffer;
  unsigned char* output_pos = output->data->buffer;

  input_row_length = input->width * input->bpp;
  offset = crop_opt->x * input->bpp;
  output_row_length = crop_opt->w * input->bpp;

  input_pos += crop_opt->y * input_row_length;
  for (i = 0; i < (int)crop_opt->h; i++) {
    memcpy(output_pos, input_pos + offset, output_row_length);
    input_pos += input_row_length;
    output_pos += output_row_length;
  }

  output->data->used = crop_opt->w * crop_opt->h * input->bpp;
  output->width = crop_opt->w;
  output->height = crop_opt->h;
  output->bpp = input->bpp;
  output->format = input->format;

  return retval;
}

/*
 * Crop init
 */
int crop_mcu_init(char* errStr, njt_object* input, njt_crop* crop_opt, int* height, int* width, bool* need_input_copy) {
  int retval = 0;

  if (crop_opt->x > 0) {
    crop_opt->mcu_x = crop_opt->x - crop_opt->x % tjMCUWidth[input->subsampling];
  }

  if (crop_opt->mcu_x >= input->width) {
    _throw("Crop: x out of bounds");
  }

  if (crop_opt->w < 0) {
    crop_opt->w = (input->width - crop_opt->x) + crop_opt->w;
  }
  if (crop_opt->w > 0 && crop_opt->w + crop_opt->x < input->width) {
    crop_opt->mcu_w = crop_opt->w + (crop_opt->x - crop_opt->mcu_x);
  }
  else {
    crop_opt->mcu_w = input->width - crop_opt->mcu_x;
    crop_opt->w = crop_opt->mcu_w - (crop_opt->x - crop_opt->mcu_x);
  }

  if (crop_opt->y > 0) {
    crop_opt->mcu_y = crop_opt->y - crop_opt->y % tjMCUHeight[input->subsampling];
  }

  if (crop_opt->mcu_y >= input->height) {
    _throw("Crop: y out of bounds");
  }

  if (crop_opt->h < 0) {
    crop_opt->h = (input->height - crop_opt->y) + crop_opt->h;
  }
  if (crop_opt->h > 0 && crop_opt->h + crop_opt->y < input->height) {
    crop_opt->mcu_h = crop_opt->h + (crop_opt->y - crop_opt->mcu_y);
  }
  else {
    crop_opt->mcu_h = input->height - crop_opt->mcu_y;
    crop_opt->h = crop_opt->mcu_h - (crop_opt->y - crop_opt->mcu_y);
  }

  *width = crop_opt->mcu_w;
  *height = crop_opt->mcu_h;

  *need_input_copy = true;

  bailout:
  return retval;
}

/*
 * Crop by mcu blocks (precise = false)
 */
int crop_mcu(char* errStr, njt_object* input, njt_object* output, njt_crop* crop_opt) {
  int retval = 0;
  tjhandle handle = NULL;
  tjtransform transform;
  tjregion rect;
  unsigned long length = output->data->length;

  handle = tjInitTransform();
  if (handle == NULL) {
    _throw(tjGetErrorStr());
  }

  rect.x = crop_opt->mcu_x;
  rect.y = crop_opt->mcu_y;
  rect.w = crop_opt->mcu_w;
  rect.h = crop_opt->mcu_h;
  transform.r = rect;
  transform.op = TJXOP_NONE;
  transform.options = TJXOPT_CROP;
  transform.data = NULL;
  transform.customFilter = NULL;

  retval = tjTransform(handle, input->data->buffer, input->data->length, 1, &output->data->buffer, &length, &transform, 0); //TJFLAG_NOREALLOC);
  if (retval != 0) {
    _throw(tjGetErrorStr());
  }
  output->data->length = length;

  output->width = crop_opt->mcu_w;
  output->height = crop_opt->mcu_h;
  output->bpp = input->bpp;
  output->format = input->format;

  crop_opt->x = crop_opt->x - crop_opt->mcu_x;
  crop_opt->y = crop_opt->y - crop_opt->mcu_y;

  bailout:
  if (handle != NULL) {
    retval = tjDestroy(handle);
    // todo, need to catch error str here as well!
  }
  return retval;
}

NAN_METHOD(Crop) {
  int retval = 0;
  int err;
  static char errStr[NJT_MSG_LENGTH_MAX] = "No error";

  // Input
  Callback *callback = NULL;
  njt_object* input = new_njt_object();
  njt_crop* crop_opt = new_njt_crop();

  Local<Object> options;

  // Output
  Local<Object> output_obj;
  Local<Object> obj = New<Object>();

  njt_object* output = new_njt_object();

  if (input == NULL || output == NULL || crop_opt == NULL) {
    _throw("Unable to allocate memory");
  }
  input->data->read_only = true;

  // Try to find callback here, so if we want to throw something we can use callback's err
  if (info[info.Length() - 1]->IsFunction()) {
    callback = new Callback(info[info.Length() - 1].As<Function>());
  }

  if ((callback != NULL && info.Length() < 3 ) || (callback == NULL && info.Length() < 2)) {
    _throw("Too few arguments");
  }

  err = parse_input(errStr, info, input, output, &options, &output_obj);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  if (!options->IsObject()) {
    _throw("Options must be an object");
  }

  err = parse_format(errStr, options, input, output);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  err = parse_dimensions(errStr, options, input);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  // Check that incoming buffer has enough data
  if ((uint32_t)input->width * input->height * input->bpp > input->data->length) {
    _throw("Input buffer too small");
  }

  err = parse_crop(errStr, options, crop_opt);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  if(crop_opt->x >= input->width) {
    _throw("Crop: x out of bounds");
  }

  if(crop_opt->y >= input->height) {
    _throw("Crop: y out of bounds");
  }

  // Figure out what we are actually cropping
  if (crop_opt->w < 0) {
    crop_opt->w = (input->width - crop_opt->x) + crop_opt->w;
  }
  if (crop_opt->w <= 0 || crop_opt->w + crop_opt->x > input->width) {
    crop_opt->w = input->width - crop_opt->x;
  }

  if(crop_opt->h < 0) {
    crop_opt->h = (input->height - crop_opt->y) + crop_opt->h;
  }
  if(crop_opt->h <= 0 || crop_opt->h + crop_opt->y > input->height) {
    crop_opt->h = input->height - crop_opt->y;
  }

  // Check that output buffer has enough space
  output->data->used = crop_opt->w * crop_opt->h * input->bpp;
  if (output->data->length != 0) {
    if (output->data->length < output->data->used) {
      _throw("Output buffer too small");
    }
  }
  else {
    output->data->buffer = (unsigned char*)malloc(output->data->used * sizeof(unsigned char));
    output->data->length = output->data->used;
    output->data->created = true;
  }

  retval = crop(errStr, input, output, crop_opt);
  if (retval != 0) {
    goto bailout;
  }

  if (output->data->created) {
    output_obj = NewBuffer((char*)output->data->buffer, output->data->used).ToLocalChecked();
  }

  obj->Set(New("data").ToLocalChecked(), output_obj);
  obj->Set(New("width").ToLocalChecked(), New(output->width));
  obj->Set(New("height").ToLocalChecked(), New(output->height));
  obj->Set(New("length").ToLocalChecked(), New(output->data->used));
  obj->Set(New("format").ToLocalChecked(), New(output->format));
  obj->Set(New("bpp").ToLocalChecked(), New(output->bpp));

  if (callback != NULL) {
    Local<Value> argv[] = {
      Null(),
      obj
    };

    callback->Call(2, argv);
  }
  else {
    info.GetReturnValue().Set(obj);
  }

  // If we have error throw error or call callback with error
  bailout:
  del_njt_object(input);
  del_njt_object(output);
  del_njt_crop(crop_opt);

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
