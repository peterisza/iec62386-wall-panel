TI_CGT  := /opt/ti/ccs2031/ccs/tools/compiler/ti-cgt-msp430_21.6.1.LTS
CC      := $(TI_CGT)/bin/cl430
HEX     := $(TI_CGT)/bin/hex430

TI_INC      := $(TI_CGT)/include
MSP430_INC  := /opt/ti/ccs2031/ccs/ccs_base/msp430/include
CMD         := $(MSP430_INC)/lnk_msp430fr2675.cmd


CFLAGS := --silicon_version=mspx --code_model=large --data_model=large --abi=eabi \
          -O2 -g --display_error_number --diag_warning=225 \
          --define=__MSP430FR2675__ \
          --include_path=$(TI_INC) --include_path=$(MSP430_INC) \
          --include_path=./driverlib

# Large code + small data + EABI lib útvonalak
ILIBS  := -i"$(TI_CGT)/lib"

LIBS := lib/captivate_fr2676_family.lib lib/driverlib_fr2676_family.lib

# >>> LINKER opció: BELÉPÉSI PONT (csak itt, a -z után fog hatni!)
LDFLAGS := -e _c_int00

TARGET := blink
OBJS   := main.obj CAPT_UserConfig.obj CAPT_ISR.obj CAPT_App.obj CAPT_Manager.obj

all: $(TARGET).txt

%.obj: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).out: $(OBJS) $(CMD)
	# FONTOS: LDFLAGS a -z UTÁN legyen
	$(CC) $(CFLAGS) -z $(OBJS) $(LDFLAGS) -o $@ -m $(TARGET).map $(ILIBS) $(LIBS) $(CMD)

$(TARGET).txt: $(TARGET).out
	$(HEX) --ti_txt -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET).out $(TARGET).map $(TARGET).txt

flash:
	/opt/ti/MSPFlasher_1.3.20/MSP430Flasher -n MSP430FR2675 -w $(TARGET).txt -v -g -z [VCC]