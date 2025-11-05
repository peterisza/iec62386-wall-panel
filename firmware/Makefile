TI_CGT  := /opt/ti/ccs2031/ccs/tools/compiler/ti-cgt-msp430_21.6.1.LTS
CC      := $(TI_CGT)/bin/cl430
HEX     := $(TI_CGT)/bin/hex430

TI_INC      := $(TI_CGT)/include
MSP430_INC  := /opt/ti/ccs2031/ccs/ccs_base/msp430/include
CMD         := $(MSP430_INC)/lnk_msp430fr2675.cmd


CFLAGS := --silicon_version=mspx --code_model=large --data_model=large --abi=eabi \
          --c99 \
          -O2 -g --display_error_number --diag_warning=225 \
          --define=__MSP430FR2675__ \
          --include_path=$(TI_INC) --include_path=$(MSP430_INC)

# Large code + small data + EABI lib útvonalak
ILIBS  := -i"$(TI_CGT)/lib"

LIBS := ti_captivate_lib/captivate_fr2676_family.lib ti_captivate_lib/driverlib_fr2676_family.lib

# >>> LINKER opció: BELÉPÉSI PONT (csak itt, a -z után fog hatni!)
LDFLAGS := -e _c_int00

TARGET := firmware
OBJS   := binary/main.obj \
          binary/CAPT_UserConfig.obj \
          binary/CAPT_ISR.obj \
          binary/CAPT_App.obj \
          binary/CAPT_Manager.obj \
          binary/buzzer.obj \
          binary/dali.obj \
          binary/i2c.obj \
          binary/touch.obj \
          binary/uart.obj \
		  binary/watchdog.obj \
		  binary/gpio.obj \
          binary/util.obj \
          binary/ens210_temperature_sensor.obj \
          binary/lps22hh_barometer.obj \
          binary/stcc4_co2_sensor.obj \
          binary/scheduler.obj

all: binary/$(TARGET).txt

# Relatív include path-ok (binary mappából nézve)
REL_INC := --include_path=../driverlib --include_path=../source --include_path=../ti_captivate_lib

# Objektum fájlok a gyökérből
binary/%.obj: %.c | binary
	cd binary && $(CC) $(CFLAGS) $(REL_INC) -c ../$< -o $(notdir $@)

# Objektum fájlok a source/ mappából
binary/%.obj: source/%.c | binary
	cd binary && $(CC) $(CFLAGS) $(REL_INC) -c ../$< -o $(notdir $@)

# Objektum fájlok a ti_captivate_lib/ mappából
binary/%.obj: ti_captivate_lib/%.c | binary
	cd binary && $(CC) $(CFLAGS) $(REL_INC) -c ../$< -o $(notdir $@)

binary/$(TARGET).out: $(OBJS) $(CMD) | binary
	# FONTOS: LDFLAGS a -z UTÁN legyen
	$(CC) $(CFLAGS) -z $(OBJS) $(LDFLAGS) -o $@ -m binary/$(TARGET).map $(ILIBS) $(LIBS) $(CMD)

binary/$(TARGET).txt: binary/$(TARGET).out
	$(HEX) --ti_txt -o $@ $<

binary:
	mkdir -p binary

clean:
	rm -f $(OBJS) binary/$(TARGET).out binary/$(TARGET).map binary/$(TARGET).txt

flash: binary/$(TARGET).txt
	/opt/ti/MSPFlasher_1.3.20/MSP430Flasher -n MSP430FR2675 -w binary/$(TARGET).txt -v -g -z [VCC]
	@echo "=========================================="
	@echo "Memory Statistics:"
	@echo "=========================================="
	@if [ -f binary/$(TARGET).map ]; then \
		RAM_USED=$$(grep -E "^  RAM" binary/$(TARGET).map | awk '{print $$4}'); \
		RAM_TOTAL=$$(grep -E "^  RAM" binary/$(TARGET).map | awk '{print $$3}'); \
		FRAM_USED=$$(grep -E "^  FRAM" binary/$(TARGET).map | awk '{print $$4}'); \
		FRAM_TOTAL=$$(grep -E "^  FRAM" binary/$(TARGET).map | awk '{print $$3}'); \
		if [ -n "$$RAM_USED" ] && [ -n "$$RAM_TOTAL" ]; then \
			RAM_USED_DEC=$$(printf "%d" 0x$$RAM_USED 2>/dev/null || echo "0"); \
			RAM_TOTAL_DEC=$$(printf "%d" 0x$$RAM_TOTAL 2>/dev/null || echo "1"); \
			RAM_PERCENT=$$(awk "BEGIN {printf \"%.1f\", ($$RAM_USED_DEC / $$RAM_TOTAL_DEC) * 100}"); \
			echo "RAM:  $$RAM_USED_DEC / $$RAM_TOTAL_DEC bytes ($$RAM_PERCENT%)"; \
		fi; \
		if [ -n "$$FRAM_USED" ] && [ -n "$$FRAM_TOTAL" ]; then \
			FRAM_USED_DEC=$$(printf "%d" 0x$$FRAM_USED 2>/dev/null || echo "0"); \
			FRAM_TOTAL_DEC=$$(printf "%d" 0x$$FRAM_TOTAL 2>/dev/null || echo "1"); \
			FRAM_PERCENT=$$(awk "BEGIN {printf \"%.1f\", ($$FRAM_USED_DEC / $$FRAM_TOTAL_DEC) * 100}"); \
			echo "FRAM: $$FRAM_USED_DEC / $$FRAM_TOTAL_DEC bytes ($$FRAM_PERCENT%)"; \
		fi; \
		echo "=========================================="; \
	fi