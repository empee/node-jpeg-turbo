#ifndef _NODE_JPEG_TURBO_EXPORTS
#define _NODE_JPEG_TURBO_EXPORTS

#include <nan.h>
#include <turbojpeg.h>

// Unfortunately Travis still uses Ubuntu 12.04, and their libjpeg-turbo is
// super old (1.2.0). We still want to build there, but opt in to the new
// flag when possible.
#ifndef TJFLAG_FASTDCT
#define TJFLAG_FASTDCT 0
#endif

#define NJT_MSG_LENGTH_MAX 200

static int NJT_DEFAULT_QUALITY = 80;
static int NJT_DEFAULT_SUBSAMPLING = TJSAMP_420;
static int NJT_DEFAULT_FORMAT = TJPF_RGBA;

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

typedef struct {
  uint32_t x;
  uint32_t y;
  int32_t w;
  int32_t h;
  uint32_t mcu_x;
  uint32_t mcu_w;
  uint32_t mcu_y;
  uint32_t mcu_h;
  bool precise;
} njt_crop;

enum {
  SCALE_FAST = 0,
  SCALE_NEAREST = 1,
  SCALE_BILINEAR = 2,
  SCALE_BICUBIC = 3
};

int decompress(unsigned char* srcData, uint32_t srcLength, uint32_t format, uint32_t* width, uint32_t* height, uint32_t scale_mode, njt_crop* crop, uint32_t* dstLength, unsigned char** dstData, uint32_t dstBufferLength);
int scale(unsigned char* image, uint32_t scale_mode, int w, int h, int s_w, int s_h, int bpp);

NAN_METHOD(BufferSize);
NAN_METHOD(CompressSync);
NAN_METHOD(Compress);
NAN_METHOD(DecompressSync);
NAN_METHOD(Decompress);

#endif
