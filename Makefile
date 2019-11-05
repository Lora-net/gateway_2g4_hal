### Environment constants

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

.PHONY: all clean libtools libloragw packet_forwarder util_net_downlink util_chip_id util_boot

all: libtools libloragw packet_forwarder util_net_downlink util_chip_id util_boot

libtools:
	$(MAKE) all -e -C $@

libloragw: libtools
	$(MAKE) all -e -C $@

packet_forwarder: libloragw
	$(MAKE) all -e -C $@

util_net_downlink: libtools
	$(MAKE) all -e -C $@

util_chip_id: libloragw
	$(MAKE) all -e -C $@

util_boot: libloragw
	$(MAKE) all -e -C $@

clean:
	$(MAKE) clean -e -C libtools
	$(MAKE) clean -e -C libloragw
	$(MAKE) clean -e -C packet_forwarder
	$(MAKE) clean -e -C util_net_downlink
	$(MAKE) clean -e -C util_chip_id
	$(MAKE) clean -e -C util_boot

### EOF
