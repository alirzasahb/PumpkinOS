#include <PalmOS.h>
#include <CPMLib.h>

#include "sys.h"
#include "pumpkin.h"
#include "HashPlugin.h"
#include "sha1.h"
#include "md5.h"
#include "debug.h"

#define pluginId 'intH'

static HashPluginType plugin;

typedef struct {
  UInt32 type;
  union {
    SHA1_CTX sha1;
    MD5Context md5;
  } ctx;
} internal_hash_ctx_t;

static void *hashInit(UInt32 type) {
  internal_hash_ctx_t *ctx = NULL;

  switch (type) {
    case apHashTypeUnspecified:
      // fall-through
    case apHashTypeSHA1:
      ctx = MemPtrNew(sizeof(internal_hash_ctx_t));
      ctx->type = type;
      SHA1Init(&ctx->ctx.sha1);
      break;
    case apHashTypeMD5:
      ctx = MemPtrNew(sizeof(internal_hash_ctx_t));
      ctx->type = type;
      md5Init(&ctx->ctx.md5);
      break;
  }

  return ctx;
}

static uint32_t hashSize(void *_ctx) {
  internal_hash_ctx_t *ctx = (internal_hash_ctx_t *)_ctx;
  UInt32 size = 0;

  if (ctx) {
    switch (ctx->type) {
      case apHashTypeSHA1: size = SHA1_HASH_LEN; break;
      case apHashTypeMD5:  size = MD5_HASH_LEN; break;
    }
  }

  return size;
}

static void hashUpdate(void *_ctx, UInt8 *input, UInt32 len) {
  internal_hash_ctx_t *ctx = (internal_hash_ctx_t *)_ctx;

  if (ctx && input) {
    switch (ctx->type) {
      case apHashTypeSHA1:
        SHA1Update(&ctx->ctx.sha1, input, len);
        break;
      case apHashTypeMD5:
        md5Update(&ctx->ctx.md5, input, len);
        break;
    }
  }
}

static void hashFinalize(void *_ctx, UInt8 *hash) {
  internal_hash_ctx_t *ctx = (internal_hash_ctx_t *)_ctx;

  if (ctx && hash) {
    switch (ctx->type) {
      case apHashTypeSHA1:
        SHA1Final(hash, &ctx->ctx.sha1);
        break;
      case apHashTypeMD5:
        md5Finalize(&ctx->ctx.md5);
        MemMove(hash, ctx->ctx.md5.digest, MD5_HASH_LEN);
        break;
    }
  }
}

static void hashFree(void *ctx) {
  if (ctx) {
    MemPtrFree(ctx);
  }
}

static void *PluginMain(void *p) {
  plugin.init = hashInit;
  plugin.size = hashSize;
  plugin.update = hashUpdate;
  plugin.finalize = hashFinalize;
  plugin.free = hashFree;

  return &plugin;
}

pluginMainF PluginInit(UInt32 *type, UInt32 *id) {
  *type = hashPluginType;
  *id   = pluginId;

  return PluginMain;
}
