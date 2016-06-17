#include "exports.h"

/*
 * Init decompress
 * Calculates crop areas & scale areas
 */
int decompress_init(char* errStr, njt_object* input, njt_object* output, njt_scale* scale_opt, njt_crop* crop_opt, uint32_t* max_length, bool* need_input_copy) {
  int retval = 0;
  int width = input->width;
  int height = input->height;
  uint32_t length = 0;

  *max_length = input->width * input->height * output->bpp;

  if (output->data->length != 0 && output->data->length < *max_length) {
    _throw("Output buffer too small");
  }

  // Since mcu cropping will crop "more" than precise cropping, we will only need to count mcu crop width & height
  if (crop_opt != NULL) {
    retval = crop_mcu_init(errStr, input, crop_opt, &width, &height, need_input_copy);
    if (retval != 0) {
      goto bailout;
    }
    length = width * height * output->bpp;
    if (length > *max_length) *max_length = length;
    if (output->data->length != 0 && output->data->length < *max_length) {
      _throw("Output buffer too small");
    }
  }

  // Scale needs to have crop as well, since in case of precise cropping we need to translate coordinates
  if (scale_opt != NULL) {
    retval = scale_init(errStr, input, scale_opt, crop_opt, &width, &height);
    if (retval != 0) {
      goto bailout;
    }
    length = width * height * output->bpp;
    if (length > *max_length) *max_length = length;
    if (output->data->length != 0 && output->data->length < *max_length) {
      _throw("Output buffer too small");
    }
  }

  bailout:
  return retval;
}

/*
 * Decompress, get header
 */
int decompress_get_header(char* errStr, tjhandle* handle, njt_object* input) {
  int retval = 0;

  retval = tjDecompressHeader2(*handle, input->data->buffer, input->data->length, &input->width, &input->height, &input->subsampling);
  if (retval != 0) {
    _throw(tjGetErrorStr());
  }

  bailout:
  return retval;
}

int decompress(char* errStr, njt_object* input, njt_object* output, njt_scale* scale_opt, njt_crop* crop_opt) {
  int retval = 0;
  int err;
  tjhandle handle = NULL;
  bool need_input_copy = false;
  uint32_t length = 0;

  njt_object* cur_input = input;
  njt_object* cur_output = output;
  bool using_tmp_output = false;
  njt_object* tmp_input = new_njt_object();
  njt_object* tmp_output = new_njt_object();

  if (tmp_input == NULL || tmp_output == NULL) {
    _throw("Unable to allocate memory");
  }

  // Init libjpeg-turbo decompress
  handle = tjInitDecompress();
  if (handle == NULL) {
    _throw(tjGetErrorStr());
  }

  err = decompress_get_header(errStr, &handle, cur_input);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  /*
   * Init our own decompress (counts buffer sizes for us)
   */
  err = decompress_init(errStr, cur_input, cur_output, scale_opt, crop_opt, &length, &need_input_copy);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  if (cur_output->data->length == 0) {
    cur_output->data->buffer = (unsigned char*)malloc(length * sizeof(unsigned char));
    cur_output->data->length = length;
    cur_output->data->created = true;
  }

  // Precise crop & every other scale but SCALE_FAST needs a second pixel buffer
  if ((crop_opt != NULL && crop_opt->precise) || (scale_opt != NULL && scale_opt->mode != SCALE_FAST)) {
    copy_njt_object(tmp_output, cur_output);
    tmp_output->data->buffer = (unsigned char*)malloc(length * sizeof(unsigned char));
    tmp_output->data->length = length;
    tmp_output->data->created = true;
  }

  // MCU cropping returns raw jpeg data so unless specified otherwise input is read only and needs to be copied
  if (need_input_copy) {
    copy_njt_object(tmp_input, input);
    cur_input = tmp_input;
  }

  // Do mcu cropping
  if (crop_opt != NULL) {
    cur_input->data->length = 0;
    err = crop_mcu(errStr, input, cur_input, crop_opt); // use real input, output to cur_input which we just copied
  }

  // Do fast scaling (let libjpeg-turbo scale)
  if (scale_opt != NULL && scale_opt->mode == SCALE_FAST) {
    cur_input->width = scale_opt->w;
    cur_input->height = scale_opt->h;
  }

  err = tjDecompress2(handle, cur_input->data->buffer, cur_input->data->length, cur_output->data->buffer, cur_input->width, 0, cur_input->height, cur_input->format, TJFLAG_FASTDCT | TJFLAG_NOREALLOC);
  if(err != 0) {
    _throw(tjGetErrorStr());
  }
  cur_output->data->used = cur_input->height * cur_input->width * cur_input->bpp;
  copy_njt_object(cur_output, cur_input);

  // Precise cropping
  if (crop_opt != NULL && crop_opt->precise) {
    // We need to move cur_output -> cur_input and create new output, we can't modify the same buffer
    cur_input = cur_output;
    copy_njt_object(cur_input, cur_output);
    cur_output = tmp_output;
    copy_njt_object(cur_output, tmp_output);
    using_tmp_output = true;

    err = crop(errStr, cur_input, cur_output, crop_opt);
    if (err) {
      goto bailout;
    }
  }

  // Precise (as in resolution) scaling
  if (scale_opt != NULL && scale_opt->mode != SCALE_FAST) {
    if (using_tmp_output) {
      cur_input = tmp_output;
      copy_njt_object(cur_input, tmp_output);
      cur_output = output;
      copy_njt_object(cur_output, output);
      using_tmp_output = false;
    }
    else {
      cur_input = cur_output;
      copy_njt_object(cur_input, cur_output);
      cur_output = tmp_output;
      copy_njt_object(cur_output, tmp_input);
      using_tmp_output = true;
    }

    err = scale(errStr, cur_input, cur_output, scale_opt);
    if (err) {
      goto bailout;
    }
  }

  if (using_tmp_output) {
    copy_njt_object(output, tmp_output);
    output->data->used = tmp_output->data->used;
    memcpy(output->data->buffer, tmp_output->data->buffer, tmp_output->data->used);
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

  tjFree(tmp_input->data->buffer);
  free(tmp_output->data->buffer);
  del_njt_object(tmp_input);
  del_njt_object(tmp_output);

  return retval;
}

class DecompressWorker : public AsyncWorker {
  public:
    DecompressWorker(char* errStr, Callback *callback, njt_object* input, njt_object* output, njt_scale* scale_opt, njt_crop* crop_opt, Local<Object>& output_obj) :
      AsyncWorker(callback),
      errStr(errStr),
      input(input),
      output(output),
      scale_opt(scale_opt),
      crop_opt(crop_opt) {
        SaveToPersistent("output_obj", output_obj);
      }
    ~DecompressWorker() {}

    void Execute () {
      int err;

      err = decompress(this->errStr, this->input, this->output, this->scale_opt, this->crop_opt);

      if(err != 0) {
        del_njt_object(this->input);
        del_njt_object(this->output);
        SetErrorMessage(errStr);
      }
    }

    void HandleOKCallback () {
      Local<Object> obj = New<Object>();
      Local<Object> output_obj = GetFromPersistent("output_obj").As<Object>();

      if (this->output->data->created) {
        output_obj = NewBuffer((char*)this->output->data->buffer, this->output->data->used).ToLocalChecked();
      }

      obj->Set(New("length").ToLocalChecked(), New(this->output->data->used));
      obj->Set(New("data").ToLocalChecked(), output_obj);
      obj->Set(New("width").ToLocalChecked(), New(this->output->width));
      obj->Set(New("height").ToLocalChecked(), New(this->output->height));
      obj->Set(New("format").ToLocalChecked(), New(this->output->format));
      obj->Set(New("bpp").ToLocalChecked(), New(this->output->bpp));
      obj->Set(New("subsampling").ToLocalChecked(), New(this->output->subsampling));

      Local<Value> argv[] = {
        Null(),
        obj
      };

      callback->Call(2, argv);
      del_njt_object(this->input);
      del_njt_object(this->output);
      del_njt_crop(this->crop_opt);
    }

  private:
    char* errStr;
    njt_object* input;
    njt_object* output;
    njt_scale* scale_opt;
    njt_crop* crop_opt;
};

NAN_METHOD(Decompress) {
  int retval = 0;
  int err;
  static char errStr[NJT_MSG_LENGTH_MAX] = "No error";

  bool do_scale = false;
  bool do_crop = false;

  // Input
  Callback *callback = NULL;
  njt_object* input = new_njt_object();
  njt_scale* scale_opt = new_njt_scale();
  njt_crop* crop_opt = new_njt_crop();

  Local<Object> options;

  // Output
  Local<Object> output_obj;
  njt_object* output = new_njt_object();

  if (input == NULL || output == NULL || scale_opt == NULL || crop_opt == NULL) {
    _throw("Unable to allocate memory");
  }
  input->data->read_only = true;

  // Try to find callback here, so if we want to throw something we can use callback's err
  if (info[info.Length() - 1]->IsFunction()) {
    callback = new Callback(info[info.Length() - 1].As<Function>());
  }

  if ((callback != NULL && info.Length() < 2) || (callback == NULL && info.Length() < 1)) {
    _throw("Too few arguments");
  }

  err = parse_input(errStr, info, input, output, &options, &output_obj);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  // Options are optional
  if (!options->IsUndefined()) {
    if (!options->IsObject()) {
      _throw("Options must be an object");
    }

    err = parse_format(errStr, options, input, output);
    if (err != 0) {
      retval = -1;
      goto bailout;
    }

    err = parse_scale(errStr, options, scale_opt);
    if (err != 0) {
      retval = -1;
      goto bailout;
    }

    if(scale_opt->w != 0 || scale_opt->h != 0) {
      do_scale = true;
    }

    err = parse_crop(errStr, options, crop_opt);
    if (err != 0) {
      retval = -1;
      goto bailout;
    }

    if (crop_opt->x != 0 || crop_opt->y != 0 || crop_opt->w != 0 || crop_opt->h != 0) {
      do_crop = true;
    }
  }

  // Do either async or sync decompress
  if (callback != NULL) {
    AsyncQueueWorker(new DecompressWorker(errStr, callback, input, output, do_scale?scale_opt:NULL, do_crop?crop_opt:NULL, output_obj));
  }
  else {
    retval = decompress(errStr, input, output, do_scale?scale_opt:NULL, do_crop?crop_opt:NULL);

    if(retval != 0) {
      // decompress will set the errStr
      goto bailout;
    }
    Local<Object> obj = New<Object>();

    if (output->data->created) {
      output_obj = NewBuffer((char*)output->data->buffer, output->data->used).ToLocalChecked();
    }

    obj->Set(New("length").ToLocalChecked(), New(output->data->used));
    obj->Set(New("data").ToLocalChecked(), output_obj);
    obj->Set(New("width").ToLocalChecked(), New(output->width));
    obj->Set(New("height").ToLocalChecked(), New(output->height));
    obj->Set(New("format").ToLocalChecked(), New(output->format));
    obj->Set(New("bpp").ToLocalChecked(), New(output->bpp));
    obj->Set(New("subsampling").ToLocalChecked(), New(output->subsampling));

    info.GetReturnValue().Set(obj);
    del_njt_object(input);
    del_njt_object(output);
    del_njt_crop(crop_opt);
    del_njt_scale(scale_opt);
  }

  // If we have error throw error or call callback with error
  bailout:
  if (retval != 0) {
    del_njt_object(input);
    del_njt_object(output);
    del_njt_crop(crop_opt);
    del_njt_scale(scale_opt);

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
