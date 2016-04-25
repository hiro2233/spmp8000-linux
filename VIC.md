# Register map #

Looking at the ISR routines, the following are the used addresses:

|0x9001.0000|VIC0 base|
|:----------|:--------|
|0x9002.0000|VIC1 base|

|0x900x.0000|IRQ status|
|:----------|:---------|
|0x900x.0010|IRQ enable / unmask|
|0x900x.0014|IRQ disable / mask|
|0x900x.0200+offset|Set IRQ level|
|0x900x.0F00|Read at beginning of main ISR, written at end|

This corresponds nicely to the [ARM PL192 VIC PrimeCell](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0273a/I1006637.html) registers, so it quite sure that is being used.

TODO: Prove by reading the PrimeCell ID registers at [0xFF0-0xFFC](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0273a/I1006637.html).

# IRQ source mapping #

For finding out the individual peripheral IRQ bits, one needs to
look at the irq attach routine parameters called from the init of the peripheral.