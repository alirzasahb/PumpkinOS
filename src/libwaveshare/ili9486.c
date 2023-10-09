#include "sys.h"
#include "script.h"
#include "gpio.h"
#include "pspi.h"
#include "bytes.h"
#include "ili9486.h"
#include "debug.h"

static uint32_t init_seq[] = {
  0x010000b0, 0x00, // interface Mode Control: SDA_EN = 0, DIN and DOUT pins are used for 3/4 wire serial interface
  0x01000011,       // sleep OUT
  0x020000ff,       // delay ???
  0x0100003a, 0x55, // interface Pixel Format: RGB Interface Format = CPU Interface Format = 16 bits/pixel
  0x010000c2, 0x44, // power Control 3 (For Normal Mode)
  0x010000c5, 0x00, 0x00, 0x00, 0x00, // VCOM Control: NV memory is not programmed VCOM = -2 VCOM value from NV memory NV memory programmed value = 0
  0x010000e0, 0x0f, 0x1f, 0x1c, 0x0c, 0x0f, 0x08, 0x48, 0x98, 0x37, 0x0a, 0x13, 0x04, 0x11, 0x0d, 0x00,  // PGAMCTRL(Positive Gamma Control)
  0x010000e1, 0x0f, 0x32, 0x2e, 0x0b, 0x0d, 0x05, 0x47, 0x75, 0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,  // NGAMCTRL (Negative Gamma Correction)
  0x010000e2, 0x0f, 0x32, 0x2e, 0x0b, 0x0d, 0x05, 0x47, 0x75, 0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,  // Digital Gamma Control 1
  0x01000011,       // sleep OUT
  0x01000029,       // display ON
  0x01000036, 0x88, // memory Access Control: BGR color filter panel
  0xFFFFFFFF
};

/*
20MHz:
ILI9486: draw 320x320 @ 0,0: 173430 us

20MHz, just write:
ILI9486: draw 320x320 @ 0,0: 90233 us

32MHz:
ILI9486: draw 320x320 @ 0,0: 107872 us

32MHz, just write:
ILI9486: draw 320x320 @ 0,0: 54730 us
*/

static int ili9486_word(spi_provider_t *spip, spi_t *spi, uint16_t w) {
  uint8_t b[2], r[2];

  b[0] = w >> 8;
  b[1] = w & 0xFF;
  debug(DEBUG_TRACE, "ILI9486", "word 0x%04X", w);

  return spip->transfer(spi, b, r, 2) == 2 ? 0 : -1;
}

static int ili9486_command(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, uint8_t b) {
  gpiop->output(gpio, dc_pin, 0);

  return ili9486_word(spip, spi, b);
}

static int ili9486_param(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, uint8_t b) {
  gpiop->output(gpio, dc_pin, 1);

  return ili9486_word(spip, spi, b);
}

// data in d must be big endian
static int ili9486_data(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, uint16_t *d, int n, uint16_t *aux) {
  gpiop->output(gpio, dc_pin, 1);
  n *= 2;
  debug(DEBUG_TRACE, "ILI9486", "data %d bytes", n);
  //debug_bytes(DEBUG_TRACE, "ILI9486", (uint8_t *)d, n);

  return spip->transfer(spi, (uint8_t *)d, (uint8_t *)aux, n) == n ? 0 : -1;
}

int ili9486_begin(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, int rst_pin) {
  uint8_t b;
  int i;

  gpiop->setup(gpio, dc_pin, GPIO_OUT);
  gpiop->setup(gpio, rst_pin, GPIO_OUT);

  gpiop->output(gpio, rst_pin, 1);
  sys_usleep(5000);
  gpiop->output(gpio, rst_pin, 0);
  sys_usleep(20000);
  gpiop->output(gpio, rst_pin, 1);
  sys_usleep(65000);

  debug(DEBUG_TRACE, "ILI9486", "begin initialization");
  for (i = 0;; i++) {
    if (init_seq[i] == 0xFFFFFFFF) break;
    b = init_seq[i] & 0xFF;

    if (init_seq[i] & 0x01000000) {
      debug(DEBUG_TRACE, "ILI9486", "command 0x%02X", b);
      if (ili9486_command(spip, spi, gpiop, gpio, dc_pin, b) == -1) return -1;
    } else if (init_seq[i] & 0x02000000) {
      debug(DEBUG_TRACE, "ILI9486", "sleep %d", b * 1000);
      sys_usleep(b * 1000);
    } else {
      debug(DEBUG_TRACE, "ILI9486", "param 0x%02X", b);
      if (ili9486_param(spip, spi, gpiop, gpio, dc_pin, b) == -1) return -1;
    }
  }
  debug(DEBUG_TRACE, "ILI9486", "end initialization");

  return 0;
}

int ili9486_enable(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, int enable) {
  if (enable) {
    ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x11);
    ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x29);
  } else {
    ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x28);
    ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x10);
  }

  return 0;
}

static int ili9486_area(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  uint8_t data[8], aux[8];

  debug(DEBUG_TRACE, "ILI9486", "area %d,%d %d,%d", x1, y1, x2, y2);
  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x2A);
  data[0] = 0;
  data[1] = x1 >> 8;
  data[2] = 0;
  data[3] = x1 & 0xFF;
  data[4] = 0;
  data[5] = x2 >> 8;
  data[6] = 0;
  data[7] = x2 & 0xFF;
  ili9486_data(spip, spi, gpiop, gpio, dc_pin, (uint16_t *)data, 4, (uint16_t *)aux);

  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x2B);
  data[0] = 0;
  data[1] = y1 >> 8;
  data[2] = 0;
  data[3] = y1 & 0xFF;
  data[4] = 0;
  data[5] = y2 >> 8;
  data[6] = 0;
  data[7] = y2 & 0xFF;
  ili9486_data(spip, spi, gpiop, gpio, dc_pin, (uint16_t *)data, 4, (uint16_t *)aux);

  return 0;
}

/*
static int ili9486_char(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, int x, int y, uint8_t c, int i, font_t *f, uint16_t fg, uint16_t bg) {
  uint16_t data[256];
  uint8_t *d, b;
  int j, k;

  d = (i ? f->font1 : f->font0) + (c - f->min) * f->width;
  for (j = 0, k = 0; j < f->width && k < 256-8; j++) {
    b = d[j];
    put2b((b & 0x01) ? fg : bg, (uint8_t *)&data[k++], 0);
    put2b((b & 0x02) ? fg : bg, (uint8_t *)&data[k++], 0);
    put2b((b & 0x04) ? fg : bg, (uint8_t *)&data[k++], 0);
    put2b((b & 0x08) ? fg : bg, (uint8_t *)&data[k++], 0);
    put2b((b & 0x10) ? fg : bg, (uint8_t *)&data[k++], 0);
    put2b((b & 0x20) ? fg : bg, (uint8_t *)&data[k++], 0);
    put2b((b & 0x40) ? fg : bg, (uint8_t *)&data[k++], 0);
    put2b((b & 0x80) ? fg : bg, (uint8_t *)&data[k++], 0);
  }

  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x36);
  ili9486_param(spip, spi, gpiop, gpio, dc_pin, 0xA8); // invert row/col

  ili9486_area(spip, spi, gpiop, gpio, dc_pin, y, x, y+7, x+f->width);
  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x2C);
  ili9486_data(spip, spi, gpiop, gpio, dc_pin, data, k, data);

  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x36);
  ili9486_param(spip, spi, gpiop, gpio, dc_pin, 0x88);

  return 0;
}

int ili9486_printchar(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, int x, int y, uint8_t c, font_t *f, uint16_t fg, uint16_t bg) {
  if (f->height == 8) {
    return ili9486_char(spip, spi, gpiop, gpio, dc_pin, x, y, c, 1, f, fg, bg);
  }

  if (ili9486_char(spip, spi, gpiop, gpio, dc_pin, x, y,   c, 0, f, fg, bg) != 0) return -1;
  if (ili9486_char(spip, spi, gpiop, gpio, dc_pin, x, y+8, c, 1, f, fg, bg) != 0) return -1;

  return 0;
}
*/

int ili9486_setpixel(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, uint16_t bg, int x, int y) {
  uint16_t aux;

  put2b(bg, (uint8_t *)&aux, 0);
  ili9486_area(spip, spi, gpiop, gpio, dc_pin, x, y, x, y);
  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x2C);
  ili9486_data(spip, spi, gpiop, gpio, dc_pin, &aux, 1, &aux);

  return 0;
}

int ili9486_cls(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, uint16_t bg, int x, int y, int width, int height, uint16_t *aux) {
  int i, n;

  ili9486_area(spip, spi, gpiop, gpio, dc_pin, x, y, x+width-1, y+height-1);
  n = width * height;

  for (i = 0; i < n; i++) {
    put2b(bg, (uint8_t *)&aux[i], 0);
  }

  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x2C);
  ili9486_data(spip, spi, gpiop, gpio, dc_pin, aux, n, aux);
  ili9486_area(spip, spi, gpiop, gpio, dc_pin, x, y, x+width-1, y+height-1);

  return 0;
}

int ili9486_draw(spi_provider_t *spip, spi_t *spi, gpio_provider_t *gpiop, gpio_t *gpio, int dc_pin, uint16_t *pic, int x, int y, int width, int height, uint16_t *aux) {
  int i, n;
  int64_t t;

  t = sys_get_clock();
  ili9486_area(spip, spi, gpiop, gpio, dc_pin, x, y, x+width-1, y+height-1);
  n = width * height;

  for (i = 0; i < n; i++) {
    put2b(pic[i], (uint8_t *)&aux[i], 0);
  }

  ili9486_command(spip, spi, gpiop, gpio, dc_pin, 0x2C);
  ili9486_data(spip, spi, gpiop, gpio, dc_pin, aux, n, aux);

  t = sys_get_clock() - t;
  debug(DEBUG_TRACE, "ILI9486", "draw %dx%d @ %d,%d: %ld us", width, height, x, y, t);

  return 0;
}

// A: blue,  5 bits
// B: green, 6 bits
// C: red,   5 bits
// FEDCBA9876543210
// CCCCCBBBBBBAAAAA

uint16_t ili9486_color565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t c;

  c = r >> 3;
  c <<= 11; // XXX c <<= 11 ???
  c |= g >> 2;
  c <<= 5;
  c |= b >> 3;

  return c;
}

void ili9486_rgb565(uint32_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = ((c >> 8) & 0xF8) | 0x07;
  *g = ((c >> 3) & 0xFC) | 0x03;
  *b = ((c << 3) & 0xF8) | 0x07;
}
