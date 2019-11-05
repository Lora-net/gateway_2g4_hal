### get external defined data

LIBLORAGW_VERSION := `cat ../VERSION`
include library.cfg

### constant symbols

ARCH ?=
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

CFLAGS := -g -O0 -Wall -Wextra -std=c99 -Iinc -I.

OBJDIR = obj
INCLUDES = $(wildcard inc/*.h)

### linking options

LIBS := -lloragw -lrt -lm

### general build targets

all: libloragw.a test_hal_tx test_hal_rx test_hal_cnt test_hal_toa test_hal_reg test_hal_reset

clean:
	rm -f libloragw.a
	rm -f test_hal_*
	rm -f $(OBJDIR)/*.o
	rm -f inc/config.h

### transpose library.cfg into a C header file : config.h

inc/config.h: ../VERSION library.cfg
	@echo "*** Checking libloragw library configuration ***"
	@rm -f $@
	#File initialization
	@echo "#ifndef _LORAGW_CONFIGURATION_H" >> $@
	@echo "#define _LORAGW_CONFIGURATION_H" >> $@
	# Release version
	@echo "Release version   : $(LIBLORAGW_VERSION)"
	@echo "	#define LIBLORAGW_VERSION	"\"$(LIBLORAGW_VERSION)\""" >> $@
	# Debug options
	@echo "	#define DEBUG_AUX	$(DEBUG_AUX)" >> $@
	@echo "	#define DEBUG_HAL	$(DEBUG_HAL)" >> $@
	@echo "	#define DEBUG_MCU	$(DEBUG_MCU)" >> $@
	# end of file
	@echo "#endif" >> $@
	@echo "*** Configuration seems ok ***"

### library module target

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) inc/config.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

### static library

libloragw.a: $(OBJDIR)/loragw_hal.o $(OBJDIR)/loragw_aux.o $(OBJDIR)/loragw_mcu.o
	$(AR) rcs $@ $^

### test programs

test_hal_tx: tst/test_hal_tx.c libloragw.a
	$(CC) $(CFLAGS) -L. $< -o $@ $(LIBS)

test_hal_rx: tst/test_hal_rx.c libloragw.a
	$(CC) $(CFLAGS) -L. $< -o $@ $(LIBS)

test_hal_cnt: tst/test_hal_cnt.c libloragw.a
	$(CC) $(CFLAGS) -L. $< -o $@ $(LIBS)

test_hal_toa: tst/test_hal_toa.c libloragw.a
	$(CC) $(CFLAGS) -L. $< -o $@ $(LIBS)

test_hal_reg: tst/test_hal_reg.c libloragw.a
	$(CC) $(CFLAGS) -L. $< -o $@ $(LIBS)

test_hal_reset: tst/test_hal_reset.c libloragw.a
	$(CC) $(CFLAGS) -L. $< -o $@ $(LIBS)

### EOF
