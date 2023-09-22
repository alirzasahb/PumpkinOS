#include <PalmOS.h>

#include "sys.h"
#include "thread.h"
#include "mutex.h"
#include "pwindow.h"
#include "rgb.h"
#include "vfs.h"
#include "bytes.h"
#include "AppRegistry.h"
#include "storage.h"
#include "pumpkin.h"
#include "dbg.h"
#include "debug.h"
#include "xalloc.h"

typedef struct {
  UInt16 density;
} bmp_module_t;

typedef struct {
  UInt16 encoding;
  MemHandle h;
  BitmapType *bitmapP;
} bmp_surface_t;

extern thread_key_t *bmp_key;

static const UInt8 gray1[2]       = {0x00, 0xe6};
static const UInt8 gray1values[2] = {0xff, 0x00};

                                    /* white light-gray dark-gray black */
static const UInt8 gray2[4]       = {0x00, 0xdd, 0xda, 0xe6};
static const UInt8 gray2values[4] = {0xff, 0xaa, 0x55, 0x00};

static const UInt8 gray4[16]       = {0x00, 0xe0, 0xdf, 0x19, 0xde, 0xdd, 0x32, 0xdc, 0xdb, 0xa5, 0xda, 0xd9, 0xbe, 0xd8, 0xd7, 0xe6};
static const UInt8 gray4values[16] = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};

int BmpInitModule(UInt16 density) {
  bmp_module_t *module;

  if ((module = xcalloc(1, sizeof(bmp_module_t))) == NULL) {
    return -1;
  }

  thread_set(bmp_key, module);
  module->density = density;

  debug(DEBUG_TRACE, "Bitmap", "BitmapTypeV0 = %d bytes", sizeof(BitmapTypeV0));
  debug(DEBUG_TRACE, "Bitmap", "BitmapTypeV1 = %d bytes", sizeof(BitmapTypeV1));
  debug(DEBUG_TRACE, "Bitmap", "BitmapTypeV2 = %d bytes", sizeof(BitmapTypeV2));
  debug(DEBUG_TRACE, "Bitmap", "BitmapTypeV3 = %d bytes", sizeof(BitmapTypeV3));

  return 0;
}

int BmpFinishModule(void) {
  bmp_module_t *module = (bmp_module_t *)thread_get(bmp_key);

  if (module) {
    xfree(module);
  }

  return 0;
}

static void BmpEncodeFlags(BitmapFlagsType flags, void *p, UInt16 offset) {
  UInt16 f = 0x0000;
  if (flags.compressed)         f |= 0x8000;  // If true, the bitmap is compressed and the compressionType field specifies the compression used.
  if (flags.hasColorTable)      f |= 0x4000;  // If true, the bitmap has its own color table. If false, the bitmap uses the system color table.
  if (flags.hasTransparency)    f |= 0x2000;  // If true, the OS will not draw pixels that have a value equal to the transparentIndex. If false, the bitmap has no transparency value.
  if (flags.indirect)           f |= 0x1000;
  if (flags.forScreen)          f |= 0x0800;
  if (flags.directColor)        f |= 0x0400;
  if (flags.indirectColorTable) f |= 0x0200;
  if (flags.noDither)           f |= 0x0100;
  put2b(f, p, offset);
}

void BmpFillData(BitmapType *bitmapP) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;

  if (bitmapP) {
    switch (bitmapP->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bitmapP;
        put2b(bmpV0->width,            bmpV0->data,  0);
        put2b(bmpV0->height,           bmpV0->data,  2);
        put2b(bmpV0->rowBytes,         bmpV0->data,  4);
        BmpEncodeFlags(bmpV0->flags,   bmpV0->data,  6);
        put1(0,                        bmpV0->data,  8);
        put1(0,                        bmpV0->data,  9);
        put4b(BITMAP_MAGIC,            bmpV0->data, BITMAP_SPACE-4);
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bitmapP;
        put2b(bmpV1->width,            bmpV1->data,  0);
        put2b(bmpV1->height,           bmpV1->data,  2);
        put2b(bmpV1->rowBytes,         bmpV1->data,  4);
        BmpEncodeFlags(bmpV1->flags,   bmpV1->data,  6);
        put1(bmpV1->pixelSize,         bmpV1->data,  8);
        put1(bmpV1->version,           bmpV1->data,  9);
        put2b(0,                       bmpV1->data, 10);
        put2b(bmpV1->reserved[0],      bmpV1->data, 12);
        put2b(bmpV1->reserved[1],      bmpV1->data, 14);
        put4b(BITMAP_MAGIC,            bmpV1->data, BITMAP_SPACE-4);
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        put2b(bmpV2->width,            bmpV2->data,  0);
        put2b(bmpV2->height,           bmpV2->data,  2);
        put2b(bmpV2->rowBytes,         bmpV2->data,  4);
        BmpEncodeFlags(bmpV2->flags,   bmpV2->data,  6);
        put1(bmpV2->pixelSize,         bmpV2->data,  8);
        put1(bmpV2->version,           bmpV2->data,  9);
        put2b(0,                       bmpV2->data, 10);
        put1(bmpV2->transparentIndex,  bmpV2->data, 12);
        put1(bmpV2->compressionType,   bmpV2->data, 13);
        put2b(bmpV2->reserved,         bmpV2->data, 14);
        put4b(BITMAP_MAGIC,            bmpV2->data, BITMAP_SPACE-4);
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        put2b(bmpV3->width,            bmpV3->data,  0);
        put2b(bmpV3->height,           bmpV3->data,  2);
        put2b(bmpV3->rowBytes,         bmpV3->data,  4);
        BmpEncodeFlags(bmpV3->flags,   bmpV3->data,  6);
        put1(bmpV3->pixelSize,         bmpV3->data,  8);
        put1(bmpV3->version,           bmpV3->data,  9);
        put1(bmpV3->size,              bmpV3->data, 10);
        put1(bmpV3->pixelFormat,       bmpV3->data, 11);
        put1(bmpV3->unused,            bmpV3->data, 12);
        put1(bmpV3->compressionType,   bmpV3->data, 13);
        put2b(bmpV3->density,          bmpV3->data, 14);
        put4b(bmpV3->transparentValue, bmpV3->data, 16);
        put4b(0,                       bmpV3->data, 20);
        put4b(BITMAP_MAGIC,            bmpV3->data, BITMAP_SPACE-4);
        break;
    }
  }
}

BitmapTypeV3 *BmpCreateBitmapV3(const BitmapType *bitmapP, UInt16 density, const void *bitsP, const ColorTableType *colorTableP) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  BitmapTypeV3 *newBmp = NULL;
  ColorTableType *bitmapColorTable;
  UInt16 numEntries, size;
  MemHandle h = NULL;
  BitmapType *tmp;

  if (bitmapP && bitsP && (newBmp = MemPtrNew(sizeof(BitmapTypeV3))) != NULL) {
    // density: if 0, the returned bitmap’s density is set to the default value of kDensityLow
    if (density == 0) density = kDensityLow;

    if (!StoPtrDecoded((void *)bitmapP)) {
      if ((h = MemHandleNew(4096)) != NULL) {
        if ((tmp = pumpkin_create_bitmap(h, (uint8_t *)bitmapP, MemHandleSize(h), bitmapRsc, 1, NULL)) != NULL) {
          debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 decoding native bitmap %p (bits=%d, colorTable=%d) into %p", bitmapP, bitsP ? 1 : 0, colorTableP ? 1 : 0, tmp);
          bitmapP = tmp;
        }
      }
    }

    newBmp->size = 24;
    newBmp->width = bitmapP->width;
    newBmp->height = bitmapP->height;
    newBmp->rowBytes = bitmapP->rowBytes;
    newBmp->density = density;
    newBmp->version = 3;
    newBmp->bits = (void *)bitsP;
    debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 %p %dx%d density %d rowBytes %d", newBmp, newBmp->width, newBmp->height, newBmp->density, newBmp->rowBytes);

    bitmapColorTable = NULL;

    switch (bitmapP->version) {
      case 0:
        debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 create from V0 %p", bitmapP);
        bmpV0 = (BitmapTypeV0 *)bitmapP;
        newBmp->pixelFormat = pixelFormatIndexed;
        newBmp->pixelSize = 1;
        newBmp->bitsSize = bmpV0->bitsSize;
        newBmp->flags = bmpV0->flags;
        newBmp->flags.indirect = true;
        break;
      case 1:
        debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 create from V1 %p", bitmapP);
        bmpV1 = (BitmapTypeV1 *)bitmapP;
        newBmp->pixelFormat = pixelFormatIndexed;
        newBmp->pixelSize = bmpV1->pixelSize;
        newBmp->bitsSize = bmpV1->bitsSize;
        newBmp->flags = bmpV1->flags;
        newBmp->flags.indirect = true;
        break;
      case 2:
        debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 create from V2 %p", bitmapP);
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        newBmp->pixelFormat = pixelFormatIndexed;
        newBmp->pixelSize = bmpV2->pixelSize;
        bitmapColorTable = bmpV2->colorTable;
        newBmp->flags = bmpV2->flags;
        if (bmpV2->flags.hasTransparency) {
          newBmp->transparentValue = bmpV2->pixelSize == 16 ? bmpV2->transparentValue : bmpV2->transparentIndex;
        }
        newBmp->bitsSize = bmpV2->bitsSize;
        newBmp->flags.indirect = true;
        break;
      case 3:
        debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 create from V3 %p", bitmapP);
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        newBmp->pixelFormat = bmpV3->pixelFormat;
        newBmp->pixelSize = bmpV3->pixelSize;
        bitmapColorTable = bmpV3->colorTable;
        newBmp->flags = bmpV3->flags;
        if (bmpV3->flags.hasTransparency) {
          newBmp->transparentValue = bmpV3->transparentValue;
        }
        newBmp->bitsSize = bmpV3->bitsSize;
        newBmp->flags.indirect = true;
        break;
      default:
        debug(DEBUG_ERROR, "Bitmap", "BmpCreateBitmapV3 create from invalid V%d", bitmapP->version);
        break;
    }

    // colorTableP: pointer to a color table, or NULL to use bitmapP’s color table, if one exists.
    if (colorTableP == NULL) colorTableP = bitmapColorTable;

    if (colorTableP) {
      switch (newBmp->pixelSize) {
        case 1: numEntries = 2; break;
        case 2: numEntries = 4; break;
        case 4: numEntries = 16; break;
        case 8: numEntries = 256; break;
        default: numEntries = 0; break;
      }
      if (colorTableP->numEntries == numEntries) {
        size = sizeof(ColorTableType) + colorTableP->numEntries * sizeof(RGBColorType);
        newBmp->colorTable = pumpkin_heap_alloc(size, "ColorTable");
        newBmp->flags.hasColorTable = true;
        xmemcpy(newBmp->colorTable, colorTableP, size);
      } else {
        debug(DEBUG_ERROR, "Bitmap", "BmpCreateBitmapV3 wrong colorTable numEntries %d for depth %d", colorTableP->numEntries, newBmp->pixelSize);
      }
    }

    BmpFillData((BitmapType *)newBmp);

    if (h) {
      //debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 destroying temporary bitmap %p", bitmapP);
      //pumpkin_destroy_bitmap((void *)bitmapP);
    }
  }

  debug(DEBUG_TRACE, "Bitmap", "BmpCreateBitmapV3 returning bitmap %p", newBmp);
  return newBmp;
}

// This function creates an uncompressed, non-transparent VersionTwo bitmap with the width, height, and depth that you specify.
BitmapType *BmpCreate(Coord width, Coord height, UInt8 depth, ColorTableType *colorTableP, UInt16 *error) {
  UInt16 rowBytes, numEntries;
  UInt32 size;
  BitmapTypeV2 *bmpV2 = NULL;

  if (error) *error = sysErrParamErr;

  if (width <= 0 || height <= 0) {
    debug(DEBUG_ERROR, "Bitmap", "BmpCreate invalid width %d or height %d", width, height);
    return NULL;
  }

  switch (depth) {
    case  1: numEntries = 2;   rowBytes = (width + 7) / 8; break;
    case  2: numEntries = 4;   rowBytes = (width + 3) / 4; break;
    case  4: numEntries = 16;  rowBytes = (width + 1) / 2; break;
    case  8: numEntries = 256; rowBytes = width; break;
    case 16: numEntries = 0;   rowBytes = width * 2; break;
    case 24: numEntries = 0;   rowBytes = width * 3; break;
    case 32: numEntries = 0;   rowBytes = width * 4; break;
    default:
      debug(DEBUG_ERROR, "Bitmap", "BmpCreate invalid depth %d", depth);
      return NULL;
  }

  bmpV2 = MemPtrNew(sizeof(BitmapTypeV2));

  if (bmpV2 != NULL) {
    if (error) *error = errNone;

    bmpV2->width = width;
    bmpV2->height = height;
    bmpV2->rowBytes = rowBytes;
    bmpV2->pixelSize = depth;
    bmpV2->version = 2;
    bmpV2->compressionType = BitmapCompressionTypeNone;

    bmpV2->bitsSize = height * rowBytes;
    bmpV2->bits = pumpkin_heap_alloc(bmpV2->bitsSize, "Bits");

    if (bmpV2->bits != NULL) {
      if (colorTableP) {
        // colorTableP: if specified, the number of colors in the color table must match
        // the depth parameter. (2 for 1-bit, 4 for 2-bit, 16 for 4-bit, and 256 for 8-bit).
        // 16-bit bitmaps do not use a color table.

        if (colorTableP->numEntries == numEntries) {
          size = sizeof(ColorTableType) + colorTableP->numEntries * sizeof(RGBColorType);
          if ((bmpV2->colorTable = pumpkin_heap_alloc(size, "ColorTable")) != NULL) {
            bmpV2->flags.hasColorTable = true;
            xmemcpy(bmpV2->colorTable, colorTableP, size);
          } else {
            pumpkin_heap_free(bmpV2->bits, "Bits");
            MemPtrFree(bmpV2);
            bmpV2 = NULL;
            if (error) *error = sysErrNoFreeResource;
          }
        } else {
          debug(DEBUG_ERROR, "Bitmap", "BmpCreate wrong colorTable numEntries %d for depth %d", colorTableP->numEntries, depth);
        }
      }
    } else {
      MemPtrFree(bmpV2);
      bmpV2 = NULL;
      if (error) *error = sysErrNoFreeResource;
    }
  } else {
    if (error) *error = sysErrNoFreeResource;
  }

  BmpFillData((BitmapType *)bmpV2);

  return (BitmapType *)bmpV2;
}

BitmapType *BmpCreate3(Coord width, Coord height, UInt16 density, UInt8 depth, Boolean hasTransparency, UInt32 transparentValue, ColorTableType *colorTableP, UInt16 *error) {
  UInt16 rowBytes, numEntries;
  UInt32 size;
  BitmapTypeV3 *bmpV3 = NULL;

  if (error) *error = sysErrParamErr;

  if (width <= 0 || height <= 0) {
    debug(DEBUG_ERROR, "Bitmap", "BmpCreate3 invalid width %d or height %d", width, height);
    return NULL;
  }

  switch (depth) {
    case  1: numEntries = 2;   rowBytes = (width + 7) / 8; break;
    case  2: numEntries = 4;   rowBytes = (width + 3) / 4; break;
    case  4: numEntries = 16;  rowBytes = (width + 1) / 2; break;
    case  8: numEntries = 256; rowBytes = width; break;
    case 16: numEntries = 0;   rowBytes = width * 2; break;
    case 24: numEntries = 0;   rowBytes = width * 3; break;
    case 32: numEntries = 0;   rowBytes = width * 4; break;
    default:
      debug(DEBUG_ERROR, "Bitmap", "BmpCreate3 invalid depth %d", depth);
      return NULL;
  }

  if ((bmpV3 = MemPtrNew(sizeof(BitmapTypeV3))) != NULL) {
    if (error) *error = errNone;

    bmpV3->width = width;
    bmpV3->height = height;
    bmpV3->rowBytes = rowBytes;
    bmpV3->flags.hasTransparency = hasTransparency;
    bmpV3->pixelSize = depth;
    bmpV3->version = 3;

    bmpV3->size = 24; // XXX fixed bitmap V3 header size
    bmpV3->pixelFormat = pixelFormatIndexed;
    bmpV3->compressionType = BitmapCompressionTypeNone;
    bmpV3->density = density;
    bmpV3->transparentValue = transparentValue;

    bmpV3->bitsSize = height * rowBytes;
    bmpV3->bits = pumpkin_heap_alloc(bmpV3->bitsSize, "Bits");

    if (bmpV3->bits != NULL) {
      if (colorTableP) {
        // colorTableP: if specified, the number of colors in the color table must match
        // the depth parameter. (2 for 1-bit, 4 for 2-bit, 16 for 4-bit, and 256 for 8-bit).
        // 16-bit bitmaps do not use a color table.

        if (colorTableP->numEntries == numEntries) {
          size = sizeof(ColorTableType) + colorTableP->numEntries * sizeof(RGBColorType);
          if ((bmpV3->colorTable = pumpkin_heap_alloc(size, "ColorTable")) != NULL) {
            bmpV3->flags.hasColorTable = true;
            xmemcpy(bmpV3->colorTable, colorTableP, size);
          } else {
            pumpkin_heap_free(bmpV3->bits, "Bits");
            MemPtrFree(bmpV3);
            bmpV3 = NULL;
            if (error) *error = sysErrNoFreeResource;
          }
        } else {
          debug(DEBUG_ERROR, "Bitmap", "BmpCreate3 wrong colorTable numEntries %d for depth %d", colorTableP->numEntries, depth);
        }
      }
    } else {
      MemPtrFree(bmpV3);
      bmpV3 = NULL;
      if (error) *error = sysErrNoFreeResource;
    }
  } else {
    if (error) *error = sysErrNoFreeResource;
  }

  BmpFillData((BitmapType *)bmpV3);

  return (BitmapType *)bmpV3;
}

static uint32_t BmpSurfaceGetPixel(void *data, int x, int y) {
  bmp_surface_t *bsurf = (bmp_surface_t *)data;

  return BmpGetPixelValue(bsurf->bitmapP, x, y);
}

static int BmpSurfaceGetTransparent(void *data, uint32_t *transp) {
  bmp_surface_t *bsurf = (bmp_surface_t *)data;
  UInt32 transparentValue;
  Boolean transparent;

  transparent = BmpGetTransparentValue(bsurf->bitmapP, &transparentValue);
  *transp = transparentValue;

  return transparent;
}

static void BmpSurfaceDestroy(void *data) {
  bmp_surface_t *bsurf = (bmp_surface_t *)data;

  if (bsurf) {
    MemHandleUnlock(bsurf->h);
    DmReleaseResource(bsurf->h);
    xfree(bsurf);
  }
}

surface_t *BmpBitmapCreateSurface(UInt16 id) {
  surface_t *surface = NULL;
  bmp_surface_t *bsurf;
  MemHandle h;
  BitmapType *bitmapP;
  Coord width, height;
  Int16 encoding;
  UInt8 depth;

  if ((h = DmGetResource(bitmapRsc, id)) != NULL) {
    if ((bitmapP = MemHandleLock(h)) != NULL) {
      depth = BmpGetBitDepth(bitmapP);
      switch (depth) {
        case  8: encoding = SURFACE_ENCODING_PALETTE; break;
        case 16: encoding = SURFACE_ENCODING_RGB565;  break;
        case 32: encoding = SURFACE_ENCODING_ARGB;    break;
        default:
          debug(DEBUG_ERROR, "Bitmap", "BmpBitmapCreateSurface unsupported depth %d", depth);
          encoding = -1;
          break;
      }

      if (encoding >= 0) {
        if ((bsurf = xcalloc(1, sizeof(bmp_surface_t))) != NULL) {
          if ((surface = xcalloc(1, sizeof(surface_t))) != NULL) {
            bsurf->h = h;
            bsurf->bitmapP = bitmapP;

            BmpGetDimensions(bitmapP, &width, &height, NULL);
            surface->tag = TAG_SURFACE;
            surface->encoding = encoding;
            surface->width = width;
            surface->height = height;
            surface->getpixel = BmpSurfaceGetPixel;
            surface->gettransp = BmpSurfaceGetTransparent;
            surface->destroy = BmpSurfaceDestroy;
            surface->data = bsurf;
          } else {
            xfree(bsurf);
            MemHandleUnlock(h);
            DmReleaseResource(h);
          }
        } else {
          MemHandleUnlock(h);
          DmReleaseResource(h);
        }
      } else {
        MemHandleUnlock(h);
        DmReleaseResource(h);
      }
    } else {
      DmReleaseResource(h);
    }
  }

  return surface;
}

void BmpPrintChain(BitmapType *bitmapP, DmResType type, DmResID resID, char *label) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  BitmapType *bmp;
  MemHandle h;
  void *decoded, *bits;
  char st[8];

  pumpkin_id2s(type, st);
  if (type) {
    debug(DEBUG_TRACE, "Bitmap", "%s Bitmap chain %s %d bmp=%p begin", label, st, resID, bitmapP);
  } else {
    debug(DEBUG_TRACE, "Bitmap", "%s Bitmap chain %p begin", label, bitmapP);
  }

  for (bmp = bitmapP; bmp;) {
    h = MemPtrRecoverHandle(bmp);
    decoded = StoDecoded(h);
    bits = BmpGetBits(bmp);
    debug(DEBUG_TRACE, "Bitmap", "  V%d %dx%d h=%p bmp=%p decoded=%p bits=%p", bmp->version, bmp->width, bmp->height, h, bmp, decoded, bits);

    switch (bmp->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bmp;
        bmp = bmpV0->next;
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bmp;
        bmp = bmpV1->next;
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bmp;
        bmp = bmpV2->next;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bmp;
        bmp = bmpV3->next;
        break;
    }
  }

  if (type) {
    debug(DEBUG_TRACE, "Bitmap", "%s Bitmap chain %s %d %p end", label, st, resID, bitmapP);
  } else {
    debug(DEBUG_TRACE, "Bitmap", "%s Bitmap chain %p end", label, bitmapP);
  }
}

static void BmpFreeBits(BitmapType *bitmapP) {
  void *bits;

  if (bitmapP && !bitmapP->flags.indirect) {
    if ((bits = BmpGetBits(bitmapP)) != NULL) {
      debug(DEBUG_TRACE, "Bitmap", "BmpFreeBits %p bits %p", bitmapP, bits);
      //dbg_delete(bitmapP);
      pumpkin_heap_free(bits, "Bits");
    }
  }
}

// Only delete bitmaps that have been created using BmpCreate().
// You cannot use this function on a bitmap located in a database.

Err BmpDelete(BitmapType *bitmapP) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  BitmapType *bmp, *next;
  Boolean first;

  debug(DEBUG_TRACE, "Bitmap", "BmpDelete %p begin", bitmapP);

  for (bmp = bitmapP, first = true; bmp; first = false) {
    debug(DEBUG_TRACE, "Bitmap", "BmpDelete V%d %p", bmp->version, bmp);

    switch (bmp->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bmp;
        next = bmpV0->next;
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bmp;
        next = bmpV1->next;
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bmp;
        next = bmpV2->next;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bmp;
        next = bmpV3->next;
        break;
      default:
        debug(DEBUG_ERROR, "Bitmap", "BmpDelete invalid bitmap version %d", bmp->version);
        bmp = next = NULL;
        break;
    }
    if (bmp) {
      debug(DEBUG_TRACE, "Bitmap", "BmpDelete next is %p", next);
      BmpFreeBits(bmp);
      if (first) {
        debug(DEBUG_TRACE, "Bitmap", "MemChunkFree %p", bmp);
        MemChunkFree(bmp); // caused a fault in BookWorm once
      } else {
        StoPtrFree(bmp);
      }
      bmp = next;
    }
  }

  debug(DEBUG_TRACE, "Bitmap", "BmpDelete %p end", bitmapP);

  return errNone;
}

BitmapType *BmpGetBestBitmapEx(BitmapPtr bitmapP, UInt16 density, UInt8 depth, Boolean checkAddr) {
  UInt16 best_depth, best_density;
  Boolean exact_depth;
  Boolean exact_density;
  BitmapType *best;
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  uint8_t *bmp, *base, *end;

  base = (uint8_t *)pumpkin_heap_base();
  end = base + pumpkin_heap_size();

  for (best = NULL, best_depth = 0, best_density = 0, exact_depth = false, exact_density = false; bitmapP;) {
    bmp = (uint8_t *)bitmapP;
    if (checkAddr && (bmp < base || bmp >= end)) {
      debug(DEBUG_ERROR, "Bitmap", "BmpGetBestBitmap invalid bitmap %p", bitmapP);
      break;
    }

    switch (bitmapP->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bitmapP;
        debug(DEBUG_TRACE, "Bitmap", "BmpGetBestBitmap candidate V%d, %dx%d, bpp %d, density %d", bitmapP->version, bmpV0->width, bmpV0->height, 1, kDensityLow);
        if (best == NULL) {
          if (bmpV0->flags.compressed == 0) {
            best = (BitmapType *)bmpV0;
            best_depth = 1;
            best_density = kDensityLow;
            exact_depth = best_depth == depth;
            exact_density = best_density == density;
          } else {
            debug(DEBUG_ERROR, "Bitmap", "BmpGetBestBitmap compressed bitmap V%d can not be used", bitmapP->version);
          }
        }
        bitmapP = bmpV0->next;
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bitmapP;
        debug(DEBUG_TRACE, "Bitmap", "BmpGetBestBitmap candidate V%d, %dx%d, bpp %d, density %d", bitmapP->version, bmpV1->width, bmpV1->height, bmpV1->pixelSize, kDensityLow);
        if (bmpV1->pixelSize > best_depth && !exact_depth) {
          if (bmpV1->flags.compressed == 0) {
            best = (BitmapType *)bmpV1;
            best_depth = bmpV1->pixelSize;
            best_density = kDensityLow;
            exact_depth = best_depth == depth;
            exact_density = best_density == density;
          } else {
            debug(DEBUG_ERROR, "Bitmap", "BmpGetBestBitmap compressed bitmap V%d can not be used", bitmapP->version);
          }
        }
        bitmapP = bmpV1->next;
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        debug(DEBUG_TRACE, "Bitmap", "BmpGetBestBitmap candidate V%d, %dx%d, bpp %d, density %d", bitmapP->version, bmpV2->width, bmpV2->height, bmpV2->pixelSize, kDensityLow);
        if (bmpV2->pixelSize > best_depth && !exact_depth) {
          if (bmpV2->flags.compressed == 0) {
            best = (BitmapType *)bmpV2;
            best_depth = bmpV2->pixelSize;
            best_density = kDensityLow;
            exact_depth = best_depth == depth;
            exact_density = best_density == density;
          } else {
            debug(DEBUG_ERROR, "Bitmap", "BmpGetBestBitmap compressed bitmap V%d can not be used", bitmapP->version);
          }
        }
        bitmapP = bmpV2->next;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        debug(DEBUG_TRACE, "Bitmap", "BmpGetBestBitmap candidate V%d, %dx%d, bpp %d, density %d", bitmapP->version, bmpV3->width, bmpV3->height, bmpV3->pixelSize, bmpV3->density);
        if (bmpV3->pixelSize >= best_depth && !exact_depth && bmpV3->density >= best_density && !exact_density /*bmpV3->density <= density*/) {
          if (bmpV3->flags.compressed == 0) {
            best = (BitmapType *)bmpV3;
            best_depth = bmpV3->pixelSize;
            best_density = bmpV3->density;
            exact_depth = best_depth == depth;
            exact_density = best_density == density;
          } else {
            debug(DEBUG_ERROR, "Bitmap", "BmpGetBestBitmap compressed bitmap V%d can not be used", bitmapP->version);
          }
        }
        bitmapP = bmpV3->next;
        break;
      default:
        debug(DEBUG_ERROR, "Bitmap", "BmpGetBestBitmap invalid version %d", bitmapP->version);
        debug_bytes(DEBUG_ERROR, "Bitmap", (uint8_t *)bitmapP, 256);
        return NULL;
    }
  }

  if (best) {
    debug(DEBUG_TRACE, "Bitmap", "BmpGetBestBitmap V%d, %dx%d, bpp %d (%d), density %d",
      best->version, best->width, best->height, best_depth, exact_depth ? 1 : 0, best_density);
  } else {
    debug(DEBUG_ERROR, "Bitmap", "BmpGetBestBitmap no suitable bitmap for bpp %d, density %d", depth, density);
  }

  return best;
}

BitmapType *BmpGetBestBitmap(BitmapPtr bitmapP, UInt16 density, UInt8 depth) {
  return BmpGetBestBitmapEx(bitmapP, density, depth, true);
}

// Compress or uncompress a bitmap.
Err BmpCompress(BitmapType *bitmapP, BitmapCompressionType compType) {
  debug(DEBUG_ERROR, "Bitmap", "BmpCompress not implemented");
  return sysErrParamErr;
}

void *BmpGetBits(BitmapType *bitmapP) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  void *bits = NULL;

  if (bitmapP) {
    switch (bitmapP->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bitmapP;
        bits = bmpV0->bits;
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bitmapP;
        bits = bmpV1->bits;
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        bits = bmpV2->bits;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        bits = bmpV3->bits;
        break;
    }
  }

  return bits;
}

ColorTableType *BmpGetColortable(BitmapType *bitmapP) {
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  ColorTableType *colorTable = NULL;

  if (bitmapP) {
    switch (bitmapP->version) {
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        colorTable = bmpV2->colorTable;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        colorTable = bmpV3->colorTable;
        break;
    }
  }

  return colorTable;
}

// Returns the size in bytes of the bitmap, including its header, color table (if any), and sizeof(BitmapDirectInfoType) if one exists.
UInt16 BmpSize(const BitmapType *bitmapP) {
  UInt32 headerSize, dataSize;

  BmpGetSizes(bitmapP, &dataSize, &headerSize);

  return headerSize + dataSize;
}

UInt16 BmpBitsSize(const BitmapType *bitmapP) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  UInt16 bitsSize = 0;

  if (bitmapP) {
    switch (bitmapP->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bitmapP;
        bitsSize = bmpV0->bitsSize;
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bitmapP;
        bitsSize = bmpV1->bitsSize;
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        bitsSize = bmpV2->bitsSize;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        bitsSize = bmpV3->bitsSize;
        break;
    }
  }

  return bitsSize;
}

void BmpGetSizes(const BitmapType *bitmapP, UInt32 *dataSizeP, UInt32 *headerSizeP) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;

  // headerSize is not accurate, because BitmapTypeVn is bigger than on real PalmOS

  if (bitmapP) {
    switch (bitmapP->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bitmapP;
        if (dataSizeP) *dataSizeP = bmpV0->bitsSize;
        if (headerSizeP) *headerSizeP = sizeof(BitmapTypeV0) + BmpColortableSize(bitmapP);
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bitmapP;
        if (dataSizeP) *dataSizeP = bmpV1->bitsSize;
        if (headerSizeP) *headerSizeP = sizeof(BitmapTypeV1) + BmpColortableSize(bitmapP);
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        if (dataSizeP) *dataSizeP = bmpV2->bitsSize;
        if (headerSizeP) *headerSizeP = sizeof(BitmapTypeV2) + BmpColortableSize(bitmapP);
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        if (dataSizeP) *dataSizeP = bmpV3->bitsSize;
        if (headerSizeP) *headerSizeP = sizeof(BitmapTypeV3) + BmpColortableSize(bitmapP);
        break;
    }
  }
}

UInt16 BmpColortableSize(const BitmapType *bitmapP) {
  ColorTableType *colorTable;
  UInt16 r = 0;

  if (bitmapP) {
    if ((colorTable = BmpGetColortable((BitmapType *)bitmapP)) != NULL) {
      // XXX is it correct to include size of field numEntries (UInt16) ?
      r = sizeof(UInt16) + colorTable->numEntries * sizeof(RGBColorType);
    }
  }

  return r;
}

void BmpGetDimensions(const BitmapType *bitmapP, Coord *widthP, Coord *heightP, UInt16 *rowBytesP) {
  if (bitmapP) {
    if (widthP) *widthP = bitmapP->width;
    if (heightP) *heightP = bitmapP->height;
    if (rowBytesP) *rowBytesP = bitmapP->rowBytes;
  }
}

UInt8 BmpGetBitDepth(const BitmapType *bitmapP) {
  UInt8 depth = 0;
  if (bitmapP) {
    depth = bitmapP->version == 0 ? 1 : bitmapP->pixelSize;
  }
  return depth;
}

BitmapType *BmpGetNextBitmapAnyDensity(BitmapType *bitmapP) {
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;

  if (bitmapP) {
    switch (bitmapP->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bitmapP;
        bitmapP = bmpV0->next;
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bitmapP;
        bitmapP = bmpV1->next;
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        bitmapP = bmpV2->next;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        bitmapP = bmpV3->next;
        break;
      default:
        debug(DEBUG_ERROR, "Bitmap", "BmpGetNextBitmap invalid version %d", bitmapP->version);
        debug_bytes(DEBUG_ERROR, "Bitmap", (uint8_t *)bitmapP, 256);
        bitmapP = NULL;
        break;
    }
  }

  return bitmapP;
}

BitmapType *BmpGetNextBitmap(BitmapType *bitmapP) {
  for (;;) {
    bitmapP = BmpGetNextBitmapAnyDensity(bitmapP);
    if (bitmapP == NULL) break;
    if (BmpGetDensity(bitmapP) == kDensityDouble) continue;
  }

  return bitmapP;
}

UInt8 BmpGetVersion(const BitmapType *bitmapP) {
  return bitmapP ? bitmapP->version : 0;
}

BitmapCompressionType BmpGetCompressionType(const BitmapType *bitmapP) {
  BitmapTypeV2 *bmpV2;
  BitmapCompressionType ct = BitmapCompressionTypeNone;

  if (bitmapP) {
    if (bitmapP->version >= 2) {
      bmpV2 = (BitmapTypeV2 *)bitmapP;
      ct = bmpV2->compressionType;
    }
  }

  return ct;
}

UInt16 BmpGetDensity(const BitmapType *bitmapP) {
  BitmapTypeV3 *bmpV3;
  UInt16 d = kDensityLow;

  if (bitmapP) {
    if (bitmapP->version >= 3) {
      bmpV3 = (BitmapTypeV3 *)bitmapP;
      d = bmpV3->density;
    }
  }

  return d;
}

Err BmpSetDensity(BitmapType *bitmapP, UInt16 density) {
  BitmapTypeV3 *bmpV3;
  Err err = sysErrParamErr;

  if (bitmapP) {
    if (bitmapP->version >= 3) {
      switch (density) {
        case kDensityLow:
        case kDensityDouble:
          bmpV3 = (BitmapTypeV3 *)bitmapP;
          bmpV3->density = density;
          err = errNone;
          break;
       }
    }
  }

  return err;
}

Boolean BmpGetTransparentValue(const BitmapType *bitmapP, UInt32 *transparentValueP) {
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  Boolean r = false;

  if (bitmapP) {
    switch (bitmapP->version) {
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        if (bmpV2->flags.hasTransparency) {
          //*transparentValueP = bmpV2->transparentValue ? bmpV2->transparentValue : bmpV2->transparentIndex;
          *transparentValueP = bmpV2->pixelSize == 16 ? bmpV2->transparentValue : bmpV2->transparentIndex;
          r = true;
        }
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        if (bmpV3->flags.hasTransparency) {
          *transparentValueP = bmpV3->transparentValue;
          r = true;
        }
        break;
      default:
        break;
    }
  }

  return r;
}

void BmpSetTransparentValue(BitmapType *bitmapP, UInt32 transparentValue) {
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;

  if (bitmapP) {
    switch (bitmapP->version) {
      case 2:
        bmpV2 = (BitmapTypeV2 *)bitmapP;
        if (transparentValue == kTransparencyNone) {
          bmpV2->flags.hasTransparency = 0;
          bmpV2->transparentIndex = 0;
        } else {
          bmpV2->flags.hasTransparency = 1;
          if (bmpV2->pixelSize == 8) {
            bmpV2->transparentIndex = transparentValue;
            debug(DEBUG_TRACE, "Bitmap", "BmpSetTransparentValue V2 index %d", bmpV2->transparentIndex);
          } else if (bmpV2->pixelSize > 8) {
            bmpV2->transparentValue = transparentValue;
            debug(DEBUG_TRACE, "Bitmap", "BmpSetTransparentValue V2 value 0x%04X", bmpV2->transparentIndex);
          }
        }
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bitmapP;
        if (transparentValue == kTransparencyNone) {
          bmpV3->flags.hasTransparency = 0;
          bmpV3->transparentValue = 0;
        } else {
          bmpV3->flags.hasTransparency = 1;
          bmpV3->transparentValue = transparentValue;
          if (bmpV3->pixelSize == 8) {
            debug(DEBUG_TRACE, "Bitmap", "BmpSetTransparentValue V3 index %d", bmpV3->transparentValue);
          } else if (bmpV3->pixelSize > 8) {
            debug(DEBUG_TRACE, "Bitmap", "BmpSetTransparentValue V3 value 0x%04X", bmpV3->transparentValue);
          }
        }
        break;
      default:
        debug(DEBUG_ERROR, "Bitmap", "BmpSetTransparentValue V%d index %d not supported", bitmapP->version, transparentValue);
        break;
    }
  }
}

static UInt8 BmpRGBToIndex(UInt8 red, UInt8 green, UInt8 blue, ColorTableType *colorTable) {
  Int32 dr, dg, db;
  UInt32 i, d, dmin, imin;

  dmin = 0xffffffff;
  imin = 0;

  for (i = 0; i < colorTable->numEntries; i++) {
    if (red == colorTable->entry[i].r && green == colorTable->entry[i].g && blue == colorTable->entry[i].b) {
      return i;
    }
    dr = (Int32)red - (Int32)colorTable->entry[i].r;
    dr = dr * dr;
    dg = (Int32)green - (Int32)colorTable->entry[i].g;
    dg = dg * dg;
    db = (Int32)blue - (Int32)colorTable->entry[i].b;
    db = db * db;
    d = dr + dg + db;
    if (d < dmin) {
      dmin = d;
      imin = i;
    }
  }

  return imin;
}

static void BmpIndexToRGB(UInt8 i, UInt8 *red, UInt8 *green, UInt8 *blue, ColorTableType *colorTable) {
  *red   = colorTable->entry[i].r;
  *green = colorTable->entry[i].g;
  *blue  = colorTable->entry[i].b;
}

IndexedColorType BmpGetPixel(BitmapType *bitmapP, Coord x, Coord y) {
  UInt8 red, green, blue;
  UInt32 value;
  IndexedColorType b = 0;

  if (bitmapP) {
    switch (BmpGetBitDepth(bitmapP)) {
      case  1:
        b = BmpGetPixelValue(bitmapP, x, y);
        b = gray1[b];
        break;
      case  2:
        b = BmpGetPixelValue(bitmapP, x, y);
        b = gray2[b];
        break;
      case  4:
        b = BmpGetPixelValue(bitmapP, x, y);
        b = gray4[b];
        break;
      case  8:
        b = BmpGetPixelValue(bitmapP, x, y);
        break;
      case 16:
        value = BmpGetPixelValue(bitmapP, x, y);
        red   = r565(value);
        green = g565(value);
        blue  = b565(value);
        b = BmpRGBToIndex(red, green, blue, pumpkin_defaultcolorTable());
        break;
      case 24:
        value = BmpGetPixelValue(bitmapP, x, y);
        red   = r24(value);
        green = g24(value);
        blue  = b24(value);
        b = BmpRGBToIndex(red, green, blue, pumpkin_defaultcolorTable());
        break;
      case 32:
        value = BmpGetPixelValue(bitmapP, x, y);
        red   = r32(value);
        green = g32(value);
        blue  = b32(value);
        b = BmpRGBToIndex(red, green, blue, pumpkin_defaultcolorTable());
        break;
    }
  }

  return b;
}

UInt32 BmpGetPixelValue(BitmapType *bitmapP, Coord x, Coord y) {
  UInt8 b, *bits;
  UInt16 w;
  UInt32 offset;
  UInt32 value = 0;

  if (bitmapP) {
    bits = BmpGetBits(bitmapP);

    switch (BmpGetBitDepth(bitmapP)) {
      case  1:
        offset = y * bitmapP->rowBytes + (x >> 3);
        b = bits[offset];
        value = (b >> (7 - (x & 0x07))) & 0x01;
        break;
      case  2:
        offset = y * bitmapP->rowBytes + (x >> 2);
        b = bits[offset];
        value = (b >> (3 - (x & 0x03))) & 0x03;
        //value = (b >> ((x & 0x03) << 1)) & 0x03;
        break;
      case  4:
        offset = y * bitmapP->rowBytes + (x >> 1);
        b = bits[offset];
        //value = (x & 0x01) ? b & 0x0F : b >> 4;
        value = (x & 0x01) ? b >> 4 : b & 0x0F;
        break;
      case  8:
        offset = y * bitmapP->rowBytes + x;
        value = bits[offset];
        break;
      case 16:
        offset = y * bitmapP->rowBytes + x*2;
        get2b(&w, bits, offset);
        value = w;
        break;
      case 24:
        offset = y * bitmapP->rowBytes + x*3;
        value = rgb24(bits[offset], bits[offset+1], bits[offset+2]);
        break;
      case 32:
        offset = y * bitmapP->rowBytes + x*4;
        get4l(&value, bits, offset);
        break;
    }
  }

  return value;
}

Err BmpGetPixelRGB(BitmapType *bitmapP, Coord x, Coord y, RGBColorType *rgbP) {
  ColorTableType *colorTable;
  UInt8 b, *bits;
  UInt16 w;
  UInt32 offset;
  Err err = sysErrParamErr;

  if (bitmapP && rgbP) {
    switch (BmpGetBitDepth(bitmapP)) {
      case  1:
        b = BmpGetPixelValue(bitmapP, x, y);
        rgbP->r = rgbP->g = rgbP->b = gray1values[b];
        break;
      case  2:
        b = BmpGetPixelValue(bitmapP, x, y);
        rgbP->r = rgbP->g = rgbP->b = gray2values[b];
        break;
      case  4:
        b = BmpGetPixelValue(bitmapP, x, y);
        rgbP->r = rgbP->g = rgbP->b = gray4values[b];
        break;
      case  8:
        b = BmpGetPixelValue(bitmapP, x, y);
        colorTable = BmpGetColortable(bitmapP);
        if (colorTable == NULL) colorTable = pumpkin_defaultcolorTable();
        BmpIndexToRGB(b, &rgbP->r, &rgbP->g, &rgbP->b, colorTable);
        break;
      case 16:
        w = BmpGetPixelValue(bitmapP, x, y);
        rgbP->r = r565(w);
        rgbP->g = g565(w);
        rgbP->b = b565(w);
        break;
      case 24:
        bits = BmpGetBits(bitmapP);
        offset = y * bitmapP->rowBytes + x*3;
        rgbP->r = bits[offset];
        rgbP->g = bits[offset+1];
        rgbP->b = bits[offset+2];
        break;
      case 32:
        bits = BmpGetBits(bitmapP);
        offset = y * bitmapP->rowBytes + x*4;
        // little-endian: B G R A
        rgbP->b = bits[offset];
        rgbP->g = bits[offset+1];
        rgbP->g = bits[offset+2];
        break;
    }

    err = errNone;
  }

  return err;
}

void BmpDrawSurface(BitmapType *bitmapP, Coord sx, Coord sy, Coord w, Coord h, surface_t *surface, Coord x, Coord y, Boolean useTransp) {
  ColorTableType *colorTable;
  UInt32 offset, transparentValue, c;
  Int32 offsetb;
  UInt8 *bits, b, red, green, blue, gray;
  UInt16 rgb;
  Coord i, j, k;
  Boolean transp;

//debug(1, "XXX", "BmpDraw sx=%d sy=%d w=%d h=%d x=%d y=%d", sx, sy, w, h, x, y);
  if (bitmapP && surface && w > 0 && h > 0 && sx < bitmapP->width && sy < bitmapP->height) {
//debug(1, "XXX", "BmpDraw coords ok");
    bits = BmpGetBits(bitmapP);

    if (bits) {
//debug(1, "XXX", "BmpDraw bits ok");
      if (sx < 0) {
        w += sx;
        x -= sx;
        sx = 0;
      }
      if ((sx + w) >= bitmapP->width) {
        w = bitmapP->width - sx;
      }
      if (sy < 0) {
        h += sy;
        y -= sy;
        sy = 0;
      }
      if ((sy + h) >= bitmapP->height) {
        h = bitmapP->height - sy;
      }
//debug(1, "XXX", "BmpDraw adjust sx=%d sy=%d w=%d h=%d x=%d y=%d", sx, sy, w, h, x, y);

      if (w > 0 && h > 0) {
//debug(1, "XXX", "BmpDraw w h ok");
        transp = BmpGetTransparentValue(bitmapP, &transparentValue);

        switch (BmpGetBitDepth(bitmapP)) {
          case 1:
            offset = sy * bitmapP->rowBytes + sx / 8;
            for (i = 0; i < h; i++, offset += bitmapP->rowBytes) {
              offsetb = 7 - (sx % 8);
              k = 0;
              for (j = 0; j < w; j++) {
                b = bits[offset + k] & (1 << offsetb);
                gray = gray1values[b ? 1 : 0];
                c = surface_color_rgb(surface->encoding, surface->palette, surface->npalette, gray, gray, gray, 0xff);
                surface->setpixel(surface->data, x+j, y+i, c);
                offsetb--;
                if (offsetb == -1) {
                  offsetb = 7;
                  k++;
                }
              }
            }
            break;
          case 2:
            offset = sy * bitmapP->rowBytes + sx / 4;
            for (i = 0; i < h; i++, offset += bitmapP->rowBytes) {
              offsetb = (3 - (sx % 4)) << 1;
              k = 0;
              for (j = 0; j < w; j++) {
                b = (bits[offset + k] & (3 << offsetb)) >> offsetb;
                gray = gray2values[b];
                c = surface_color_rgb(surface->encoding, surface->palette, surface->npalette, gray, gray, gray, 0xff);
                surface->setpixel(surface->data, x+j, y+i, c);
                offsetb -= 2;
                if (offsetb == -2) {
                  offsetb = 6;
                  k++;
                }
              }
            }
            break;
          case 4:
            offset = sy * bitmapP->rowBytes + sx / 2;
            for (i = 0; i < h; i++, offset += bitmapP->rowBytes) {
              offsetb = (sx % 2) << 2;
              k = 0;
              for (j = 0; j < w; j++) {
                b = (bits[offset + k] & (0xf << offsetb)) >> offsetb;
                gray = gray4values[b];
                c = surface_color_rgb(surface->encoding, surface->palette, surface->npalette, gray, gray, gray, 0xff);
                surface->setpixel(surface->data, x+j, y+i, c);
                offsetb += 4;
                if (offsetb == 8) {
                  offsetb = 0;
                  k++;
                }
              }
            }
            break;
          case 8:
            colorTable = BmpGetColortable(bitmapP);
            if (colorTable == NULL) colorTable = pumpkin_defaultcolorTable();
            offset = sy * bitmapP->rowBytes + sx;
            for (i = 0; i < h; i++, offset += bitmapP->rowBytes) {
              for (j = 0; j < w; j++) {
                b = bits[offset + j];
                if (!useTransp || !transp || b != transparentValue) {
                  BmpIndexToRGB(b, &red, &green, &blue, colorTable);
                  c = surface_color_rgb(surface->encoding, surface->palette, surface->npalette, red, green, blue, 0xff);
                  surface->setpixel(surface->data, x+j, y+i, c);
                }
              }
            }
            break;
          case 16:
            offset = sy * bitmapP->rowBytes + sx*2;
//debug(1, "XXX", "BmpDraw depth=16 rb=%d offset=%d", bitmapP->rowBytes, offset);
            for (i = 0; i < h; i++, offset += bitmapP->rowBytes) {
              for (j = 0, k = 0; j < w; j++, k += 2) {
                get2b(&rgb, bits, offset + k);
                if (!useTransp || !transp || rgb != transparentValue) {
                  red   = r565(rgb);
                  green = g565(rgb);
                  blue  = b565(rgb);
                  c = surface_color_rgb(surface->encoding, surface->palette, surface->npalette, red, green, blue, 0xff);
                  surface->setpixel(surface->data, x+j, y+i, c);
                }
              }
            }
            break;
          case 24:
            offset = sy * bitmapP->rowBytes + sx*3;
            for (i = 0; i < h; i++, offset += bitmapP->rowBytes) {
              for (j = 0, k = 0; j < w; j++, k += 3) {
                red   = bits[offset];
                green = bits[offset+1];
                blue  = bits[offset+2];
                if (!useTransp || !transp || rgb24(red, green, blue) != transparentValue) {
                  c = surface_color_rgb(surface->encoding, surface->palette, surface->npalette, red, green, blue, 0xff);
                  surface->setpixel(surface->data, x+j, y+i, c);
                }
              }
            }
            break;
          case 32:
            offset = sy * bitmapP->rowBytes + sx*4;
            for (i = 0; i < h; i++, offset += bitmapP->rowBytes) {
              for (j = 0, k = 0; j < w; j++, k += 4) {
                // little-endian: B G R A
                blue  = bits[offset];
                green = bits[offset+1];
                red   = bits[offset+2];
                if (!useTransp || !transp || rgb32(red, green, blue) != transparentValue) {
                  c = surface_color_rgb(surface->encoding, surface->palette, surface->npalette, red, green, blue, 0xff);
                  surface->setpixel(surface->data, x+j, y+i, c);
                }
              }
            }
            break;
        }
      }
    }
  }
}

static UInt32 BmpConvertFrom1Bit(UInt32 b, UInt8 depth, ColorTableType *colorTable, Boolean isDefault) {
  switch (depth) {
    case 1: break;
    case 2: b = b ? 0x03 : 0x00; break;
    case 4: b = b ? 0x0F : 0x00; break;
    case 8:
      if (isDefault) {
        b = b ? 0xE6 : 0x00;
      } else {
        b = b ? BmpRGBToIndex(0x00, 0x00, 0x00, colorTable) : BmpRGBToIndex(0xFF, 0xFF, 0xFF, colorTable);
      }
      break;
    case 16:
      b = b ? 0x0000 : 0xFFFF;
      break;
    case 24:
      b = b ? 0x000000 : 0xFFFFFF;
      break;
    case 32:
      b = b ? 0x000000 : 0xFFFFFF;
      break;
  }

  return b;
}

/*
0x00: 0xff,0xff,0xff
0xe0: 0xee,0xee,0xee
0xdf: 0xdd,0xdd,0xdd
0x19: 0xcc,0xcc,0xcc
0xde: 0xbb,0xbb,0xbb
0xdd: 0xaa,0xaa,0xaa
0x32: 0x99,0x99,0x99
0xdc: 0x88,0x88,0x88
0xdb: 0x77,0x77,0x77
0xa5: 0x66,0x66,0x66
0xda: 0x55,0x55,0x55
0xd9: 0x44,0x44,0x44
0xbe: 0x33,0x33,0x33
0xd8: 0x22,0x22,0x22
0xd7: 0x11,0x11,0x11
0xe6: 0x00,0x00,0x00
*/

static UInt32 BmpConvertFrom2Bits(UInt32 b, UInt8 depth, ColorTableType *colorTable, Boolean isDefault) {
  switch (depth) {
    case 1: b = b ? 1 : 0; break;
    case 2: break;
    case 4: b = b << 2; break;
    case 8:
      if (isDefault) {
        b = gray2[b];
      } else {
        b = BmpRGBToIndex(gray2values[b], gray2values[b], gray2values[b], colorTable);
      }
      break;
    case 16:
      b = rgb565(gray2values[b], gray2values[b], gray2values[b]);
      break;
    case 24:
      b = rgb24(gray2values[b], gray2values[b], gray2values[b]);
      break;
    case 32:
      b = rgb32(gray2values[b], gray2values[b], gray2values[b]);
      break;
  }

  return b;
}

static UInt32 BmpConvertFrom4Bits(UInt32 b, UInt8 depth, ColorTableType *colorTable, Boolean isDefault) {
  switch (depth) {
    case 1: b = b ? 1 : 0; break;
    case 2: b = b >> 2; break;
    case 4: break;
    case 8:
      if (isDefault) {
        b = gray4[b];
      } else {
        b = BmpRGBToIndex(gray4values[b], gray4values[b], gray4values[b], colorTable);
      }
      break;
    case 16:
      b = rgb565(gray4values[b], gray4values[b], gray4values[b]);
      break;
    case 24:
      b = rgb24(gray4values[b], gray4values[b], gray4values[b]);
      break;
    case 32:
      b = rgb32(gray4values[b], gray4values[b], gray4values[b]);
      break;
  }

  return b;
}

static UInt8 rgbToGray1(UInt16 r, UInt16 g, UInt16 b) {
  return (r > 127 && g > 127 && b > 127) ? 0 : 1;
}

static UInt8 rgbToGray2(UInt16 r, UInt16 g, UInt16 b) {
  UInt16 c = (r + g + b) / 3;
  return 3 - (c >> 6);
}

static UInt8 rgbToGray4(UInt16 r, UInt16 g, UInt16 b) {
  UInt16 c = (r + g + b) / 3;
  return 15 - (c >> 4);
}

static UInt32 BmpConvertFrom8Bits(UInt32 b, ColorTableType *srcColorTable, Boolean isSrcDefault, UInt8 depth, ColorTableType *dstColorTable, Boolean isDstDefault) {
  switch (depth) {
    case  1: b = rgbToGray1(srcColorTable->entry[b].r, srcColorTable->entry[b].g, srcColorTable->entry[b].b); break;
    case  2: b = rgbToGray2(srcColorTable->entry[b].r, srcColorTable->entry[b].g, srcColorTable->entry[b].b); break;
    case  4: b = rgbToGray4(srcColorTable->entry[b].r, srcColorTable->entry[b].g, srcColorTable->entry[b].b); break;
    case  8: b = BmpRGBToIndex(srcColorTable->entry[b].r, srcColorTable->entry[b].g, srcColorTable->entry[b].b, dstColorTable); break;
    case 16: b = rgb565(srcColorTable->entry[b].r, srcColorTable->entry[b].g, srcColorTable->entry[b].b); break;
    case 24: b = rgb24(srcColorTable->entry[b].r, srcColorTable->entry[b].g, srcColorTable->entry[b].b); break;
    case 32: b = rgb32(srcColorTable->entry[b].r, srcColorTable->entry[b].g, srcColorTable->entry[b].b); break;
  }

  return b;
}

static UInt32 BmpConvertFrom16Bits(UInt32 b, UInt8 depth, ColorTableType *dstColorTable) {
  switch (depth) {
    case  1: b = rgbToGray1(r565(b), g565(b), b565(b)); break;
    case  2: b = rgbToGray2(r565(b), g565(b), b565(b)); break;
    case  4: b = rgbToGray4(r565(b), g565(b), b565(b)); break;
    case  8: b = BmpRGBToIndex(r565(b), g565(b), b565(b), dstColorTable); break;
    case 24: b = rgb24(r565(b), g565(b), b565(b)); break;
    case 32: b = rgb32(r565(b), g565(b), b565(b)); break;
  }

  return b;
}

UInt32 BmpConvertFrom24Bits(UInt32 b, UInt8 depth, ColorTableType *dstColorTable) {
  switch (depth) {
    case  1: b = rgbToGray1(r24(b), g24(b), b24(b)); break;
    case  2: b = rgbToGray2(r24(b), g24(b), b24(b)); break;
    case  4: b = rgbToGray4(r24(b), g24(b), b24(b)); break;
    case  8: b = BmpRGBToIndex(r24(b), g24(b), b24(b), dstColorTable); break;
    case 16: b = rgb565(r24(b), g24(b), b24(b)); break;
    case 32: b = rgb32(r24(b), g24(b), b24(b)); break;
  }

  return b;
}

static UInt32 BmpConvertFrom32Bits(UInt32 b, UInt8 depth, ColorTableType *dstColorTable) {
  switch (depth) {
    case  1: b = rgbToGray1(r32(b), g32(b), b32(b)); break;
    case  2: b = rgbToGray2(r32(b), g32(b), b32(b)); break;
    case  4: b = rgbToGray4(r32(b), g32(b), b32(b)); break;
    case  8: b = BmpRGBToIndex(r32(b), g32(b), b32(b), dstColorTable); break;
    case 16: b = rgb565(r32(b), g32(b), b32(b)); break;
    case 24: b = rgb24(r32(b), g32(b), b32(b)); break;
  }

  return b;
}

#define BmpSetBit1p(offset, mask, dataSize, b) \
  if (offset < dataSize) { \
    bits[offset] &= ~(mask); \
    bits[offset] |= (b); \
  }

#define BmpSetBit1(offset, mask, dataSize, b, dbl) \
  BmpSetBit1p(offset, mask, dataSize, b); \
  if (dbl) { \
    BmpSetBit1p(offset, mask>>1, dataSize, b>>1); \
    BmpSetBit1p(offset+dst->rowBytes, mask, dataSize, b); \
    BmpSetBit1p(offset+dst->rowBytes, mask>>1, dataSize, b>>1); \
  }

static void BmpCopyBit1(UInt8 b, Boolean transp, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  UInt8 *bits, mask, old, fg, bg;
  UInt32 offset, shift, dataSize;
  RGBColorType rgb;

  BmpGetSizes(dst, &dataSize, NULL);
  bits = BmpGetBits(dst);
  offset = dy * dst->rowBytes + (dx >> 3);
  shift = 7 - (dx & 0x07);
  b = b << shift;
  mask = (1 << shift);

  switch (mode) {
    case winPaint:        // write color-matched source pixels to the destination
                          // If a bitmap’s hasTransparency flag is set, winPaint behaves like winOverlay instead.
      if (!transp) {
        BmpSetBit1(offset, mask, dataSize, b, dbl);
      }
      break;
    case winErase:        // write backColor if the source pixel is transparent
      if (transp) {
        WinSetBackColorRGB(NULL, &rgb);
        b = ((rgb.r == 0 && rgb.g == 0 && rgb.b == 0) ? 0 : 1) << shift;
        BmpSetBit1(offset, mask, dataSize, b, dbl);
      }
      break;
    case winMask:         // write backColor if the source pixel is not transparent
      if (!transp) {
        WinSetBackColorRGB(NULL, &rgb);
        b = ((rgb.r == 0 && rgb.g == 0 && rgb.b == 0) ? 0 : 1) << shift;
        BmpSetBit1(offset, mask, dataSize, b, dbl);
      }
      break;
    case winInvert:       // bitwise XOR the color-matched source pixel onto the destination (this mode does not honor the transparent color in any way)
      old = bits[offset] & mask;
      BmpSetBit1(offset, mask, dataSize, (old ^ b), dbl);
      break;
    case winOverlay:      // write color-matched source pixel to the destination if the source pixel is not transparent. Transparent pixels are skipped.
                          // For a 1-bit display, the "off" bits are considered to be the transparent color
      if (!transp) {
        BmpSetBit1(offset, mask, dataSize, b, dbl);
      }
      break;
    case winPaintInverse: // invert the source pixel color and then proceed as with winPaint
      BmpSetBit1(offset, mask, dataSize, b ^ mask, dbl);
      break;
    case winSwap:         // Swap the backColor and foreColor destination colors if the source is a pattern (the type of pattern is disregarded).
                          // If the source is a bitmap, then the bitmap is transferred using winPaint mode instead.
      b = bits[offset] & mask ? 1 : 0;
      bg = WinGetBackColor() ? 1 : 0;
      fg = WinGetForeColor()? 1 : 0;
      if (b == bg) {
        BmpSetBit1(offset, mask, dataSize, fg << shift, dbl);
      } else if (b == fg) {
        BmpSetBit1(offset, mask, dataSize, bg << shift, dbl);
      }
      break;
  }
}

#define BmpSetBit2p(offset, mask, dataSize, b) \
  if (offset < dataSize) { \
    bits[offset] &= ~(mask); \
    bits[offset] |= (b); \
  }

#define BmpSetBit2(offset, mask, dataSize, b, dbl) \
  BmpSetBit2p(offset, mask, dataSize, b); \
  if (dbl) { \
    BmpSetBit2p(offset, mask>>2, dataSize, b>>2); \
    BmpSetBit2p(offset+dst->rowBytes, mask, dataSize, b); \
    BmpSetBit2p(offset+dst->rowBytes, mask>>2, dataSize, b>>2); \
  }

static void BmpCopyBit2(UInt8 b, Boolean transp, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  UInt8 *bits, mask, old, fg, bg;
  UInt32 offset, shift, dataSize;

  BmpGetSizes(dst, &dataSize, NULL);
  bits = BmpGetBits(dst);
  offset = dy * dst->rowBytes + (dx >> 2);
  shift = (3 - (dx & 0x03)) << 1;
  b = b << shift;
  mask = (0x03 << shift);

  switch (mode) {
    case winPaint:        // write color-matched source pixels to the destination
                          // If a bitmap’s hasTransparency flag is set, winPaint behaves like winOverlay instead.
      if (!transp) {
        BmpSetBit2(offset, mask, dataSize, b, dbl);
      }
      break;
    case winErase:        // write backColor if the source pixel is transparent
      if (transp) {
        BmpSetBit2(offset, mask, dataSize, 0x03, dbl);
      }
      break;
    case winMask:         // write backColor if the source pixel is not transparent
      if (!transp) {
        BmpSetBit2(offset, mask, dataSize, 0x03, dbl);
      }
      break;
    case winInvert:       // bitwise XOR the color-matched source pixel onto the destination (this mode does not honor the transparent color in any way)
      old = bits[offset] & mask;
      BmpSetBit2(offset, mask, dataSize, (old ^ b), dbl);
      break;
    case winOverlay:      // write color-matched source pixel to the destination if the source pixel is not transparent. Transparent pixels are skipped.
      if (!transp) {
        BmpSetBit2(offset, mask, dataSize, b, dbl);
      }
      break;
    case winPaintInverse: // invert the source pixel color and then proceed as with winPaint
      BmpSetBit2(offset, mask, dataSize, b ^ 0x03, dbl);
      break;
    case winSwap:         // Swap the backColor and foreColor destination colors if the source is a pattern (the type of pattern is disregarded).
                          // If the source is a bitmap, then the bitmap is transferred using winPaint mode instead.
      b = (bits[offset] & mask) >> shift;
      bg = WinGetBackColor();
      fg = WinGetForeColor();
      if (b == bg) b = fg;
      else if (b == fg) b = bg;
      BmpSetBit2(offset, mask, dataSize, b << shift, dbl);
      break;
  }
}

#define BmpSetBit4p(offset, mask, dataSize, b) \
  if (offset < dataSize) { \
    bits[offset] &= ~(mask); \
    bits[offset] |= (b); \
  }

#define BmpSetBit4(offset, mask, dataSize, b, dbl) \
  BmpSetBit4p(offset, mask, dataSize, b); \
  if (dbl) { \
    BmpSetBit4p(offset, mask<<4, dataSize, b<<4); \
    BmpSetBit4p(offset+dst->rowBytes, mask, dataSize, b); \
    BmpSetBit4p(offset+dst->rowBytes, mask<<4, dataSize, b<<4); \
  }

static void BmpCopyBit4(UInt8 b, Boolean transp, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  UInt8 *bits, mask, old, fg, bg;
  UInt32 offset, shift, dataSize;

  BmpGetSizes(dst, &dataSize, NULL);
  bits = BmpGetBits(dst);
  offset = dy * dst->rowBytes + (dx >> 1);
  shift = (dx & 0x01) ? 4 : 0;
  b = b << shift;
  mask = (0x0F << shift);

  switch (mode) {
    case winPaint:        // write color-matched source pixels to the destination
                          // If a bitmap’s hasTransparency flag is set, winPaint behaves like winOverlay instead.
      if (!transp) {
        BmpSetBit4(offset, mask, dataSize, b, dbl);
      }
      break;
    case winErase:        // write backColor if the source pixel is transparent
      if (transp) {
        BmpSetBit4(offset, mask, dataSize, 0x0F, dbl);
      }
      break;
    case winMask:         // write backColor if the source pixel is not transparent
      if (!transp) {
        BmpSetBit2(offset, mask, dataSize, 0x0F, dbl);
      }
      break;
    case winInvert:       // bitwise XOR the color-matched source pixel onto the destination (this mode does not honor the transparent color in any way)
      old = bits[offset] & mask;
      BmpSetBit4(offset, mask, dataSize, (old ^ b), dbl);
      break;
    case winOverlay:      // write color-matched source pixel to the destination if the source pixel is not transparent. Transparent pixels are skipped.
      if (!transp) {
        BmpSetBit4(offset, mask, dataSize, b, dbl);
      }
      break;
    case winPaintInverse: // invert the source pixel color and then proceed as with winPaint
      BmpSetBit4(offset, mask, dataSize, b ^ 0x0f, dbl);
      break;
    case winSwap:         // Swap the backColor and foreColor destination colors if the source is a pattern (the type of pattern is disregarded).
                          // If the source is a bitmap, then the bitmap is transferred using winPaint mode instead.
      b = (bits[offset] & mask) >> shift;
      bg = WinGetBackColor();
      fg = WinGetForeColor();
      if (b == bg) b = fg;
      else if (b == fg) b = bg;
      BmpSetBit4(offset, mask, dataSize, b << shift, dbl);
      break;
  }
}

#define BmpSetBit8p(offset, dataSize, b) if (offset < dataSize) bits[offset] = b

#define BmpSetBit8(offset, dataSize, b, dbl) \
  BmpSetBit8p(offset, dataSize, b); \
  if (dbl) { \
    BmpSetBit8p(offset+1, dataSize, b); \
    BmpSetBit8p(offset+dst->rowBytes, dataSize, b); \
    BmpSetBit8p(offset+dst->rowBytes+1, dataSize, b); \
  }

static void BmpCopyBit8(UInt8 b, Boolean transp, BitmapType *dst, ColorTableType *colorTable, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  UInt8 *bits, old, r1, g1, b1, r2, g2, b2, fg, bg;
  UInt32 offset, dataSize;

  BmpGetSizes(dst, &dataSize, NULL);
  bits = BmpGetBits(dst);
  offset = dy * dst->rowBytes + dx;

  switch (mode) {
    case winPaint:        // write color-matched source pixels to the destination
                          // If a bitmap’s hasTransparency flag is set, winPaint behaves like winOverlay instead.
      if (!transp) {
        BmpSetBit8(offset, dataSize, b, dbl);
      }
      break;
    case winErase:        // write backColor if the source pixel is transparent
      if (transp) {
        bg = WinGetBackColor();
        BmpSetBit8(offset, dataSize, bg, dbl);
      }
      break;
    case winMask:         // write backColor if the source pixel is not transparent
      if (!transp) {
        bg = WinGetBackColor();
        BmpSetBit8(offset, dataSize, bg, dbl);
      }
      break;
    case winInvert:       // bitwise XOR the color-matched source pixel onto the destination (this mode does not honor the transparent color in any way)
      old = bits[offset];
      BmpIndexToRGB(old, &r1, &g1, &b1, colorTable);
      BmpIndexToRGB(b, &r2, &g2, &b2, colorTable);
      r1 ^= r2;
      g1 ^= g2;
      b1 ^= b2;
      b = BmpRGBToIndex(r1, g1, b1, colorTable);
      BmpSetBit8(offset, dataSize, b, dbl);
      break;
    case winOverlay:      // write color-matched source pixel to the destination if the source pixel is not transparent. Transparent pixels are skipped.
      if (!transp) {
        BmpSetBit8(offset, dataSize, b, dbl);
      }
      break;
    case winPaintInverse: // invert the source pixel color and then proceed as with winPaint
      if (!transp) {
        BmpIndexToRGB(b, &r1, &g1, &b1, colorTable);
        r1 ^= 0xff;
        g1 ^= 0xff;
        b1 ^= 0xff;
        b = BmpRGBToIndex(r1, g1, b1, colorTable);
        BmpSetBit8(offset, dataSize, b, dbl);
      }
      break;
    case winSwap:         // Swap the backColor and foreColor destination colors if the source is a pattern (the type of pattern is disregarded).
                          // If the source is a bitmap, then the bitmap is transferred using winPaint mode instead.
      b = bits[offset];
      bg = WinGetBackColor();
      fg = WinGetForeColor();
      if (b == bg) b = fg;
      else if (b == fg) b = bg;
      BmpSetBit8(offset, dataSize, b, dbl);
      break;
  }
}

#define BmpSetBit16p(offset, dataSize, b) if (offset+1 < dataSize) put2b(b, bits, offset)

#define BmpSetBit16(offset, dataSize, b, dbl) \
  BmpSetBit16p(offset, dataSize, b); \
  if (dbl) { \
    BmpSetBit16p(offset+2, dataSize, b); \
    BmpSetBit16p(offset+dst->rowBytes, dataSize, b); \
    BmpSetBit16p(offset+dst->rowBytes+2, dataSize, b); \
  }

static void BmpCopyBit16(UInt16 b, Boolean transp, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  RGBColorType rgb, aux;
  UInt8 *bits;
  UInt16 old, fg, bg;
  UInt32 offset, dataSize;

  BmpGetSizes(dst, &dataSize, NULL);
  bits = BmpGetBits(dst);
  offset = dy * dst->rowBytes + dx*2;

  switch (mode) {
    case winPaint:        // write color-matched source pixels to the destination
      if (!transp) {
        BmpSetBit16(offset, dataSize, b, dbl);
      }
      break;
    case winErase:        // write backColor if the source pixel is transparent
      if (transp) {
        WinSetBackColorRGB(NULL, &rgb);
        BmpSetBit16(offset, dataSize, rgb565(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winMask:         // write backColor if the source pixel is not transparent
      if (!transp) {
        WinSetBackColorRGB(NULL, &rgb);
        BmpSetBit16(offset, dataSize, rgb565(rgb.r, rgb.g, rgb.b), dbl);
      }
/*
      if (b != 0xffff) {
        BmpSetBit16(offset, dataSize, b, dbl);
      }
*/
      break;
    case winInvert:       // bitwise XOR the color-matched source pixel onto the destination (this mode does not honor the transparent color in any way)
      get2b(&old, bits, offset);
      rgb.r = r565(b);
      rgb.g = g565(b);
      rgb.b = b565(b);
      rgb.r ^= r565(old);
      rgb.g ^= g565(old);
      rgb.b ^= b565(old);
      b = rgb565(rgb.r, rgb.g, rgb.b);
      BmpSetBit16(offset, dataSize, b, dbl);
      break;
    case winOverlay:      // write color-matched source pixel to the destination if the source pixel is not transparent. Transparent pixels are skipped.
      if (!transp) {
        BmpSetBit16(offset, dataSize, b, dbl);
      }
      break;
    case winPaintInverse: // invert the source pixel color and then proceed as with winPaint
      if (!transp) {
        rgb.r = r565(b);
        rgb.g = g565(b);
        rgb.b = b565(b);
        rgb.r ^= 0xff;
        rgb.g ^= 0xff;
        rgb.b ^= 0xff;
        BmpSetBit16(offset, dataSize, rgb565(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winSwap:         // Swap the backColor and foreColor destination colors if the source is a pattern (the type of pattern is disregarded).
                          // If the source is a bitmap, then the bitmap is transferred using winPaint mode instead.
      get2b(&old, bits, offset);
      WinSetBackColorRGB(NULL, &rgb);
      WinSetForeColorRGB(NULL, &aux);
      bg = rgb565(rgb.r, rgb.g, rgb.b);
      fg = rgb565(aux.r, aux.g, aux.b);
      if (old == bg) {
        BmpSetBit16(offset, dataSize, fg, dbl);
      } else if (old == fg) {
        BmpSetBit16(offset, dataSize, bg, dbl);
      }
      break;
    default:
      break;
  }
}

#define BmpSetBit24p(offset, dataSize, b) if (offset+2 < dataSize) { \
    bits[offset] = r24(b); \
    bits[offset+1] = g24(b); \
    bits[offset+2] = b24(b); \
  }

#define BmpSetBit24(offset, dataSize, b, dbl) \
  BmpSetBit24p(offset, dataSize, b); \
  if (dbl) { \
    BmpSetBit24p(offset+3, dataSize, b); \
    BmpSetBit24p(offset+dst->rowBytes, dataSize, b); \
    BmpSetBit24p(offset+dst->rowBytes+3, dataSize, b); \
  }

static void BmpCopyBit24(UInt32 b, Boolean transp, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  RGBColorType rgb, aux;
  UInt8 *bits;
  UInt32 old, fg, bg;
  UInt32 offset, dataSize;

  BmpGetSizes(dst, &dataSize, NULL);
  bits = BmpGetBits(dst);
  offset = dy * dst->rowBytes + dx*3;

  switch (mode) {
    case winPaint:        // write color-matched source pixels to the destination
      if (!transp) {
        BmpSetBit24(offset, dataSize, b, dbl);
      }
      break;
    case winErase:        // write backColor if the source pixel is transparent
      if (transp) {
        WinSetBackColorRGB(NULL, &rgb);
        BmpSetBit24(offset, dataSize, rgb24(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winMask:         // write backColor if the source pixel is not transparent
      if (!transp) {
        WinSetBackColorRGB(NULL, &rgb);
        BmpSetBit24(offset, dataSize, rgb24(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winInvert:       // bitwise XOR the color-matched source pixel onto the destination (this mode does not honor the transparent color in any way)
      rgb.r = r24(b);
      rgb.g = g24(b);
      rgb.b = b24(b);
      rgb.r ^= bits[offset];
      rgb.g ^= bits[offset+1];
      rgb.b ^= bits[offset+1];
      b = rgb24(rgb.r, rgb.g, rgb.b);
      BmpSetBit24(offset, dataSize, b, dbl);
      break;
    case winOverlay:      // write color-matched source pixel to the destination if the source pixel is not transparent. Transparent pixels are skipped.
      if (!transp) {
        BmpSetBit24(offset, dataSize, b, dbl);
      }
      break;
    case winPaintInverse: // invert the source pixel color and then proceed as with winPaint
      if (!transp) {
        rgb.r = r24(b);
        rgb.g = g24(b);
        rgb.b = b24(b);
        rgb.r ^= 0xff;
        rgb.g ^= 0xff;
        rgb.b ^= 0xff;
        BmpSetBit24(offset, dataSize, rgb24(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winSwap:         // Swap the backColor and foreColor destination colors if the source is a pattern (the type of pattern is disregarded).
                          // If the source is a bitmap, then the bitmap is transferred using winPaint mode instead.
      old = rgb24(bits[offset], bits[offset+1], bits[offset+2]);
      WinSetBackColorRGB(NULL, &rgb);
      WinSetForeColorRGB(NULL, &aux);
      bg = rgb24(rgb.r, rgb.g, rgb.b);
      fg = rgb24(aux.r, aux.g, aux.b);
      if (old == bg) {
        BmpSetBit24(offset, dataSize, fg, dbl);
      } else if (old == fg) {
        BmpSetBit24(offset, dataSize, bg, dbl);
      }
      break;
    default:
      break;
  }
}

#define BmpSetBit32p(offset, dataSize, b) if (offset+3 < dataSize) put4l(b, bits, offset)

#define BmpSetBit32(offset, dataSize, b, dbl) \
  BmpSetBit32p(offset, dataSize, b); \
  if (dbl) { \
    BmpSetBit32p(offset+4, dataSize, b); \
    BmpSetBit32p(offset+dst->rowBytes, dataSize, b); \
    BmpSetBit32p(offset+dst->rowBytes+4, dataSize, b); \
  }

static void BmpCopyBit32(UInt32 b, Boolean transp, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  RGBColorType rgb, aux;
  UInt8 *bits;
  UInt32 old, fg, bg;
  UInt32 offset, dataSize;

  BmpGetSizes(dst, &dataSize, NULL);
  bits = BmpGetBits(dst);
  offset = dy * dst->rowBytes + dx*4;

  switch (mode) {
    case winPaint:        // write color-matched source pixels to the destination
      if (!transp) {
        BmpSetBit32(offset, dataSize, b, dbl);
      }
      break;
    case winErase:        // write backColor if the source pixel is transparent
      if (transp) {
        WinSetBackColorRGB(NULL, &rgb);
        BmpSetBit32(offset, dataSize, rgb32(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winMask:         // write backColor if the source pixel is not transparent
      if (!transp) {
        WinSetBackColorRGB(NULL, &rgb);
        BmpSetBit32(offset, dataSize, rgb32(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winInvert:       // bitwise XOR the color-matched source pixel onto the destination (this mode does not honor the transparent color in any way)
      rgb.r = r32(b);
      rgb.g = g32(b);
      rgb.b = b32(b);
      // little-endian
      rgb.r ^= bits[offset+2];
      rgb.g ^= bits[offset+1];
      rgb.b ^= bits[offset];
      b = rgb32(rgb.r, rgb.g, rgb.b);
      BmpSetBit32(offset, dataSize, b, dbl);
      break;
    case winOverlay:      // write color-matched source pixel to the destination if the source pixel is not transparent. Transparent pixels are skipped.
      if (!transp) {
        BmpSetBit32(offset, dataSize, b, dbl);
      }
      break;
    case winPaintInverse: // invert the source pixel color and then proceed as with winPaint
      if (!transp) {
        rgb.r = r32(b);
        rgb.g = g32(b);
        rgb.b = b32(b);
        rgb.r ^= 0xff;
        rgb.g ^= 0xff;
        rgb.b ^= 0xff;
        BmpSetBit32(offset, dataSize, rgb24(rgb.r, rgb.g, rgb.b), dbl);
      }
      break;
    case winSwap:         // Swap the backColor and foreColor destination colors if the source is a pattern (the type of pattern is disregarded).
                          // If the source is a bitmap, then the bitmap is transferred using winPaint mode instead.
      old = rgb32(bits[offset], bits[offset+1], bits[offset+2]);
      WinSetBackColorRGB(NULL, &rgb);
      WinSetForeColorRGB(NULL, &aux);
      bg = rgb32(rgb.r, rgb.g, rgb.b);
      fg = rgb32(aux.r, aux.g, aux.b);
      if (old == bg) {
        BmpSetBit32(offset, dataSize, fg, dbl);
      } else if (old == fg) {
        BmpSetBit32(offset, dataSize, bg, dbl);
      }
      break;
    default:
      break;
  }
}

void BmpPutBit(UInt32 b, Boolean transp, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl) {
  ColorTableType *dstColorTable, *colorTable;
  UInt8 dstDepth;

  if (dst && dx >= 0 && dx < dst->width && dy >= 0 && dy < dst->height) {
    colorTable = pumpkin_defaultcolorTable();
    dstColorTable = BmpGetColortable(dst);
    if (dstColorTable == NULL) {
      dstColorTable = colorTable;
    }

    dstDepth = BmpGetBitDepth(dst);

    switch (dstDepth) {
        case 1:
          BmpCopyBit1(b, transp, dst, dx, dy, mode, dbl);
          break;
        case 2:
          BmpCopyBit2(b, transp, dst, dx, dy, mode, dbl);
          break;
        case 4:
          BmpCopyBit4(b, transp, dst, dx, dy, mode, dbl);
          break;
        case 8:
          BmpCopyBit8(b, transp, dst, dstColorTable, dx, dy, mode, dbl);
          break;
        case 16:
          BmpCopyBit16(b, transp, dst, dx, dy, mode, dbl);
          break;
        case 24:
          BmpCopyBit24(b, transp, dst, dx, dy, mode, dbl);
          break;
        case 32:
          BmpCopyBit32(b, transp, dst, dx, dy, mode, dbl);
          break;
    }
    dbg_update(dst);
  }
}

void BmpSetPixel(BitmapType *bmp, Coord x, Coord y, UInt32 value) {
  BmpPutBit(value, false, bmp, x, y, winPaint, false);
}

void BmpCopyBit(BitmapType *src, Coord sx, Coord sy, BitmapType *dst, Coord dx, Coord dy, WinDrawOperation mode, Boolean dbl, Boolean text, UInt32 tc, UInt32 bc) {
  ColorTableType *srcColorTable, *dstColorTable, *colorTable;
  UInt8 srcDepth, dstDepth, *bits;
  UInt32 srcPixel, dstPixel, offset;
  UInt32 srcTransparentValue;
  UInt16 aux;
  Boolean srcTransp, isSrcDefault, isDstDefault;

  if (src && dst && sx >= 0 && sx < src->width && sy >= 0 && sy < src->height && dx >= 0 && dx < dst->width && dy >= 0 && dy < dst->height) {
    colorTable = pumpkin_defaultcolorTable();

    srcColorTable = BmpGetColortable(src);
    if (srcColorTable == NULL) {
      srcColorTable = colorTable;
    }
    isSrcDefault = srcColorTable == colorTable;

    dstColorTable = BmpGetColortable(dst);
    if (dstColorTable == NULL) {
      dstColorTable = colorTable;
    }
    isDstDefault = dstColorTable == colorTable;

    srcTransp = BmpGetTransparentValue(src, &srcTransparentValue);
    srcDepth = BmpGetBitDepth(src);
    dstDepth = BmpGetBitDepth(dst);
    bits = BmpGetBits(src);

    switch (srcDepth) {
      case 1:
        srcPixel = bits[sy * src->rowBytes + (sx >> 3)];
        srcPixel = (srcPixel >> (7 - (sx & 0x07))) & 1;
        dstPixel = (dstDepth == 1) ? srcPixel : BmpConvertFrom1Bit(srcPixel, dstDepth, dstColorTable, isDstDefault);
        break;
      case 2:
        srcPixel = bits[sy * src->rowBytes + (sx >> 2)];
        srcPixel = (srcPixel >> ((3 - (sx & 0x03)) << 1)) & 0x03;
        dstPixel = (dstDepth == 2) ? srcPixel : BmpConvertFrom2Bits(srcPixel, dstDepth, dstColorTable, isDstDefault);
        break;
      case 4:
        srcPixel = bits[sy * src->rowBytes + (sx >> 1)];
        srcPixel = (sx & 0x01) ? srcPixel >> 4 : srcPixel & 0x0F;
        dstPixel = (dstDepth == 4) ? srcPixel : BmpConvertFrom4Bits(srcPixel, dstDepth, dstColorTable, isDstDefault);
        break;
      case 8:
        srcPixel = bits[sy * src->rowBytes + sx];
        if (dstDepth != 8 || !isSrcDefault || !isDstDefault) {
          dstPixel = BmpConvertFrom8Bits(srcPixel, srcColorTable, isSrcDefault, dstDepth, dstColorTable, isDstDefault);
        } else {
          dstPixel = srcPixel;
        }
        break;
      case 16:
        get2b(&aux, bits, sy * src->rowBytes + sx*2);
        srcPixel = aux;
        dstPixel = (dstDepth == 16) ? srcPixel : BmpConvertFrom16Bits(srcPixel, dstDepth, dstColorTable);
        break;
      case 24:
        offset = sy * src->rowBytes + sx*3;
        srcPixel = rgb24(bits[offset], bits[offset+1], bits[offset+2]);
        dstPixel = (dstDepth == 24) ? srcPixel : BmpConvertFrom24Bits(srcPixel, dstDepth, dstColorTable);
        break;
      case 32:
        get4l(&srcPixel, bits, sy * src->rowBytes + sx*4);
        dstPixel = (dstDepth == 32) ? srcPixel : BmpConvertFrom32Bits(srcPixel, dstDepth, dstColorTable);
        break;
    }

    if (srcTransp) {
      srcTransp = (srcPixel == srcTransparentValue);
    } else if (mode == winMask || mode == winOverlay) {
      // source bitmap is not transparent but mode is winMask or winOverlay
      // assume the transparent color is white
      // I am not sure this is correct, but transparency in SimCity only works with this hack
      switch (dstDepth) {
        case  8: srcTransp = (srcPixel == 0x00); break;
        case 16: srcTransp = (srcPixel == 0xffff); break;
        case 24: srcTransp = (srcPixel == 0xffffff); break;
        case 32: srcTransp = (srcPixel == 0xffffff); break;
      }
    }

    if (text) dstPixel = srcPixel ? tc : bc;
    if (text && mode == winPaint) srcTransp = false;
    bits = BmpGetBits(dst);

    switch (dstDepth) {
      case 1:
        BmpCopyBit1(dstPixel, srcTransp, dst, dx, dy, mode, dbl);
        break;
      case 2:
        BmpCopyBit2(dstPixel, srcTransp, dst, dx, dy, mode, dbl);
        break;
      case 4:
        BmpCopyBit4(dstPixel, srcTransp, dst, dx, dy, mode, dbl);
        break;
      case 8:
        BmpCopyBit8(dstPixel, srcTransp, dst, dstColorTable, dx, dy, mode, dbl);
        break;
      case 16:
        BmpCopyBit16(dstPixel, srcTransp, dst, dx, dy, mode, dbl);
        break;
      case 24:
        BmpCopyBit24(dstPixel, srcTransp, dst, dx, dy, mode, dbl);
        break;
      case 32:
        BmpCopyBit32(dstPixel, srcTransp, dst, dx, dy, mode, dbl);
        break;
    }
    dbg_update(dst);
  } else {
//debug(1, "XXX", "BmpCopyBit out");
  }
}

/*
Compression:
BitmapCompressionTypeScanLine : Use scan line compression. Scan line compression is compatible with Palm OS 2.0 and higher.
BitmapCompressionTypeRLE      : Use RLE compression. RLE compression is supported in Palm OS 3.5 only.

Exemplo de bitmap V2 com BitmapCompressionTypeRLE:
00 13 00 10 00 14 a0 00 08 02 00 00 00 01 00 00

00 9e : number of bytes following

row 0:
05 00
04 5f
0b 00

row 1:
05 00
04 5f
0b 00

row 2:
05 00
04 6b
0b 00

row 3:
02 00
03 6b
04 5f
06 6b
05 00

02 1b
0e 5f
04 00

02 1b
02 5f
01 6b
01 5b
01 6b
01 5b
04 6b
01 5b
04 5f
03 00

02 1b
02 5f
09 6b
02 5b
03 5f
02 00

02 1b
02 5f
0b 6b
01 5b
03 5f
01 00

02 1b
02 5f
0b 6b
01 5b
03 5f
01 00

02 1b
02 5f
09 6b
02 5b
03 5f
02 00

02 1b
02 5f
01 6b
01 5b
01 6b
01 5b
04 6b
01 5b
04 5f
03 00

02 1b
0e 5f
04 00

02 00
03 6b
04 5f
06 6b
05 00

05 00
04 6b
0b 00

05 00
04 5f
0b 00

05 00
04 5f
0b 00

00 00 : end marker
*/

static int decompress_bitmap_rle(uint8_t *p, uint8_t *dp, uint16_t dsize) {
  uint8_t len, b;
  int i, j, k;

  debug(DEBUG_TRACE, "Bitmap", "RLE bitmap decompressing");

  for (i = 0, j = 0;;) {
    i += get1(&len, p, i);
    i += get1(&b, p, i);

    for (k = 0; k < len && j < dsize; k++) {
      dp[j++] = b;
    }

    if (j == dsize) {
      if (k == len) {
        debug(DEBUG_TRACE, "Bitmap", "RLE bitmap decompressed %d bytes into %d bytes", i, dsize);
        break;
      }
      debug(DEBUG_ERROR, "Bitmap", "RLE bitmap decompression error 3 (%d < %d)", k, len);
      return -1;
    }
  }

  return 0;
}

static int decompress_bitmap_packbits8(uint8_t *p, uint8_t *dp, uint32_t dsize) {
  uint8_t len, b;
  int8_t count;
  int i, j, k;

  debug(DEBUG_TRACE, "Bitmap", "PackBits8 bitmap decompressing");

  for (i = 0, j = 0;;) {
    i += get1((uint8_t *)&count, p, i);
    debug(DEBUG_TRACE, "Bitmap", "PackBits8 index %d code %d", i-1, count);

    if (count >= -127 && count <= -1) {
      // encoded run packet
      len = (uint8_t)(-count) + 1;
      i += get1(&b, p, i);
      debug(DEBUG_TRACE, "Bitmap", "PackBits8 %d bytes encoded run packet 0x%02X", len, b);

      for (k = 0; k < len && j < dsize; k++) {
        dp[j++] = b;
      }

    } else if (count >= 0 && count <= 127) {
      // literal run packet
      len = (uint8_t)count + 1;
      debug(DEBUG_TRACE, "Bitmap", "PackBits8 %d bytes literal run packet", len);

      for (k = 0; k < len && j < dsize; k++) {
        i += get1(&b, p, i);
        dp[j++] = b;
      }

    } else {
      debug(DEBUG_TRACE, "Bitmap", "PackBits8 code -128 ignored");
    }

    if (j == dsize) {
      debug(DEBUG_TRACE, "Bitmap", "PackBits8 bitmap decompressed %d bytes into %d bytes", i, dsize);
      break;
    }
  }

  return i;
}

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static int decompress_bitmap_scanline(uint8_t *p, uint8_t *dp, uint16_t rowBytes, uint16_t width, uint16_t height) {
  uint32_t i, j, k, row, byteCount;
  uint16_t dsize;
  uint8_t diffmask, inval;

  dsize = rowBytes * height;
  debug(DEBUG_TRACE, "Bitmap", "scanline bitmap decompressing %d bytes", dsize);
  i = 0;

  for (row = 0; row < height; row++) {
    for (j = 0; j < rowBytes; j += 8) {
      i += get1(&diffmask, p, i);
      byteCount = MIN(rowBytes - j, 8);

      for (k = 0; k < byteCount; k++) {
        if (row == 0 || ((diffmask & (1 << (7 - k))) != 0)) {
          i += get1(&inval, p, i);
          dp[row * rowBytes + j + k] = inval;
        } else {
          dp[row * rowBytes + j + k] = dp[(row - 1) * rowBytes + j + k];
        }
      }
    }
  }

  return 0;
}

#define get2(a,p,i) le ? get2l(a, p, i) : get2b(a, p, i)
#define get4(a,p,i) le ? get4l(a, p, i) : get4b(a, p, i)

BitmapType *pumpkin_create_bitmap(void *h, uint8_t *p, uint32_t size, uint32_t type, int chain, uint32_t *bsize) {
  BitmapType *bmp;
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  uint32_t dummy32, transparentValue, nextBitmapOffset, dsize, compressedSize32;
  uint16_t dummy16, width, height, rowBytes, compressedSize;
  uint16_t nextDepthOffset, density;
  uint8_t dummy8, pixelSize, version, ssize;
  uint8_t transparentIndex, compressionType, pixelFormat;
  uint8_t *dp, r, g, b;
  uint16_t attr;
  BitmapFlagsType bmpAttr;
  char st[8];
  int i, j, invalid;
  int le;

  le = (type == 'abmp');
  pumpkin_id2s(type, st);
  debug(DEBUG_TRACE, "Bitmap", "pumpkin_create_bitmap begin");

  // szRCBITMAP "w,w,w,uuuuuuuuzu8,b,b,w,b,b,zw"
  i = 0;
  i += get2(&width, p, i);
  i += get2(&height, p, i);
  i += get2(&rowBytes, p, i);
  i += get2(&attr, p, i);
  i += get1(&pixelSize, p, i);
  i += get1(&version, p, i);
  if (le) version &= 0x7F;

  debug_bytes(DEBUG_TRACE, "Bitmap", p, 32);

  if (width == 0 && height == 0 && rowBytes == 0 && attr == 0 && pixelSize == 0xff && version == 1) {
    // 00 00 00 00 00 00 00 00 ff 01 00 00 00 00 00 00
    debug(DEBUG_TRACE, "Bitmap", "bitmap skip empty density slot");
    p = &p[16];
    i = 0;
    i += get2(&width, p, i);
    i += get2(&height, p, i);
    i += get2(&rowBytes, p, i);
    i += get2(&attr, p, i);
    i += get1(&pixelSize, p, i);
    i += get1(&version, p, i);
    if (le) version &= 0x7F;
    debug_bytes(DEBUG_TRACE, "Bitmap", p, 32);
  }

  if (le) {
    bmpAttr.compressed         = (attr & 0x0001) ? 1 : 0;
    bmpAttr.hasColorTable      = (attr & 0x0002) ? 1 : 0;
    bmpAttr.hasTransparency    = (attr & 0x0004) ? 1 : 0;
    bmpAttr.indirect           = (attr & 0x0008) ? 1 : 0;
    bmpAttr.forScreen          = (attr & 0x0010) ? 1 : 0;
    bmpAttr.directColor        = (attr & 0x0020) ? 1 : 0;
    bmpAttr.indirectColorTable = (attr & 0x0040) ? 1 : 0;
    bmpAttr.noDither           = (attr & 0x0080) ? 1 : 0;
    bmpAttr.reserved           = 0;
  } else {
    bmpAttr.compressed         = (attr & 0x8000) ? 1 : 0;
    bmpAttr.hasColorTable      = (attr & 0x4000) ? 1 : 0;
    bmpAttr.hasTransparency    = (attr & 0x2000) ? 1 : 0;
    bmpAttr.indirect           = (attr & 0x1000) ? 1 : 0;
    bmpAttr.forScreen          = (attr & 0x0800) ? 1 : 0;
    bmpAttr.directColor        = (attr & 0x0400) ? 1 : 0;
    bmpAttr.indirectColorTable = (attr & 0x0200) ? 1 : 0;
    bmpAttr.noDither           = (attr & 0x0100) ? 1 : 0;
    bmpAttr.reserved           = 0;
  }

  if (type != iconType && type != bitmapRsc && type != 'abmp') {
    // XXX FreeJongg stores bitmaps as tRAW, but in general not all tRAW resources are bitmaps...
    // Trying my best to detect if a tRAW resource is really a bitmap
    switch (pixelSize) {
      case 1:
      case 2:
      case 4:
        invalid = 0;
        break;
      case 8:
        invalid = (rowBytes < width);
        break;
      case 16:
        invalid = (rowBytes < width*2);
        break;
      case 24:
        invalid = (rowBytes < width*3);
        break;
      case 32:
        invalid = (rowBytes < width*4);
        break;
      default:
        invalid = 1;
        break;
    }
    if (invalid || version > 3 || width == 0 || height == 0 || (attr & 0xff) != 0x00) {
      debug(DEBUG_INFO, "Bitmap", "resource type %s size %d is probably not a bitmap", st, size);
      debug_bytes(DEBUG_INFO, "Bitmap", p, 32);
      return NULL;
    }
  }

  switch (version) {
    case 0: bmp = StoNewDecodedResource(h, sizeof(BitmapTypeV0), bitmapRsc, 1); if (bsize) *bsize = sizeof(BitmapTypeV0); break;
    case 1: bmp = StoNewDecodedResource(h, sizeof(BitmapTypeV1), bitmapRsc, 1); if (bsize) *bsize = sizeof(BitmapTypeV1); break;
    case 2: bmp = StoNewDecodedResource(h, sizeof(BitmapTypeV2), bitmapRsc, 1); if (bsize) *bsize = sizeof(BitmapTypeV2); break;
    case 3: bmp = StoNewDecodedResource(h, sizeof(BitmapTypeV3), bitmapRsc, 1); if (bsize) *bsize = sizeof(BitmapTypeV3); break;
    default:
      debug(DEBUG_ERROR, "Bitmap", "invalid bitmap '%s' (%s) version %d", st, le ? "LE" : "BE", version);
      return NULL;
  }

  if (bmp == NULL) {
    return NULL;
  }

  xmemcpy(bmp->data, p, size < BITMAP_SPACE ? size : BITMAP_SPACE);

  switch (version) {
    case 0:
      bmpV0 = (BitmapTypeV0 *)bmp;
      i += get2(&dummy16, p, i);
      i += get2(&dummy16, p, i);
      i += get2(&dummy16, p, i);
      bmpV0->width = width;
      bmpV0->height = height;
      bmpV0->rowBytes = rowBytes;
      bmpV0->flags = bmpAttr;
      bmpV0->bitsSize = height * rowBytes;
      debug(DEBUG_TRACE, "Bitmap", "bitmap V0 size %dx%d, rowBytes %d", width, height, rowBytes);
      if (width > 0 && height > 0) {
        if (bmpAttr.compressed) {
          i += get2(&compressedSize, p, i);
          compressedSize -= 2;
          debug(DEBUG_TRACE, "Bitmap", "bitmap V0 compressed");
          if ((dp = pumpkin_heap_alloc(rowBytes * height, "Bits")) != NULL) {
            if (decompress_bitmap_scanline(&p[i], dp, rowBytes, width, height) == 0) {
              bmpV0->flags.compressed = 0;
              bmpV0->bits = dp;
            } else {
              pumpkin_heap_free(dp, "Bits");
            }
          }
          i += compressedSize;
        } else {
          bmpV0->bits = pumpkin_heap_dup(&p[i], rowBytes * height, "Bits");
          i += rowBytes * height;
        }
        if (chain && i < size-10) {
          bmpV0->next = pumpkin_create_bitmap(h, &p[i], size - i, type, 1, NULL);
        }
      }
      break;

    case 1:
      bmpV1 = (BitmapTypeV1 *)bmp;
      i += get2(&nextDepthOffset, p, i);
      i += get2(&dummy16, p, i);
      i += get2(&dummy16, p, i);
      bmpV1->width = width;
      bmpV1->height = height;
      bmpV1->rowBytes = rowBytes;
      bmpV1->flags = bmpAttr;
      bmpV1->pixelSize = pixelSize;
      bmpV1->version = version;
      bmpV1->nextDepthOffset = nextDepthOffset;
      bmpV1->bitsSize = height * rowBytes;
      debug(DEBUG_TRACE, "Bitmap", "bitmap V1 size %dx%d, bpp %d, compressed %d, rowBytes %d, nextDepthOffset %d", width, height, pixelSize, bmpV1->flags.compressed, rowBytes, nextDepthOffset*4);

      if (bmpAttr.compressed) {
        i += 2; // skip compressedSize ?
        if ((dp = pumpkin_heap_alloc(rowBytes * height, "Bits")) != NULL) {
          if (decompress_bitmap_scanline(&p[i], dp, rowBytes, width, height) == 0) {
            bmpV1->flags.compressed = 0;
            bmpV1->bits = dp;
          }
        }
      } else {
        bmpV1->bits = pumpkin_heap_dup(&p[i], rowBytes * height, "Bits");
        i += rowBytes * height;
      }

      if (chain && nextDepthOffset) {
        i = nextDepthOffset*4;
        if (i < size-10) {
          bmpV1->next = pumpkin_create_bitmap(h, &p[i], size - i, type, 1, NULL);
        }
      }
      break;

    case 2:
      bmpV2 = (BitmapTypeV2 *)bmp;
      i += get2(&nextDepthOffset, p, i);
      i += get1(&transparentIndex, p, i);
      i += get1(&compressionType, p, i);
      i += get2(&dummy16, p, i);

      if (bmpAttr.hasColorTable) {
        i += get2(&dummy16, p, i);
        dummy16 &= 0xFF;
        debug(DEBUG_TRACE, "Bitmap", "reading ColorTableType with %d entries", dummy16);

        bmpV2->colorTable = pumpkin_heap_alloc(sizeof(ColorTableType) + dummy16 * sizeof(RGBColorType), "ColorTable");
        bmpV2->colorTable->numEntries = dummy16;
        for (j = 0; j < dummy16; j++) {
          i += get1(&dummy8, p, i);
          i += get1(&dummy8, p, i);
          bmpV2->colorTable->entry[j].r = dummy8;
          i += get1(&dummy8, p, i);
          bmpV2->colorTable->entry[j].g = dummy8;
          i += get1(&dummy8, p, i);
          bmpV2->colorTable->entry[j].b = dummy8;
        }
      }

      if (bmpAttr.directColor) {
        // BitmapDirectInfoType:
        // UInt8         redBits;          // # of red bits in each pixel
        // UInt8         greenBits;        // # of green bits in each pixel
        // UInt8         blueBits;         // # of blue bits in each pixel
        // UInt8         reserved;         // must be zero
        // RGBColorType  transparentColor; // transparent color (index field ignored)
        // 0000: 00 1E 00 1E 00 3C 04 00 10 02 00 00 00 00 00 00
        // 0010: 05 06 05 00 00 00 00 00 FF FF FF FF FF FF FF FF

        // XXX ignoring redBits, greenBits and blueBits
        i += 4;

        debug(DEBUG_TRACE, "Bitmap", "reading transparency from BitmapDirectInfoType");
        i += get1(&dummy8, p, i);
        i += get1(&r, p, i);
        i += get1(&g, p, i);
        i += get1(&b, p, i);
        bmpV2->transparentValue = rgb565(r, g, b);
        transparentIndex = 0;
      }

      bmpV2->width = width;
      bmpV2->height = height;
      bmpV2->rowBytes = rowBytes;
      bmpV2->flags = bmpAttr;
      bmpV2->pixelSize = pixelSize;
      bmpV2->version = version;
      bmpV2->nextDepthOffset = nextDepthOffset;
      bmpV2->transparentIndex = transparentIndex;
      bmpV2->compressionType = compressionType;
      bmpV2->bitsSize = height * rowBytes;
      debug(DEBUG_TRACE, "Bitmap", "bitmap V2 size %dx%d, bpp %d, transparency %d, compression %d, rowBytes %d, nextDepthOffset %d", width, height, pixelSize, transparentIndex, compressionType, rowBytes, nextDepthOffset*4);
      if (bmpAttr.compressed) {
        if (compressionType == BitmapCompressionTypeScanLine) {
          i += 2; // skip compressedSize ?
          if ((dp = pumpkin_heap_alloc(rowBytes * height, "Bits")) != NULL) {
            if (decompress_bitmap_scanline(&p[i], dp, rowBytes, width, height) == 0) {
              bmpV2->flags.compressed = 0;
              bmpV2->bits = dp;
            }
          }
        } else if (compressionType == BitmapCompressionTypeRLE) {
          i += get2(&compressedSize, p, i);
          if ((dp = pumpkin_heap_alloc(rowBytes * height, "Bits")) != NULL) {
            if (decompress_bitmap_rle(&p[i], dp, rowBytes * height) == 0) {
              bmpV2->flags.compressed = 0;
              bmpV2->bits = dp;
            }
          }
          i += compressedSize;
        } else if (compressionType == BitmapCompressionTypePackBits) {
          i += 2; // skip compressed size. decompression stops when height*rowBytes is reached.
          dsize = rowBytes * height;
          if ((dp = pumpkin_heap_alloc(dsize, "Bits")) != NULL) {
            i += decompress_bitmap_packbits8(&p[i], dp, dsize);
            bmpV2->flags.compressed = 0;
            bmpV2->bits = dp;
          }
        } else {
          debug(DEBUG_ERROR, "Bitmap", "bitmap compression type %d not supported", compressionType);
        }
      } else {
        bmpV2->bits = pumpkin_heap_dup(&p[i], rowBytes * height, "Bits");
        i += rowBytes * height;
      }
      if (chain && nextDepthOffset) {
        i = nextDepthOffset*4;
        if (i < size-10) {
          bmpV2->next = pumpkin_create_bitmap(h, &p[i], size - i, type, 1, NULL);
        }
      }
      break;
    case 3:
      bmpV3 = (BitmapTypeV3 *)bmp;
      i += get1(&ssize, p, i);  // size of this structure in bytes
      i += get1(&pixelFormat, p, i);
      i += get1(&dummy8, p, i);
      i += get1(&compressionType, p, i);
      i += get2(&density, p, i);
      i += get4(&transparentValue, p, i);
      i += get4(&nextBitmapOffset, p, i);

      if (bmpAttr.hasColorTable) {
        // hasColorTable
        if (bmpAttr.indirectColorTable) {
          // XXX indirectColorTable (not handled)
          debug(DEBUG_INFO, "Bitmap", "ignoring indirect color table");
          i += get4(&dummy32, p, i);
        } else {
          i += get2(&dummy16, p, i);
          dummy16 &= 0xFF;
          debug(DEBUG_TRACE, "Bitmap", "reading ColorTableType with %d entries", dummy16);

          bmpV3->colorTable = pumpkin_heap_alloc(sizeof(ColorTableType) + dummy16 * sizeof(RGBColorType), "ColorTable");
          bmpV3->colorTable->numEntries = dummy16;
          for (j = 0; j < dummy16; j++) {
            i += get1(&dummy8, p, i);
            i += get1(&dummy8, p, i);
            bmpV3->colorTable->entry[j].r = dummy8;
            i += get1(&dummy8, p, i);
            bmpV3->colorTable->entry[j].g = dummy8;
            i += get1(&dummy8, p, i);
            bmpV3->colorTable->entry[j].b = dummy8;
          }
        }
      }

      bmpV3->width = width;
      bmpV3->height = height;
      bmpV3->rowBytes = rowBytes;
      bmpV3->flags = bmpAttr;
      bmpV3->pixelSize = pixelSize;
      bmpV3->version = version;
      bmpV3->size = ssize;
      bmpV3->pixelFormat = pixelFormat;
      bmpV3->compressionType = compressionType;
      bmpV3->density = density;
      bmpV3->transparentValue = transparentValue;
      bmpV3->nextBitmapOffset = nextBitmapOffset;
      bmpV3->bitsSize = height * rowBytes;
      debug(DEBUG_TRACE, "Bitmap", "bitmap V3 size %dx%d (%s), bpp %d, format %d, density %d, transparency 0x%08x, compression %d, nextBitmapOffset %d", width, height, le ? "LE" : "BE", pixelSize, pixelFormat, density, transparentValue, compressionType, nextBitmapOffset);

      if (pixelFormat == pixelFormatIndexed || pixelFormat == pixelFormat565) {
        if (bmpAttr.compressed) {
          if (compressionType == BitmapCompressionTypeScanLine) {
            i += 4; // skip compressedSize ?
            if ((dp = pumpkin_heap_alloc(rowBytes * height, "Bits")) != NULL) {
              if (decompress_bitmap_scanline(&p[i], dp, rowBytes, width, height) == 0) {
                bmpV3->flags.compressed = 0;
                bmpV3->bits = dp;
              }
            }
          } else {
            if (compressionType == BitmapCompressionTypeRLE) {
              i += get4(&compressedSize32, p, i);
              if ((dp = pumpkin_heap_alloc(rowBytes * height, "Bits")) != NULL) {
                if (decompress_bitmap_rle(&p[i], dp, rowBytes * height) == 0) {
                  bmpV3->flags.compressed = 0;
                  bmpV3->bits = dp;
                }
              }
              i += compressedSize32;
            } else if (compressionType == BitmapCompressionTypePackBits) {
              i += get4(&dummy32, p, i);
              dsize = rowBytes * height;
              if ((dp = pumpkin_heap_alloc(dsize, "Bits")) != NULL) {
                i += decompress_bitmap_packbits8(&p[i], dp, dsize);
                bmpV3->flags.compressed = 0;
                bmpV3->bits = dp;
              }
            } else {
              debug(DEBUG_ERROR, "Bitmap", "bitmap compression type %d not supported", compressionType);
            }
          }
        } else {
          bmpV3->bits = pumpkin_heap_dup(&p[i], rowBytes * height, "Bits");
          i += rowBytes * height;
        }

        if (chain && nextBitmapOffset) {
          i = nextBitmapOffset;
          if (i < size-10) {
            debug(DEBUG_TRACE, "Bitmap", "next bitmap at offset %d", i);
            bmpV3->next = pumpkin_create_bitmap(h, &p[i], size - i, type, 1, NULL);
          }
        }
      } else {
        debug(DEBUG_ERROR, "Bitmap", "bitmap pixel format %d not supported", pixelFormat);
      }
      break;
  }

  BmpFillData(bmp);
  debug(DEBUG_TRACE, "Bitmap", "pumpkin_create_bitmap end");

  return bmp;
}

void pumpkin_destroy_bitmap(void *p) {
  debug(DEBUG_TRACE, "Bitmap", "pumpkin_destroy_bitmap %p", p);
  BmpPrintChain((BitmapType *)p, 0, 0, "destroy");
  BmpDelete((BitmapType *)p);
}

void *pumpkin_encode_bitmap(void *p, uint32_t *size) {
  BitmapType *bmp, *next;
  BitmapTypeV0 *bmpV0;
  BitmapTypeV1 *bmpV1;
  BitmapTypeV2 *bmpV2;
  BitmapTypeV3 *bmpV3;
  uint8_t pixelSize, pixelFormat, *buf, *bits;
  uint16_t density;
  uint32_t transparentValue, iNextBitmapOffset, bsize, totalSize, bmpStart, i, r;

  debug(DEBUG_INFO, "Bitmap", "encode bitmap begin");
  totalSize = 65536;
  buf = xcalloc(1, totalSize);

  for (bmp = (BitmapType *)p, i = 0; bmp;) {
    bmpStart = i;

    switch (bmp->version) {
      case 0:
        bmpV0 = (BitmapTypeV0 *)bmp;
        pixelSize = 1;
        pixelFormat =  pixelFormatIndexed;
        density = kDensityLow;
        transparentValue = 0;
        next = bmpV0->next;
        break;
      case 1:
        bmpV1 = (BitmapTypeV1 *)bmp;
        pixelSize = bmpV1->pixelSize;
        pixelFormat =  pixelFormatIndexed;
        density = kDensityLow;
        transparentValue = 0;
        next = bmpV1->next;
        break;
      case 2:
        bmpV2 = (BitmapTypeV2 *)bmp;
        pixelSize = bmpV2->pixelSize;
        pixelFormat =  pixelFormatIndexed;
        density = kDensityLow;
        transparentValue = bmpV2->transparentIndex;
        next = bmpV2->next;
        break;
      case 3:
        bmpV3 = (BitmapTypeV3 *)bmp;
        pixelSize = bmpV3->pixelSize;
        pixelFormat =  bmpV3->pixelFormat;
        density = bmpV3->density;
        transparentValue = bmpV3->transparentValue;
        next = bmpV3->next;
        break;
    }

    debug(DEBUG_INFO, "Bitmap", "bitmap V%d %dx%d bpp %2d format %d density %3d transp 0x%04X offset 0x%08X",
      bmp->version, bmp->width, bmp->height, pixelSize, pixelFormat, density, transparentValue, i);

    i += put2b(bmp->width, buf, i);
    i += put2b(bmp->height, buf, i);
    i += put2b(bmp->rowBytes, buf, i);
    BmpEncodeFlags(bmp->flags, buf, i);
    i += put1(pixelSize, buf, i);
    i += put1(3, buf, i); // always version 3

    i += put1(24, buf, i);  // size of this structure in bytes
    i += put1(pixelFormat, buf, i);
    i += put1(0, buf, i); // not used
    i += put1(BitmapCompressionTypeNone, buf, i);
    i += put2b(density, buf, i);
    i += put4b(transparentValue, buf, i);
    iNextBitmapOffset = i;
    i += put4b(0, buf, i); // nextBitmapOffset (filled in later)

    bits = BmpGetBits(bmp);
    bsize = BmpBitsSize(bmp);
    if (i + bsize >= totalSize) {
      totalSize += bsize + 1024;
      buf = xrealloc(buf, totalSize);
    }
    xmemcpy(&buf[i], bits, bsize);
    i += bsize;
    r = i % 4;
    if (r) i += 4-r;

    if (next) {
      put4b(i - bmpStart, buf, iNextBitmapOffset);
    }
    bmp = next;
  }

  buf = xrealloc(buf, i);
  *size = i;
  debug(DEBUG_INFO, "Bitmap", "encode bitmap end size=%u", i);
  //debug_bytes(DEBUG_INFO, "Bitmap", buf, i);
  //xfree(buf);

  return buf;
}

const UInt8 *BmpGetGray(UInt8 depth) {
  switch (depth) {
    case 1: return gray1;
    case 2: return gray2;
    case 4: return gray4;
  }

  return NULL;
}
