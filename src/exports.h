#ifndef _NODE_JPEG_TURBO_EXPORTS
#define _NODE_JPEG_TURBO_EXPORTS

#include <nan.h>
#include <turbojpeg.h>

using Nan::AsyncWorker;
using Nan::Callback;
using Nan::New;
using Nan::NewBuffer;
using Nan::Null;
using Nan::Set;
using Nan::Error;
using Nan::TypeError;
using Nan::ThrowError;
using Nan::GetFunction;
using Nan::FunctionCallbackInfo;

using v8::Object;
using v8::Value;
using v8::Local;
using v8::Function;
using v8::FunctionTemplate;

using namespace node;

// Unfortunately Travis still uses Ubuntu 12.04, and their libjpeg-turbo is
// super old (1.2.0). We still want to build there, but opt in to the new
// flag when possible.
#ifndef TJFLAG_FASTDCT
#define TJFLAG_FASTDCT 0
#endif

#define NJT_MSG_LENGTH_MAX 200

enum {
  SCALE_FAST = 0,
  SCALE_NEAREST = 1,
  SCALE_BILINEAR = 2,
  SCALE_BICUBIC = 3
};

enum {
  FORMAT_RGB  = TJPF_RGB,
  FORMAT_BGR  = TJPF_BGR,
  FORMAT_RGBX = TJPF_RGBX,
  FORMAT_BGRX = TJPF_BGRX,
  FORMAT_XRGB = TJPF_XRGB,
  FORMAT_XBGR = TJPF_XBGR,
  FORMAT_GRAY = TJPF_GRAY,
  FORMAT_RGBA = TJPF_RGBA,
  FORMAT_BGRA = TJPF_BGRA,
  FORMAT_ABGR = TJPF_ABGR,
  FORMAT_ARGB = TJPF_ARGB,
};

enum {
  SAMP_444  = TJSAMP_444,
  SAMP_422  = TJSAMP_422,
  SAMP_420  = TJSAMP_420,
  SAMP_GRAY = TJSAMP_GRAY,
  SAMP_440  = TJSAMP_440,
};

static int NJT_DEFAULT_QUALITY = 80;
static int NJT_DEFAULT_SUBSAMPLING = SAMP_420;
static int NJT_DEFAULT_FORMAT = FORMAT_RGBA;
static int NJT_DEFAULT_SCALE = SCALE_FAST;

typedef struct {
  int x;
  int w;
  int y;
  int h;
  int mcu_x;
  int mcu_w;
  int mcu_y;
  int mcu_h;
  bool precise;
} njt_crop;

typedef struct {
  unsigned char* buffer;
  uint32_t length;
  uint32_t used;
  bool created;
  bool read_only;
} njt_data;

typedef struct {
  njt_data* data;
  int width;
  int height;
  int format;
  int bpp;
  int subsampling;
  int quality;
  int pitch;
} njt_object;

typedef struct {
  int w;
  int h;
  int mode;
  bool keep_aspect;
} njt_scale;

#define _throw(m) {snprintf(errStr, NJT_MSG_LENGTH_MAX, "%s", m); retval=-1; goto bailout;}

// from utils.cc
int get_bpp(int format);
int check_subsampling_mode(int mode);
njt_object *new_njt_object();
void del_njt_object(njt_object *obj);
void copy_njt_object(njt_object *to, njt_object *from);
njt_crop *new_njt_crop();
void del_njt_crop(njt_crop *obj);
njt_scale *new_njt_scale();
void del_njt_scale(njt_scale *obj);
int parse_input(char* errStr, const FunctionCallbackInfo<Value>& info, njt_object *input, njt_object *output, Local<Object>* options, Local<Object>* output_obj);
int parse_format(char* errStr, Local<Object>options, njt_object* input, njt_object* output);
int parse_dimensions(char* errStr, Local<Object>options, njt_object* input);
int parse_scale(char* errStr, Local<Object>options, njt_scale *scale_opt);
int parse_crop(char* errStr, Local<Object> options, njt_crop *crop_opt);

// from scale.cc
int check_scale_mode(int mode);
int scale_init(char* errStr, njt_object* input, njt_scale* scale_opt, njt_crop* crop_opt, int* width, int* height);
int scale(char* errStr, njt_object* input, njt_object* output, njt_scale* scale_opt);
int scale_nearest(unsigned char* input, int i_w, int i_h, int bpp, unsigned char* output, int o_w, int o_h);
int scale_bilinear(unsigned char* input, int i_w, int i_h, int bpp, unsigned char* output, int o_w, int o_h);
int scale_bicubic(unsigned char* input, int i_w, int i_h, int bpp, unsigned char* output, int o_w, int o_h);

// from crop.cc
int crop(char* errStr, njt_object* input, njt_object* output, njt_crop* crop_opt);
int crop_mcu_init(char* errStr, njt_object* input, njt_crop* crop_opt, int* height, int* width, bool* need_input_copy);
int crop_mcu(char* errStr, njt_object* input, njt_object* output, njt_crop* crop_opt);

// from decompress.cc
int decompress_init(char* errStr, njt_object* input, njt_object* output, njt_scale* scale_opt, njt_crop* crop_opt, uint32_t* max_length, bool* need_input_copy);
int decompress_get_header(char* errStr, tjhandle* handle, njt_object* input);
int decompress(char* errStr, njt_object* input, njt_object* output, njt_scale* scale_opt, njt_crop* crop_opt);

// from compress.cc
int compress(char* errStr, njt_object* input, njt_object* output);

// Nan methods (exported functions)
NAN_METHOD(BufferSize);
NAN_METHOD(Compress);
NAN_METHOD(Decompress);
NAN_METHOD(GetBpp);
NAN_METHOD(Crop);
NAN_METHOD(Scale);
NAN_METHOD(Header);

#endif
