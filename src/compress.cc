#include "exports.h"

int compress(char* errStr, njt_object* input, njt_object* output) {
  int retval = 0;
  int err;
  tjhandle handle = NULL;
  uint32_t length = 0;
  unsigned long output_length;

  // Init libjpeg-turbo compress
  handle = tjInitCompress();
  if (handle == NULL) {
    _throw(tjGetErrorStr());
  }

  // Figure out buffer size
  length = tjBufSize(input->width, input->height, input->subsampling);

  // If we have pre allocated output buffer check it's length
  if (output->data->length > 0) {
    if (output->data->length < length) {
      _throw("Output buffer too small");
    }
  }
  // If there is no pre allocated output buffer, allocate one
  else {
    output->data->buffer = (unsigned char*)malloc(length * sizeof(unsigned char));
    output->data->length = length;
    output->data->used = length;
    output->data->created = true;
  }

  output_length = output->data->length;

  err = tjCompress2(handle, input->data->buffer, input->width, input->pitch * input->bpp, input->height, input->format, &output->data->buffer, &output_length, input->subsampling, input->quality, TJFLAG_NOREALLOC);

  if (err != 0) {
    _throw(tjGetErrorStr());
  }

  output->data->used = output_length;

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

  if (retval != 0 && output->data->created) {
    free(output->data->buffer);
  }

  return retval;
}

class CompressWorker : public AsyncWorker {
  public:
    CompressWorker(char* errStr, Callback *callback, njt_object* input, njt_object* output, Local<Object>& output_obj) :
      AsyncWorker(callback),
      errStr(errStr),
      input(input),
      output(output) {
        SaveToPersistent("output_obj", output_obj);
      }
    ~CompressWorker() {}

    void Execute () {
      int err;
      err = compress(errStr, input, output);

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
      obj->Set(New("subsampling").ToLocalChecked(), New(this->input->subsampling));

      Local<Value> argv[] = {
        Null(),
        obj
      };

      callback->Call(2, argv);

      del_njt_object(this->input);
      del_njt_object(this->output);
    }

  private:
    char* errStr;
    njt_object* input;
    njt_object* output;
};

NAN_METHOD(Compress) {
  int retval = 0;
  int err;
  static char errStr[NJT_MSG_LENGTH_MAX] = "No error";

  // Input
  Callback *callback = NULL;
  njt_object* input = new_njt_object();
  Local<Object> options;
  Local<Value> subsampling_obj, quality_obj, pitch_obj;

  // Output
  Local<Object> output_obj;

  njt_object* output = new_njt_object();

  if (input == NULL || output == NULL) {
    _throw("Unable to allocate memory");
  }
  input->data->read_only = true;

  // Try to find callback here, so if we want to throw something we can use callback's err
  if (info[info.Length() - 1]->IsFunction()) {
    callback = new Callback(info[info.Length() - 1].As<Function>());
  }

  if ((callback != NULL && info.Length() < 3) || (callback == NULL && info.Length() < 2)) {
    _throw("Too few arguments");
  }

  err = parse_input(errStr, info, input, output, &options, &output_obj);
  if (err != 0) {
    retval = -1;
    goto bailout;
  }

  // Options for compress are not optional
  if (options->IsUndefined()) {
    _throw("Options must be defined");
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

  // Subsampling
  subsampling_obj = options->Get(New("subsampling").ToLocalChecked());
  if (!subsampling_obj->IsUndefined()) {
    if (!subsampling_obj->IsInt32()) {
      _throw("Invalid subsampling");
    }
    input->subsampling = check_subsampling_mode(subsampling_obj->Int32Value());

    if (input->subsampling == -1) {
      _throw("Invalid subsampling");
    }
  }
  else {
    // By default we set subsampling to NJT_DEFAULT_SUBSAMPLING
    // But in case subsampling is not defined and format/bpp is grayscale we can use SAMP_GRAY
    if (input->bpp == 1) {
      input->subsampling = SAMP_GRAY;
    }
  }

  // Quality
  quality_obj = options->Get(New("quality").ToLocalChecked());
  if (!quality_obj->IsUndefined()) {
    if (!quality_obj->IsInt32()) {
      _throw("Invalid quality");
    }
    input->quality = quality_obj->Int32Value();

    if (input->quality <= 0 || input->quality > 100) {
      _throw("Invalid quality");
    }
  }

  // Pitch
  pitch_obj = options->Get(New("pitch").ToLocalChecked());
  if (!pitch_obj->IsUndefined()) {
    if (!pitch_obj->IsInt32()) {
      _throw("Invalid pitch");
    }
    input->pitch = pitch_obj->Int32Value();

    if (input->pitch < input->width) {
      _throw("Invalid pitch");
    }
  }

  if (callback != NULL) {
    AsyncQueueWorker(new CompressWorker(errStr, callback, input, output, output_obj));
  }
  else {
    retval = compress(errStr, input, output);

    if (retval != 0) {
      // compress will set the errStr
      goto bailout;
    }
    Local<Object> obj = New<Object>();

    if (output->data->created) {
      output_obj = NewBuffer((char*)output->data->buffer, output->data->used).ToLocalChecked();
    }

    obj->Set(New("data").ToLocalChecked(), output_obj);
    obj->Set(New("subsampling").ToLocalChecked(), New(input->subsampling));
    obj->Set(New("length").ToLocalChecked(), New(output->data->used));

    info.GetReturnValue().Set(obj);
    del_njt_object(input);
    del_njt_object(output);
  }

  // If we have error throw error or call callback with error
  bailout:
  if (retval != 0) {
    del_njt_object(input);
    del_njt_object(output);

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
