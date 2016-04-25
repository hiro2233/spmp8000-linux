# Introduction #

See http://pspkox.wikia.com/wiki/SPMP8000_PSPKOX_Wiki for background info.

This project aims to add Linux support for the SPMP8000 SoC.

# Hardware #

As one of the first steps the hardware needs to be documented.
The [short-form datasheet](http://alemaxx.al.funpic.de/spmp8000/SPMP8016A_ds.pdf) gives some background info on what is actually present
in the DoC.

## Peripherals ##

Based on original firmware files, the following are the peripheral base addresses:


|0x9000.0000|Timer A|
|:----------|:------|
|0x9000.1000|Timer WDT|
|0x9000.2000|I2C S0 |
|0x9000.5000|       |
|0x9000.5100|SCU1 (System Control Unit?)|
|0x9200.5000|       |
|0x9200.5100|SCU2   |
|0x9000.A000|[GPIO](GPIO.md)|
|0x9000.B000|Real Time Clock|
|0x9053.2000|CEVA engine|
|0x9001.0000|[Vectored IRQ controller 0](VIC.md)|
|0x9002.0000|[Vectored IRQ controller 1](VIC.md)|
|0x9200.2000|Rotator|
|0x9200.6000|2D engine|
|0x9200.7000|Scale engine|
|0x92B0.0000|AHB DMA|
|0x92B0.0000|SPI    |
|0x92B0.3000|I2C P0 |
|0x92B0.4000|UART0  |
|0x92B0.6000|UART2  |
|0x92B0.8000|SSP    |
|0x92B0.B000|SD card controller|
|0x9300.3000|CSI    |
|0x9300.6000|Usb Device controller|
|0x9300.7000|Clock controller|
|0x9301.0000|APB DMA|
|0x9301.1D00|I2S RX |
|0x9301.2000|I2S TX |
|0x9301.7000|I2S clock|
|0x9301.F000|Audio Codec|
|0x9300.8000|NAND controller|
|0x9300.A000|BCH ?  |