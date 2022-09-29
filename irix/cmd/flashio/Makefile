include	$(ROOT)/usr/include/make/commondefs

HFILES= fprom_sn0.h
CFILES= flashio.c flashio_everest.c flashio_sn0.c fprom_sn0.c \
	flashio_racer.c

TARGETS=flashio 

default: $(TARGETS)

include $(COMMONRULES)

install: default
	ln -f -s flashio flash
	$(INSTALL) -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" \
		   -idb 'exitop("$$rbase/usr/sbin/flashio $$rbase/usr/cpu/firmware/io4prom.bin")' -F /usr/sbin flashio
	$(INSTALL) -idb "mach(CPUBOARD=IP30)" -F /usr/sbin flash
	$(INSTALL) -idb "mach(CPUBOARD=IP27)" \
                   -idb 'exitop("$$rbase/usr/sbin/flash -p $$rbase/usr/cpu/firmware")' -F /usr/sbin flash
	$(INSTALL) -idb "mach(CPUBOARD=IP27XXL)" \
		-idb 'exitop("$$rbase/usr/sbin/flash -p $$rbase/usr/cpu/firmware")' -F /usr/sbin flash


flashio: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)
