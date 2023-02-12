#define FONT_8_8_ZX81_WIDTH  8
#define FONT_8_8_ZX81_HEIGHT 8
#define FONT_8_8_ZX81_MIN    0
#define FONT_8_8_ZX81_MAX    63

static uint8_t font_8_8_zx81[(FONT_8_8_ZX81_MAX - FONT_8_8_ZX81_MIN + 1) * FONT_8_8_ZX81_WIDTH] = {
  // 0x00
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,

  // 0x01
  0x0F,
  0x0F,
  0x0F,
  0x0F,
  0x00,
  0x00,
  0x00,
  0x00,

  // 0x02
  0x00,
  0x00,
  0x00,
  0x00,
  0x0F,
  0x0F,
  0x0F,
  0x0F,

  // 0x03
  0x0F,
  0x0F,
  0x0F,
  0x0F,
  0x0F,
  0x0F,
  0x0F,
  0x0F,

  // 0x04
  0xF0,
  0xF0,
  0xF0,
  0xF0,
  0x00,
  0x00,
  0x00,
  0x00,

  // 0x05
  0xFF,
  0xFF,
  0xFF,
  0xFF,
  0x00,
  0x00,
  0x00,
  0x00,

  // 0x06
  0xF0,
  0xF0,
  0xF0,
  0xF0,
  0x0F,
  0x0F,
  0x0F,
  0x0F,

  // 0x07
  0xFF,
  0xFF,
  0xFF,
  0xFF,
  0x0F,
  0x0F,
  0x0F,
  0x0F,

  // 0x08
  0x55,
  0xAA,
  0x55,
  0xAA,
  0x55,
  0xAA,
  0x55,
  0xAA,

  // 0x09
  0x50,
  0xA0,
  0x50,
  0xA0,
  0x50,
  0xA0,
  0x50,
  0xA0,

  // 0x0A
  0x05,
  0x0A,
  0x05,
  0x0A,
  0x05,
  0x0A,
  0x05,
  0x0A,

  // 0x0B
  0x00,
  0x00,
  0x06,
  0x00,
  0x00,
  0x06,
  0x00,
  0x00,

  // 0x0C
  0x00,
  0x48,
  0x7C,
  0x4A,
  0x4A,
  0x42,
  0x44,
  0x00,

  // 0x0D
  0x00,
  0x00,
  0x5C,
  0x54,
  0xFE,
  0x54,
  0x74,
  0x00,

  // 0x0E
  0x00,
  0x00,
  0x00,
  0x48,
  0x00,
  0x00,
  0x00,
  0x00,

  // 0x0F
  0x00,
  0x04,
  0x02,
  0x02,
  0x52,
  0x0A,
  0x04,
  0x00,

  // 0x10
  0x00,
  0x00,
  0x00,
  0x00,
  0x3C,
  0x42,
  0x00,
  0x00,

  // 0x11
  0x00,
  0x00,
  0x42,
  0x3C,
  0x00,
  0x00,
  0x00,
  0x00,

  // 0x12
  0x00,
  0x00,
  0x00,
  0x44,
  0x28,
  0x10,
  0x00,
  0x00,

  // 0x13
  0x00,
  0x00,
  0x00,
  0x10,
  0x28,
  0x44,
  0x00,
  0x00,

  // 0x14
  0x00,
  0x00,
  0x28,
  0x28,
  0x28,
  0x28,
  0x28,
  0x00,

  // 0x15
  0x00,
  0x00,
  0x10,
  0x10,
  0x7C,
  0x10,
  0x10,
  0x00,

  // 0x16
  0x00,
  0x00,
  0x10,
  0x10,
  0x10,
  0x10,
  0x10,
  0x00,

  // 0x17
  0x00,
  0x00,
  0x10,
  0x54,
  0x38,
  0x54,
  0x10,
  0x00,

  // 0x18
  0x00,
  0x00,
  0x40,
  0x20,
  0x10,
  0x08,
  0x04,
  0x00,

  // 0x19
  0x00,
  0x00,
  0x80,
  0x64,
  0x00,
  0x00,
  0x00,
  0x00,

  // 0x1A
  0x00,
  0x00,
  0x00,
  0x80,
  0x60,
  0x00,
  0x00,
  0x00,

  // 0x1B
  0x00,
  0x00,
  0x00,
  0x60,
  0x60,
  0x00,
  0x00,
  0x00,

  // 0x1C
  0x00,
  0x3C,
  0x62,
  0x52,
  0x4A,
  0x46,
  0x3C,
  0x00,

  // 0x1D
  0x00,
  0x00,
  0x44,
  0x42,
  0x7E,
  0x40,
  0x40,
  0x00,

  // 0x1E
  0x00,
  0x64,
  0x52,
  0x52,
  0x52,
  0x52,
  0x4C,
  0x00,

  // 0x1F
  0x00,
  0x24,
  0x42,
  0x42,
  0x4A,
  0x4A,
  0x34,
  0x00,

  // 0x20
  0x00,
  0x30,
  0x28,
  0x24,
  0x7E,
  0x20,
  0x20,
  0x00,

  // 0x21
  0x00,
  0x2E,
  0x4A,
  0x4A,
  0x4A,
  0x4A,
  0x32,
  0x00,

  // 0x22
  0x00,
  0x3C,
  0x4A,
  0x4A,
  0x4A,
  0x4A,
  0x30,
  0x00,

  // 0x23
  0x00,
  0x02,
  0x02,
  0x62,
  0x12,
  0x0A,
  0x06,
  0x00,

  // 0x24
  0x00,
  0x34,
  0x4A,
  0x4A,
  0x4A,
  0x4A,
  0x34,
  0x00,

  // 0x25
  0x00,
  0x0C,
  0x52,
  0x52,
  0x52,
  0x52,
  0x3C,
  0x00,

  // 0x26
  0x00,
  0x7C,
  0x12,
  0x12,
  0x12,
  0x12,
  0x7C,
  0x00,

  // 0x27
  0x00,
  0x7E,
  0x4A,
  0x4A,
  0x4A,
  0x4A,
  0x34,
  0x00,

  // 0x28
  0x00,
  0x3C,
  0x42,
  0x42,
  0x42,
  0x42,
  0x24,
  0x00,

  // 0x29
  0x00,
  0x7E,
  0x42,
  0x42,
  0x42,
  0x24,
  0x18,
  0x00,

  // 0x2A
  0x00,
  0x7E,
  0x4A,
  0x4A,
  0x4A,
  0x4A,
  0x42,
  0x00,

  // 0x2B
  0x00,
  0x7E,
  0x0A,
  0x0A,
  0x0A,
  0x0A,
  0x02,
  0x00,

  // 0x2C
  0x00,
  0x3C,
  0x42,
  0x42,
  0x52,
  0x52,
  0x34,
  0x00,

  // 0x2D
  0x00,
  0x7E,
  0x08,
  0x08,
  0x08,
  0x08,
  0x7E,
  0x00,

  // 0x2E
  0x00,
  0x00,
  0x42,
  0x42,
  0x7E,
  0x42,
  0x42,
  0x00,

  // 0x2F
  0x00,
  0x30,
  0x40,
  0x40,
  0x40,
  0x40,
  0x3E,
  0x00,

  // 0x30
  0x00,
  0x7E,
  0x08,
  0x08,
  0x14,
  0x22,
  0x40,
  0x00,

  // 0x31
  0x00,
  0x7E,
  0x40,
  0x40,
  0x40,
  0x40,
  0x40,
  0x00,

  // 0x32
  0x00,
  0x7E,
  0x04,
  0x08,
  0x08,
  0x04,
  0x7E,
  0x00,

  // 0x33
  0x00,
  0x7E,
  0x04,
  0x08,
  0x10,
  0x20,
  0x7E,
  0x00,

  // 0x34
  0x00,
  0x3C,
  0x42,
  0x42,
  0x42,
  0x42,
  0x3C,
  0x00,

  // 0x35
  0x00,
  0x7E,
  0x12,
  0x12,
  0x12,
  0x12,
  0x0C,
  0x00,

  // 0x36
  0x00,
  0x3C,
  0x42,
  0x52,
  0x62,
  0x42,
  0x3C,
  0x00,

  // 0x37
  0x00,
  0x7E,
  0x12,
  0x12,
  0x12,
  0x32,
  0x4C,
  0x00,

  // 0x38
  0x00,
  0x24,
  0x4A,
  0x4A,
  0x4A,
  0x4A,
  0x30,
  0x00,

  // 0x39
  0x02,
  0x02,
  0x02,
  0x7E,
  0x02,
  0x02,
  0x02,
  0x00,

  // 0x3A
  0x00,
  0x3E,
  0x40,
  0x40,
  0x40,
  0x40,
  0x3E,
  0x00,

  // 0x3B
  0x00,
  0x1E,
  0x20,
  0x40,
  0x40,
  0x20,
  0x1E,
  0x00,

  // 0x3C
  0x00,
  0x3E,
  0x40,
  0x20,
  0x20,
  0x40,
  0x3E,
  0x00,

  // 0x3D
  0x00,
  0x42,
  0x24,
  0x18,
  0x18,
  0x24,
  0x42,
  0x00,

  // 0x3E
  0x02,
  0x04,
  0x08,
  0x70,
  0x08,
  0x04,
  0x02,
  0x00,

  // 0x3F
  0x00,
  0x42,
  0x62,
  0x52,
  0x4A,
  0x46,
  0x42,
  0x00,
};

static font_t font8x8zx81 = {
  FONT_8_8_ZX81_WIDTH, FONT_8_8_ZX81_HEIGHT,
  FONT_8_8_ZX81_MIN,   FONT_8_8_ZX81_MAX,
  NULL,
  &font_8_8_zx81[0]
};
