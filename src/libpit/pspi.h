#ifndef PIT_SPI_H
#define PIT_SPI_H

typedef struct spi_t spi_t;

#define SPI_PROVIDER "spi_provider"

typedef struct {
  spi_t *(*open)(int cs, int speed, void *data);
  int (*close)(spi_t *spi);
  int (*transfer)(spi_t *spi, uint8_t *txbuf, uint8_t *rxbuf, int len);
  void *data;
} spi_provider_t;

spi_t *spi_open(int cs, int speed, void *data);
int spi_close(spi_t *spi);
int spi_transfer(spi_t *spi, uint8_t *txbuf, uint8_t *rxbuf, int len);
int spi_begin(void);
int spi_end(void);

#endif
