## Hardware layout ##

The chip has multiple banks of GPIOs:

| **GPIO bank** | **Alternate functions** |
|:--------------|:------------------------|
| XGPIO 1..11   | none                    |
| GPIO0 0..7    | UART, SPI, SD, Keymatrix |
| GPIO1 0..15   |LCD controller and I2S (probably not used) |
| GPIO1 18      | PWM output              |
| GPIO1 22..23  | UART                    |
| GPIO3 0..12   | UART, SD                |
| SAR\_GPIO 0..3 | touch controller ADC    |

## GPIO register usage ##

Base address is: 0x9000.A000
Each bank has a register offset of 0x4 (0x0, 0x4, 0x8, 0xC)

Register layout of Bank x:
|0x9000.A00x|PIN on/off state|
|:----------|:---------------|
|0x9000.A01x|Set DATA        |
|0x9000.A02x|Direction (also needs to be set in SCU, see below)|
|0x9000.A03x|Polarity        |
|0x9000.A05x|Enable IRQ      |
|0x9000.A06x|IRQ status (clear by writing 1|
|0x9000.A0Ax|Get DATA        |

SCU1 GPIO controls, each GPIO bank is offset by 0x10
|0x9000.5100|GPIO direction|
|:----------|:-------------|
|0x9000.5108|GPIO pullup enable|


## Device GPIO usage ##

|Device name, HW version|
|:----------------------|
| GPIO num              | Device usage          |