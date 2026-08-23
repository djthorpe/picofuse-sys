#include "private.h"
#include <limits.h>
#include <nanojpeg.h>
#include <picofuse/pix/jpeg.h>
#include <picofuse/sys.h>
#include <stdbool.h>
#include <stdint.h>

pix_bitmap_t *pix_jpeg_init(const void *data, size_t size) {
  if (data == NULL || size == 0u || size > (size_t)INT_MAX) {
    return NULL;
  }

  sys_mutex_t *mutex = _pix_mutex();
  if (mutex == NULL || !sys_mutex_lock(mutex)) {
    return NULL;
  }

  if (njDecode(data, (int)size) != NJ_OK) {
    njDone();
    sys_mutex_unlock(mutex);
    return NULL;
  }

  int width = njGetWidth();
  int height = njGetHeight();
  if (width <= 0 || height <= 0 || width > UINT16_MAX ||
      height > UINT16_MAX) {
    njDone();
    sys_mutex_unlock(mutex);
    return NULL;
  }

  bool color = njIsColor() != 0;
  size_t pixel_count = (size_t)width * (size_t)height;
  size_t stride = (size_t)width * 3u;
  size_t pixels_size = stride * (size_t)height;

  pix_bitmap_t *bitmap =
      (pix_bitmap_t *)sys_malloc(sizeof(*bitmap) + pixels_size);
  if (bitmap == NULL) {
    njDone();
    sys_mutex_unlock(mutex);
    return NULL;
  }

  uint8_t *pixels = (uint8_t *)(bitmap + 1);
  const uint8_t *src = njGetImage();
  if (color) {
    sys_memcpy(pixels, src, pixels_size);
  } else {
    for (size_t i = 0u; i < pixel_count; i++) {
      uint8_t gray = src[i];
      pixels[i * 3u + 0u] = gray;
      pixels[i * 3u + 1u] = gray;
      pixels[i * 3u + 2u] = gray;
    }
  }
  njDone();
  sys_mutex_unlock(mutex);

  bitmap->data = pixels;
  bitmap->size.w = (uint16_t)width;
  bitmap->size.h = (uint16_t)height;
  bitmap->stride = stride;
  bitmap->fmt = PIX_FMT_RGB888;
  return bitmap;
}
