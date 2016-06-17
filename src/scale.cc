#include "exports.h"

int check_scale_mode(int mode) {
  switch (mode) {
    case SCALE_FAST:
    case SCALE_NEAREST:
    case SCALE_BILINEAR:
    case SCALE_BICUBIC:
      return mode;
    default:
      return -1;
  }
}

int scale_init(char* errStr, njt_object* input, njt_scale* scale_opt, njt_crop* crop_opt, int* width, int* height) {
  int retval = 0;
  tjscalingfactor *sf = NULL;
  int n = 0, i = 0;
  double ratio = 0;
  int scale_width = 0;
  int scale_height = 0;

  if (*width == scale_opt->w && *height == scale_opt->h) {
    goto bailout;
  }

  if (scale_opt->w < 0 || (!scale_opt->keep_aspect && scale_opt->w == 0)) {
    scale_opt->w = input->width + scale_opt->w;
    if(scale_opt->w <= 0) {
      _throw("Scaling width too small");
    }
  }

  if (scale_opt->h < 0 || (!scale_opt->keep_aspect && scale_opt->h == 0)) {
    scale_opt->h = input->height + scale_opt->h;
    if(scale_opt->h <= 0) {
      _throw("Scaling height too small");
    }
  }

  if (scale_opt->w == 0 && scale_opt->h == 0) {
    scale_opt->w = *width;
    scale_opt->h = *height;
    goto bailout;
  }

  if (scale_opt->mode == SCALE_FAST) {
    sf = tjGetScalingFactors(&n);
    if (scale_opt->w != 0 && scale_opt->w < TJSCALED(*width, sf[n-1])) {
      _throw("Scaling width too small");
    }
    if (scale_opt->h != 0 && scale_opt->h < TJSCALED(*height, sf[n-1])) {
      _throw("Scaling height too small");
    }
    for(i=0; i<n; i++) {
      scale_width = TJSCALED(*width, sf[i]);
      scale_height = TJSCALED(*height, sf[i]);
      if ((scale_opt->w == 0 || scale_width <= scale_opt->w) && (scale_opt->h == 0 || scale_height <= scale_opt->h)) {
        break;
      }
    }
    *width = scale_width;
    *height = scale_height;

    if (crop_opt != NULL && crop_opt->precise) {
      crop_opt->x = TJSCALED(crop_opt->x, sf[i]);
      crop_opt->w = TJSCALED(crop_opt->w, sf[i]);
      crop_opt->y = TJSCALED(crop_opt->y, sf[i]);
      crop_opt->h = TJSCALED(crop_opt->h, sf[i]);
      crop_opt->mcu_x = TJSCALED(crop_opt->mcu_x, sf[i]);
      crop_opt->mcu_w = TJSCALED(crop_opt->mcu_w, sf[i]);
      crop_opt->mcu_y = TJSCALED(crop_opt->mcu_y, sf[i]);
      crop_opt->mcu_h = TJSCALED(crop_opt->mcu_h, sf[i]);
    }
  }
  // keep aspect, either because height or width is not set or keep_aspect is set
  else if (scale_opt->keep_aspect || scale_opt->w == 0 || scale_opt->h == 0) {
    // scale height not set
    if (scale_opt->w != 0 && scale_opt->h == 0) {
      scale_height = *height * ((double)scale_opt->w / *width);
      scale_width = scale_opt->w;
    }
    // scale width not set
    else if (scale_opt->h != 0 && scale_opt->w == 0) {
      scale_width = *width * ((double)scale_opt->h / *height);
      scale_height = scale_opt->h;
    }
    // keep aspect
    else {
      scale_width = *width;
      scale_height = *height;

      ratio = (double)scale_opt->w / scale_width;
      scale_width = scale_opt->w;
      scale_height = scale_height * ratio;

      if (scale_height > scale_opt->h) {
        ratio = (double)scale_opt->h / *height;
        scale_width = *width * ratio;
        scale_height = scale_opt->h;
      }
    }
    *width = scale_width;
    *height = scale_height;
  }
  // just scale
  else {
    *width = scale_opt->w;
    *height = scale_opt->h;
  }

  scale_opt->w = *width;
  scale_opt->h = *height;

  bailout:
  return retval;
}

/*
 * Main scaler, handles parsing and checking
 */
int scale(char* errStr, njt_object* input, njt_object* output, njt_scale* scale_opt) {
  int retval = 0;
  int err = 0;

  switch (scale_opt->mode) {
    case SCALE_FAST:
      _throw("Scale: mode only available while decompressing");
    case SCALE_NEAREST:
      err = scale_nearest(input->data->buffer, input->width, input->height, input->bpp, output->data->buffer, scale_opt->w, scale_opt->h);
      if (err == -1) {
        goto bailout;
      }
      break;
    case SCALE_BILINEAR:
      err = scale_bilinear(input->data->buffer, input->width, input->height, input->bpp, output->data->buffer, scale_opt->w, scale_opt->h);
      if (err == -1) {
        goto bailout;
      }
      break;
    case SCALE_BICUBIC:
      err = scale_bicubic(input->data->buffer, input->width, input->height, input->bpp, output->data->buffer, scale_opt->w, scale_opt->h);
      if (err == -1) {
        goto bailout;
      }
      break;
    default:
      _throw("Scale: invalid mode");
  }

  output->data->used = scale_opt->w * scale_opt->h * input->bpp;
  output->width = scale_opt->w;
  output->height = scale_opt->h;
  output->bpp = input->bpp;
  output->format = input->format;

  bailout:
  return retval;
}

/*
 * Nearest neighbor interpolation
 */
int scale_nearest(unsigned char* input, int i_w, int i_h, int bpp, unsigned char* output, int o_w, int o_h) {
  int retval = 0;
  int y_loop, x_loop, y, x, bpp_loop;
  uint32_t i_pos, o_pos;
  double x_scale = ((double)(i_w-1))/o_w;
  double y_scale = ((double)(i_h-1))/o_h;

  for (y_loop = 0, o_pos = 0; y_loop < o_h; y_loop++) {
    y = (int)(y_scale * y_loop);

    for (x_loop = 0; x_loop < o_w; x_loop++) {
      x = (int)(x_scale * x_loop);

      i_pos = (y*i_w+x);

      for (bpp_loop = 0; bpp_loop < bpp; bpp_loop++) {
        output[o_pos++] = input[i_pos*bpp+bpp_loop];
      }

    }

  }

  return retval;
}

/*
 * Bilinear interpolation
 */
int scale_bilinear(unsigned char* input, int i_w, int i_h, int bpp, unsigned char* output, int o_w, int o_h) {
  int retval = 0;
  int y_loop, x_loop, y, x, bpp_loop;
  uint32_t i_pos, o_pos, c_pos;
  uint32_t m_pos = i_w * i_h * bpp;
  unsigned char a, b, c, d;
  double x_diff, y_diff;
  double x_scale = ((double)(i_w-1))/o_w;
  double y_scale = ((double)(i_h-1))/o_h;

  for (y_loop = 0, o_pos = 0; y_loop < o_h; y_loop++) {
    y = (int)(y_scale * y_loop);
    y_diff = (y_scale * y_loop) - y;

    for (x_loop = 0; x_loop < o_w; x_loop++) {
      x = (int)(x_scale * x_loop);
      x_diff = (x_scale * x_loop) - x;

      i_pos = (y*i_w+x);

      for (bpp_loop = 0; bpp_loop < bpp; bpp_loop++) {
        c_pos =  i_pos            *  bpp + bpp_loop; // x    , y
        a = c_pos < m_pos ? input[c_pos] : 0;
        c_pos = (i_pos + 1      ) *  bpp + bpp_loop; // x + 1, y
        b = c_pos < m_pos ? input[c_pos] : 0;
        c_pos = (i_pos + i_w    ) *  bpp + bpp_loop; // x    , y + 1
        c = c_pos < m_pos ? input[c_pos] : 0;
        c_pos = (i_pos + i_w + 1) *  bpp + bpp_loop; // x + 1, y + 1
        d = c_pos < m_pos ? input[c_pos] : 0;
        output[o_pos++] = (unsigned char)(a*(1-x_diff)*(1-y_diff) + b*(x_diff)*(1-y_diff) + c*(y_diff)*(1-x_diff) + d*(x_diff*y_diff));
      }

    }

  }
  return retval;
}

/*
 * Bicubic interpolation
 */
int scale_bicubic(unsigned char* input, int i_w, int i_h, int bpp, unsigned char* output, int o_w, int o_h) {
  int retval = 0;
  int y_loop, x_loop, y, x, bpp_loop, row_loop;
  uint32_t o_pos, i_pos, c_pos;
  uint32_t m_pos = i_w * i_h * bpp;
  unsigned char a, b, c, d;
  double x_diff, y_diff, i[4];
  double x_scale = ((double)(i_w-1))/o_w;
  double y_scale = ((double)(i_h-1))/o_h;

  for (y_loop = 0, o_pos = 0; y_loop < o_h; y_loop++) {
    y = (int)(y_scale * y_loop);
    y_diff = (y_scale * y_loop) - y;

    for (x_loop = 0; x_loop < o_w; x_loop++) {
      x = (int)(x_scale * x_loop);
      x_diff = (x_scale * x_loop) - x;

      i_pos = (y*i_w+x);

      for (bpp_loop = 0; bpp_loop < bpp; bpp_loop++) {

        for (row_loop = -1; row_loop < 3; row_loop++) { // loop from y - 1 to y + 2
          c_pos = (i_pos + (i_w * row_loop) - 1) * bpp + bpp_loop; // x - 1, y + row_loop
          a = c_pos < m_pos ? input[c_pos] : 0;
          c_pos = (i_pos + (i_w * row_loop)    ) * bpp + bpp_loop; // x    , y + row_loop
          b = c_pos < m_pos ? input[c_pos] : 0;
          c_pos = (i_pos + (i_w * row_loop) + 1) * bpp + bpp_loop; // x + 1, y + row_loop
          c = c_pos < m_pos ? input[c_pos] : 0;
          c_pos = (i_pos + (i_w * row_loop) + 2) * bpp + bpp_loop; // x + 2, y + row_loop
          d = c_pos < m_pos ? input[c_pos] : 0;
          i[row_loop+1] = (c - a + (2*a - 5*b + 4*c - d + (3*(b - c) + d - a) * x_diff) * x_diff) * .5 * x_diff + b;
        }

        i[0] = (i[2] - i[0] + (2*i[0] - 5*i[1] + 4*i[2] - i[3] + (3*(i[1] - i[2]) + i[3] - i[0]) * y_diff) * y_diff) * .5 * y_diff + i[1];
        output[o_pos++] = i[0]>255?255:i[0]<0?0:(unsigned char)i[0];
      }

    }

  }
  return retval;
}

NAN_METHOD(Scale) {
  int retval = 0;
  int err;
  static char errStr[NJT_MSG_LENGTH_MAX] = "No error";

  // Input
  Callback *callback = NULL;
  njt_object* input = new_njt_object();
  njt_scale* scale_opt = new_njt_scale();

  Local<Object> options;
  int width = 0;
  int height = 0;

  // Output
  Local<Object> output_obj;
  Local<Object> obj = New<Object>();

  njt_object* output = new_njt_object();

  if (input == NULL || output == NULL || scale_opt == NULL) {
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

  err = parse_scale(errStr, options, scale_opt);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  if (scale_opt->w == 0 && scale_opt->h == 0) {
    _throw("Nothing to scale");
  }

  width = input->width;
  height = input->height;
  retval = scale_init(errStr, input, scale_opt, NULL, &width, &height);
  if (retval != 0) {
    goto bailout;
  }

  // Check that output buffer has enough space
  output->data->used = scale_opt->w * scale_opt->h * input->bpp;
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

  retval = scale(errStr, input, output, scale_opt);
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
  del_njt_scale(scale_opt);

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
