MCU = msp430fr2675
CC = msp430-elf-gcc
OBJCOPY = msp430-elf-objcopy

TI_GCC=/opt/ti/msp430-gcc
CFLAGS = -mmcu=$(MCU) -Os -ffunction-sections -fdata-sections -Wall -I$(TI_GCC)/include
LDSDIR=/opt/ti/msp430-gcc/include/
LDFLAGS = -Wl,-gc-sections -L$(LDSDIR)

all: blink.hex
blink.elf: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
blink.hex: blink.elf
	$(OBJCOPY) -O ihex $< $@
clean:
	rm -f blink.elf blink.hex

flash:
	/opt/ti/MSPFlasher_1.3.20/MSP430Flasher -n MSP430FR2675 -w blink.hex -v -g -z [VCC]
