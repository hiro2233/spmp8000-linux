## Hardware layout ##

According to the spec. the chip has two UART ports (UART\_BT, UART\_MISC)

Both seem to be used in the firmware, but getting the register map
is a bit tricky, as they are always referenced through a register mapping
struct.

## Register map ##

|Base address|Uart|
|:-----------|:---|
|0x92B0.4000 |UART\_BT|
|0x92B0.6000 |UART\_MISC|

These are the regs the firmware is using:
|Register offset|Function|
|:--------------|:-------|
|0x00           |RX / TX |
|0x04           |Bit 0 enables/disables interrupts|
|0x0C           |Bit 7: Divisor latch access, Bit 1,0: Set 8-bit mode|
|0x14           |if (reg & 0x1E) -> RX error, Bit 6: Bus empty|
|0x20           |Bit 4: RX status|
|0x24           |Used at init|

This corresponds nicely to a 16550-like UART and it also seems like that auto flow
control is being used.

TODO: Find an ARM implementation of a 16550 UART with AFC